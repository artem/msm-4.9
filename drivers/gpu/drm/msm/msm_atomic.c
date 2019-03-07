/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2014 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/msm_drm_notify.h>
#include <linux/notifier.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "msm_gem.h"
#include "msm_fence.h"
#include "sde_trace.h"
#ifdef CONFIG_SHARP_DISPLAY /* CUST_ID_00007 */
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <video/mipi_display.h>
#include "sde/sde_encoder_phys.h"
#include "sharp/drm_cmn.h"
#include "sharp/drm_det.h"
#endif /* CONFIG_SHARP_DISPLAY */
#ifdef CONFIG_SHARP_DRM_HR_VID /* CUST_ID_00015 */
#include "sharp/drm_mfr.h"
#endif /* CONFIG_SHARP_DRM_HR_VID */

#define MULTIPLE_CONN_DETECTED(x) (x > 1)

struct msm_commit {
	struct drm_device *dev;
	struct drm_atomic_state *state;
	uint32_t crtc_mask;
	bool nonblock;
	struct kthread_work commit_work;
};

static BLOCKING_NOTIFIER_HEAD(msm_drm_notifier_list);

#ifdef CONFIG_SHARP_DISPLAY /* CUST_ID_00007 */
static int msm_atomic_update_mipiclk(struct drm_atomic_state *state);
#ifdef CONFIG_SHARP_DRM_HR_VID /* CUST_ID_00015 */
static int mipiclkcng_cnt = 3;
module_param(mipiclkcng_cnt, int, 0600);
#endif /* CONFIG_SHARP_DRM_HR_VID */
#endif /* CONFIG_SHARP_DISPLAY */

/**
 * msm_drm_register_client - register a client notifier
 * @nb: notifier block to callback on events
 *
 * This function registers a notifier callback function
 * to msm_drm_notifier_list, which would be called when
 * received unblank/power down event.
 */
int msm_drm_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&msm_drm_notifier_list,
						nb);
}
#ifdef CONFIG_SHARP_DISPLAY /* CUST_ID_00020 */
EXPORT_SYMBOL(msm_drm_register_client);
#endif /* CONFIG_SHARP_DISPLAY */

/**
 * msm_drm_unregister_client - unregister a client notifier
 * @nb: notifier block to callback on events
 *
 * This function unregisters the callback function from
 * msm_drm_notifier_list.
 */
int msm_drm_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&msm_drm_notifier_list,
						  nb);
}
#ifdef CONFIG_SHARP_DISPLAY /* CUST_ID_00020 */
EXPORT_SYMBOL(msm_drm_unregister_client);
#endif /* CONFIG_SHARP_DISPLAY */

/**
 * msm_drm_notifier_call_chain - notify clients of drm_events
 * @val: event MSM_DRM_EARLY_EVENT_BLANK or MSM_DRM_EVENT_BLANK
 * @v: notifier data, inculde display id and display blank
 *     event(unblank or power down).
 */
static int msm_drm_notifier_call_chain(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&msm_drm_notifier_list, val,
					    v);
}

/* block until specified crtcs are no longer pending update, and
 * atomically mark them as pending update
 */
static int start_atomic(struct msm_drm_private *priv, uint32_t crtc_mask)
{
	int ret;

	spin_lock(&priv->pending_crtcs_event.lock);
	ret = wait_event_interruptible_locked(priv->pending_crtcs_event,
			!(priv->pending_crtcs & crtc_mask));
	if (ret == 0) {
		DBG("start: %08x", crtc_mask);
		priv->pending_crtcs |= crtc_mask;
	}
	spin_unlock(&priv->pending_crtcs_event.lock);

	return ret;
}

/* clear specified crtcs (no longer pending update)
 */
static void end_atomic(struct msm_drm_private *priv, uint32_t crtc_mask)
{
	spin_lock(&priv->pending_crtcs_event.lock);
	DBG("end: %08x", crtc_mask);
	priv->pending_crtcs &= ~crtc_mask;
	wake_up_all_locked(&priv->pending_crtcs_event);
	spin_unlock(&priv->pending_crtcs_event.lock);
}

static void commit_destroy(struct msm_commit *c)
{
	end_atomic(c->dev->dev_private, c->crtc_mask);
	if (c->nonblock)
		kfree(c);
}

static inline bool _msm_seamless_for_crtc(struct drm_atomic_state *state,
			struct drm_crtc_state *crtc_state, bool enable)
{
	struct drm_connector *connector = NULL;
	struct drm_connector_state  *conn_state = NULL;
	int i = 0;
	int conn_cnt = 0;

	if (msm_is_mode_seamless(&crtc_state->mode) ||
		msm_is_mode_seamless_vrr(&crtc_state->adjusted_mode))
		return true;

	if (msm_is_mode_seamless_dms(&crtc_state->adjusted_mode) && !enable)
		return true;

	if (!crtc_state->mode_changed && crtc_state->connectors_changed) {
		for_each_connector_in_state(state, connector, conn_state, i) {
			if ((conn_state->crtc == crtc_state->crtc) ||
					(connector->state->crtc ==
					 crtc_state->crtc))
				conn_cnt++;

			if (MULTIPLE_CONN_DETECTED(conn_cnt))
				return true;
		}
	}

	return false;
}

static inline bool _msm_seamless_for_conn(struct drm_connector *connector,
		struct drm_connector_state *old_conn_state, bool enable)
{
	if (!old_conn_state || !old_conn_state->crtc)
		return false;

	if (!old_conn_state->crtc->state->mode_changed &&
			!old_conn_state->crtc->state->active_changed &&
			old_conn_state->crtc->state->connectors_changed) {
		if (old_conn_state->crtc == connector->state->crtc)
			return true;
	}

	if (enable)
		return false;

	if (msm_is_mode_seamless(&connector->encoder->crtc->state->mode))
		return true;

	if (msm_is_mode_seamless_vrr(
			&connector->encoder->crtc->state->adjusted_mode))
		return true;

	if (msm_is_mode_seamless_dms(
			&connector->encoder->crtc->state->adjusted_mode))
		return true;

	return false;
}

static void msm_atomic_wait_for_commit_done(
		struct drm_device *dev,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct msm_drm_private *priv = old_state->dev->dev_private;
	struct msm_kms *kms = priv->kms;
	int i;

	for_each_crtc_in_state(old_state, crtc, crtc_state, i) {
		if (!crtc->state->enable)
			continue;

		/* Legacy cursor ioctls are completely unsynced, and userspace
		 * relies on that (by doing tons of cursor updates). */
		if (old_state->legacy_cursor_update)
			continue;

		kms->funcs->wait_for_crtc_commit_done(kms, crtc);
	}
}

static void
msm_disable_outputs(struct drm_device *dev, struct drm_atomic_state *old_state)
{
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct msm_drm_notifier notifier_data;
	int i, blank;

	SDE_ATRACE_BEGIN("msm_disable");
	for_each_connector_in_state(old_state, connector, old_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;
		struct drm_crtc_state *old_crtc_state;
		unsigned int crtc_idx;

		/*
		 * Shut down everything that's in the changeset and currently
		 * still on. So need to check the old, saved state.
		 */
		if (!old_conn_state->crtc)
			continue;

		crtc_idx = drm_crtc_index(old_conn_state->crtc);
		old_crtc_state = drm_atomic_get_existing_crtc_state(old_state,
						    old_conn_state->crtc);

		if (!old_crtc_state->active ||
		    !drm_atomic_crtc_needs_modeset(old_conn_state->crtc->state))
			continue;

		encoder = old_conn_state->best_encoder;

		/* We shouldn't get this far if we didn't previously have
		 * an encoder.. but WARN_ON() rather than explode.
		 */
		if (WARN_ON(!encoder))
			continue;

		if (_msm_seamless_for_conn(connector, old_conn_state, false))
			continue;

		funcs = encoder->helper_private;

		DRM_DEBUG_ATOMIC("disabling [ENCODER:%d:%s]\n",
				 encoder->base.id, encoder->name);

		blank = MSM_DRM_BLANK_POWERDOWN;
		notifier_data.data = &blank;
		notifier_data.id = crtc_idx;
		msm_drm_notifier_call_chain(MSM_DRM_EARLY_EVENT_BLANK,
					     &notifier_data);
		/*
		 * Each encoder has at most one connector (since we always steal
		 * it away), so we won't call disable hooks twice.
		 */
		drm_bridge_disable(encoder->bridge);

		/* Right function depends upon target state. */
		if (connector->state->crtc && funcs->prepare)
			funcs->prepare(encoder);
		else if (funcs->disable)
			funcs->disable(encoder);
		else
			funcs->dpms(encoder, DRM_MODE_DPMS_OFF);

		drm_bridge_post_disable(encoder->bridge);
		msm_drm_notifier_call_chain(MSM_DRM_EVENT_BLANK,
					    &notifier_data);
	}

	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		/* Shut down everything that needs a full modeset. */
		if (!drm_atomic_crtc_needs_modeset(crtc->state))
			continue;

		if (!old_crtc_state->active)
			continue;

		if (_msm_seamless_for_crtc(old_state, crtc->state, false))
			continue;

		funcs = crtc->helper_private;

		DRM_DEBUG_ATOMIC("disabling [CRTC:%d]\n",
				 crtc->base.id);

		/* Right function depends upon target state. */
		if (crtc->state->enable && funcs->prepare)
			funcs->prepare(crtc);
		else if (funcs->disable)
			funcs->disable(crtc);
		else
			funcs->dpms(crtc, DRM_MODE_DPMS_OFF);
	}
	SDE_ATRACE_END("msm_disable");
}

static void
msm_crtc_set_mode(struct drm_device *dev, struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	int i;

	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		if (!crtc->state->mode_changed)
			continue;

		funcs = crtc->helper_private;

		if (crtc->state->enable && funcs->mode_set_nofb) {
			DRM_DEBUG_ATOMIC("modeset on [CRTC:%d]\n",
					 crtc->base.id);

			funcs->mode_set_nofb(crtc);
		}
	}

	for_each_connector_in_state(old_state, connector, old_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_crtc_state *new_crtc_state;
		struct drm_encoder *encoder;
		struct drm_display_mode *mode, *adjusted_mode;

		if (!connector->state->best_encoder)
			continue;

		encoder = connector->state->best_encoder;
		funcs = encoder->helper_private;
		new_crtc_state = connector->state->crtc->state;
		mode = &new_crtc_state->mode;
		adjusted_mode = &new_crtc_state->adjusted_mode;

		if (!new_crtc_state->mode_changed &&
				new_crtc_state->connectors_changed) {
			if (_msm_seamless_for_conn(connector,
					old_conn_state, false))
				continue;
		} else if (!new_crtc_state->mode_changed) {
			continue;
		}

		DRM_DEBUG_ATOMIC("modeset on [ENCODER:%d:%s]\n",
				 encoder->base.id, encoder->name);

		/*
		 * Each encoder has at most one connector (since we always steal
		 * it away), so we won't call mode_set hooks twice.
		 */
		if (funcs->mode_set)
			funcs->mode_set(encoder, mode, adjusted_mode);

		drm_bridge_mode_set(encoder->bridge, mode, adjusted_mode);
	}
}

/**
 * msm_atomic_helper_commit_modeset_disables - modeset commit to disable outputs
 * @dev: DRM device
 * @old_state: atomic state object with old state structures
 *
 * This function shuts down all the outputs that need to be shut down and
 * prepares them (if required) with the new mode.
 *
 * For compatibility with legacy crtc helpers this should be called before
 * drm_atomic_helper_commit_planes(), which is what the default commit function
 * does. But drivers with different needs can group the modeset commits together
 * and do the plane commits at the end. This is useful for drivers doing runtime
 * PM since planes updates then only happen when the CRTC is actually enabled.
 */
void msm_atomic_helper_commit_modeset_disables(struct drm_device *dev,
		struct drm_atomic_state *old_state)
{
	msm_disable_outputs(dev, old_state);

	drm_atomic_helper_update_legacy_modeset_state(dev, old_state);

	msm_crtc_set_mode(dev, old_state);
}

/**
 * msm_atomic_helper_commit_modeset_enables - modeset commit to enable outputs
 * @dev: DRM device
 * @old_state: atomic state object with old state structures
 *
 * This function enables all the outputs with the new configuration which had to
 * be turned off for the update.
 *
 * For compatibility with legacy crtc helpers this should be called after
 * drm_atomic_helper_commit_planes(), which is what the default commit function
 * does. But drivers with different needs can group the modeset commits together
 * and do the plane commits at the end. This is useful for drivers doing runtime
 * PM since planes updates then only happen when the CRTC is actually enabled.
 */
static void msm_atomic_helper_commit_modeset_enables(struct drm_device *dev,
		struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *old_conn_state;
	struct msm_drm_notifier notifier_data;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	int bridge_enable_count = 0;
	int i, blank;

	SDE_ATRACE_BEGIN("msm_enable");
	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		/* Need to filter out CRTCs where only planes change. */
		if (!drm_atomic_crtc_needs_modeset(crtc->state))
			continue;

		if (!crtc->state->active)
			continue;

		if (_msm_seamless_for_crtc(old_state, crtc->state, true))
			continue;

		funcs = crtc->helper_private;

		if (crtc->state->enable) {
			DRM_DEBUG_ATOMIC("enabling [CRTC:%d]\n",
					 crtc->base.id);

			if (funcs->enable)
				funcs->enable(crtc);
			else
				funcs->commit(crtc);
		}

		if (msm_needs_vblank_pre_modeset(&crtc->state->adjusted_mode))
			drm_crtc_wait_one_vblank(crtc);
	}

	for_each_connector_in_state(old_state, connector, old_conn_state, i) {
		const struct drm_encoder_helper_funcs *funcs;
		struct drm_encoder *encoder;

		if (!connector->state->best_encoder)
			continue;

		if (!connector->state->crtc->state->active ||
		    !drm_atomic_crtc_needs_modeset(
				    connector->state->crtc->state))
			continue;

		if (_msm_seamless_for_conn(connector, old_conn_state, true))
			continue;

		encoder = connector->state->best_encoder;
		funcs = encoder->helper_private;

		DRM_DEBUG_ATOMIC("enabling [ENCODER:%d:%s]\n",
				 encoder->base.id, encoder->name);

		if (connector->state->crtc->state->active_changed) {
			blank = MSM_DRM_BLANK_UNBLANK;
			notifier_data.data = &blank;
			notifier_data.id =
				connector->state->crtc->index;
			DRM_DEBUG_ATOMIC("Notify early unblank\n");
			msm_drm_notifier_call_chain(MSM_DRM_EARLY_EVENT_BLANK,
					    &notifier_data);
		}
		/*
		 * Each encoder has at most one connector (since we always steal
		 * it away), so we won't call enable hooks twice.
		 */
		drm_bridge_pre_enable(encoder->bridge);
		++bridge_enable_count;

		if (funcs->enable)
			funcs->enable(encoder);
		else
			funcs->commit(encoder);
	}

	if (kms->funcs->commit) {
		DRM_DEBUG_ATOMIC("triggering commit\n");
		kms->funcs->commit(kms, old_state);
	}

	/* If no bridges were pre_enabled, skip iterating over them again */
	if (bridge_enable_count == 0) {
		SDE_ATRACE_END("msm_enable");
		return;
	}

	for_each_connector_in_state(old_state, connector, old_conn_state, i) {
		struct drm_encoder *encoder;

		if (!connector->state->best_encoder)
			continue;

		if (!connector->state->crtc->state->active ||
		    !drm_atomic_crtc_needs_modeset(
				    connector->state->crtc->state))
			continue;

		if (_msm_seamless_for_conn(connector, old_conn_state, true))
			continue;

		encoder = connector->state->best_encoder;

		DRM_DEBUG_ATOMIC("bridge enable enabling [ENCODER:%d:%s]\n",
				 encoder->base.id, encoder->name);

		drm_bridge_enable(encoder->bridge);
		if (connector->state->crtc->state->active_changed) {
			DRM_DEBUG_ATOMIC("Notify unblank\n");
			msm_drm_notifier_call_chain(MSM_DRM_EVENT_BLANK,
					    &notifier_data);
		}
	}
	SDE_ATRACE_END("msm_enable");
}

/* The (potentially) asynchronous part of the commit.  At this point
 * nothing can fail short of armageddon.
 */
static void complete_commit(struct msm_commit *c)
{
	struct drm_atomic_state *state = c->state;
	struct drm_device *dev = state->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;

	drm_atomic_helper_wait_for_fences(dev, state, false);

#ifdef CONFIG_SHARP_DISPLAY /* CUST_ID_00009 */
	if ((priv != NULL) && (priv->upper_unit_is_connected == DRM_UPPER_UNIT_IS_NOT_CONNECTED)) {
		pr_debug("%s: upper unit is not connected\n", __func__);
		goto exit;
	}
#endif /* CONFIG_SHARP_DISPLAY */

	kms->funcs->prepare_commit(kms, state);

	msm_atomic_helper_commit_modeset_disables(dev, state);

	drm_atomic_helper_commit_planes(dev, state, 0);

	msm_atomic_helper_commit_modeset_enables(dev, state);

	/* NOTE: _wait_for_vblanks() only waits for vblank on
	 * enabled CRTCs.  So we end up faulting when disabling
	 * due to (potentially) unref'ing the outgoing fb's
	 * before the vblank when the disable has latched.
	 *
	 * But if it did wait on disabled (or newly disabled)
	 * CRTCs, that would be racy (ie. we could have missed
	 * the irq.  We need some way to poll for pipe shut
	 * down.  Or just live with occasionally hitting the
	 * timeout in the CRTC disable path (which really should
	 * not be critical path)
	 */

	msm_atomic_wait_for_commit_done(dev, state);

#ifdef CONFIG_SHARP_DISPLAY /* CUST_ID_00007 */
	msm_atomic_update_mipiclk(state);
#endif /* CONFIG_SHARP_DISPLAY */

	drm_atomic_helper_cleanup_planes(dev, state);

	kms->funcs->complete_commit(kms, state);

#ifdef CONFIG_SHARP_DISPLAY /* CUST_ID_00009 */
exit:
#endif /* CONFIG_SHARP_DISPLAY */

	drm_atomic_state_free(state);

	commit_destroy(c);
}

static void _msm_drm_commit_work_cb(struct kthread_work *work)
{
	struct msm_commit *commit =  NULL;

	if (!work) {
		DRM_ERROR("%s: Invalid commit work data!\n", __func__);
		return;
	}

	commit = container_of(work, struct msm_commit, commit_work);

	SDE_ATRACE_BEGIN("complete_commit");
	complete_commit(commit);
	SDE_ATRACE_END("complete_commit");
}

static struct msm_commit *commit_init(struct drm_atomic_state *state,
		bool nonblock)
{
	struct msm_commit *c = kzalloc(sizeof(*c), GFP_KERNEL);

	if (!c)
		return NULL;

	c->dev = state->dev;
	c->state = state;
	c->nonblock = nonblock;

	kthread_init_work(&c->commit_work, _msm_drm_commit_work_cb);

	return c;
}

/* Start display thread function */
static void msm_atomic_commit_dispatch(struct drm_device *dev,
		struct drm_atomic_state *state, struct msm_commit *commit)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct drm_crtc *crtc = NULL;
	struct drm_crtc_state *crtc_state = NULL;
	int ret = -EINVAL, i = 0, j = 0;
	bool nonblock;

	/* cache since work will kfree commit in non-blocking case */
	nonblock = commit->nonblock;

	for_each_crtc_in_state(state, crtc, crtc_state, i) {
		for (j = 0; j < priv->num_crtcs; j++) {
			if (priv->disp_thread[j].crtc_id ==
						crtc->base.id) {
				if (priv->disp_thread[j].thread) {
					kthread_queue_work(
						&priv->disp_thread[j].worker,
							&commit->commit_work);
					/* only return zero if work is
					 * queued successfully.
					 */
					ret = 0;
				} else {
					DRM_ERROR(" Error for crtc_id: %d\n",
						priv->disp_thread[j].crtc_id);
				}
				break;
			}
		}
		/*
		 * TODO: handle cases where there will be more than
		 * one crtc per commit cycle. Remove this check then.
		 * Current assumption is there will be only one crtc
		 * per commit cycle.
		 */
		if (j < priv->num_crtcs)
			break;
	}

	if (ret) {
		/**
		 * this is not expected to happen, but at this point the state
		 * has been swapped, but we couldn't dispatch to a crtc thread.
		 * fallback now to a synchronous complete_commit to try and
		 * ensure that SW and HW state don't get out of sync.
		 */
		DRM_ERROR("failed to dispatch commit to any CRTC\n");
		complete_commit(commit);
	} else if (!nonblock) {
		kthread_flush_work(&commit->commit_work);
	}

	/* free nonblocking commits in this context, after processing */
	if (!nonblock)
		kfree(commit);
}

/**
 * drm_atomic_helper_commit - commit validated state object
 * @dev: DRM device
 * @state: the driver state object
 * @nonblock: nonblocking commit
 *
 * This function commits a with drm_atomic_helper_check() pre-validated state
 * object. This can still fail when e.g. the framebuffer reservation fails.
 *
 * RETURNS
 * Zero for success or -errno.
 */
int msm_atomic_commit(struct drm_device *dev,
		struct drm_atomic_state *state, bool nonblock)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_commit *c;
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	int i, ret;

	if (!priv || priv->shutdown_in_progress) {
		DRM_ERROR("priv is null or shutdwon is in-progress\n");
		return -EINVAL;
	}

	SDE_ATRACE_BEGIN("atomic_commit");
	ret = drm_atomic_helper_prepare_planes(dev, state);
	if (ret) {
		SDE_ATRACE_END("atomic_commit");
		return ret;
	}

	c = commit_init(state, nonblock);
	if (!c) {
		ret = -ENOMEM;
		goto error;
	}

	/*
	 * Figure out what crtcs we have:
	 */
	for_each_crtc_in_state(state, crtc, crtc_state, i)
		c->crtc_mask |= drm_crtc_mask(crtc);

	/*
	 * Figure out what fence to wait for:
	 */
	for_each_plane_in_state(state, plane, plane_state, i) {
		if ((plane->state->fb != plane_state->fb) && plane_state->fb) {
			struct drm_gem_object *obj = msm_framebuffer_bo(plane_state->fb, 0);
			struct msm_gem_object *msm_obj = to_msm_bo(obj);

			plane_state->fence = reservation_object_get_excl_rcu(msm_obj->resv);
		}
	}

	/*
	 * Wait for pending updates on any of the same crtc's and then
	 * mark our set of crtc's as busy:
	 */
	ret = start_atomic(dev->dev_private, c->crtc_mask);
	if (ret) {
		kfree(c);
		goto error;
	}

	/*
	 * This is the point of no return - everything below never fails except
	 * when the hw goes bonghits. Which means we can commit the new state on
	 * the software side now.
	 */

	drm_atomic_helper_swap_state(state, true);

	/*
	 * Provide the driver a chance to prepare for output fences. This is
	 * done after the point of no return, but before asynchronous commits
	 * are dispatched to work queues, so that the fence preparation is
	 * finished before the .atomic_commit returns.
	 */
	if (priv->kms && priv->kms->funcs && priv->kms->funcs->prepare_fence)
		priv->kms->funcs->prepare_fence(priv->kms, state);

	/*
	 * Everything below can be run asynchronously without the need to grab
	 * any modeset locks at all under one conditions: It must be guaranteed
	 * that the asynchronous work has either been cancelled (if the driver
	 * supports it, which at least requires that the framebuffers get
	 * cleaned up with drm_atomic_helper_cleanup_planes()) or completed
	 * before the new state gets committed on the software side with
	 * drm_atomic_helper_swap_state().
	 *
	 * This scheme allows new atomic state updates to be prepared and
	 * checked in parallel to the asynchronous completion of the previous
	 * update. Which is important since compositors need to figure out the
	 * composition of the next frame right after having submitted the
	 * current layout.
	 */

	msm_atomic_commit_dispatch(dev, state, c);
	SDE_ATRACE_END("atomic_commit");
	return 0;

error:
	drm_atomic_helper_cleanup_planes(dev, state);
	SDE_ATRACE_END("atomic_commit");
	return ret;
}

#ifdef CONFIG_SHARP_DISPLAY /* CUST_ID_00007 */
static int msm_atomic_update_panel_timing(struct dsi_display *display,
	struct mdp_mipi_clkchg_param *clkchg_param)
{
	int rc = 0;
	int cmd_cnt;
	struct dsi_cmd_desc *cmds;

	unsigned char addr_value[29][6] = {
		{0xFF,0x10               },	// Switch to Command 1
		{0xBE,0x00,0x0E,0x00,0x14},	// MIPI_VBP_HF/MIPI_VFP_HF
		{0xFF,0x24               },	// Switch to Command 2 page 4
		{0x14,0x00               },	// VBP_NORM_HF
		{0x15,0x0E               },	// VBP_NORM_HF
		{0x16,0x00               },	// VFP_NORM_HF
		{0x17,0x14               },	// VFP_NORM_HF
		{0x08,0x00               },	// RTN_V_HF
		{0x09,0x67               },
		{0x12,0x00               },	// RTN_HF
		{0x13,0x67               },
		{0xFF,0x25               },	// Switch to Command 2 page 5
		{0x92,0x01               },	// STV_DELAY_HF
		{0x93,0x25               },	// STV_ADV_HF
		{0x96,0x01               },	// GCK_DELAY_HF
		{0x97,0x35               },	// GCK_ADV_HF
		{0xFF,0x24               },	// Switch to Command 2 page 4
		{0x38,0x01               },	// SOEHT_HF/SDT_REG_HF
		{0x7D,0x01               },	// MUXS_HF
		{0x7E,0x62               },	// MUXW_HF
		{0x80,0x01               },	// MUXS_V_HF
		{0x81,0x62               },	// MUXW_V_HF
		{0xFF,0xF0               },	// Switch to Command 3 page F0
		{0x33,0x13               },	// OSCSET1
		{0x34,0x37               },	// OSCSET2
		{0xFF,0xE0               },	// Switch to Command 3 page E0
		{0x82,0x11               },	// OSCSCOPE
		{0x81,0x65               },	// OSC_FINE_TRIM
		{0xFF,0x10               }	// Switch to Command 1
	};
	struct dsi_cmd_desc paneltiming_cmd[] = {
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[ 0], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 5, addr_value[ 1], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[ 2], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[ 3], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[ 4], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[ 5], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[ 6], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[ 7], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[ 8], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[ 9], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[10], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[11], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[12], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[13], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[14], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[15], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[16], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[17], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[18], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[19], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[20], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[21], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[22], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[23], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[24], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[25], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[26], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[27], 0, 0}, 0, 0},
		{{0, MIPI_DSI_DCS_LONG_WRITE, 0, 0, 0, 2, addr_value[28], 0, 0}, 1, 0}
	};

	pr_debug("%s\n", __func__);

	addr_value[ 1][1] = clkchg_param->panel.mipi_vbp_hf[0];
	addr_value[ 1][2] = clkchg_param->panel.mipi_vbp_hf[1];
	addr_value[ 1][3] = clkchg_param->panel.mipi_vfp_hf[0];
	addr_value[ 1][4] = clkchg_param->panel.mipi_vfp_hf[1];

	addr_value[ 3][1] = clkchg_param->panel.vbp_norm_hf[0];
	addr_value[ 4][1] = clkchg_param->panel.vbp_norm_hf[1];
	addr_value[ 5][1] = clkchg_param->panel.vfp_norm_hf[0];
	addr_value[ 6][1] = clkchg_param->panel.vfp_norm_hf[1];
	addr_value[ 7][1] = clkchg_param->panel.rtn_v_hf[0];
	addr_value[ 8][1] = clkchg_param->panel.rtn_v_hf[1];
	addr_value[ 9][1] = clkchg_param->panel.rtn_hf[0];
	addr_value[10][1] = clkchg_param->panel.rtn_hf[1];

	addr_value[12][1] = clkchg_param->panel.stv_delay_hf;
	addr_value[13][1] = clkchg_param->panel.stv_adv_hf;
	addr_value[14][1] = clkchg_param->panel.gck_delay_hf;
	addr_value[15][1] = clkchg_param->panel.gck_adv_hf;

	addr_value[17][1] = clkchg_param->panel.soeht_hf;
	addr_value[18][1] = clkchg_param->panel.muxs_hf;
	addr_value[19][1] = clkchg_param->panel.muxw_hf;
	addr_value[20][1] = clkchg_param->panel.muxs_v_hf;
	addr_value[21][1] = clkchg_param->panel.muxw_v_hf;

	addr_value[23][1] = clkchg_param->panel.oscset1;
	addr_value[24][1] = clkchg_param->panel.oscset2;

	addr_value[26][1] = clkchg_param->panel.oscscope;

	addr_value[27][1] = clkchg_param->panel.osc_fine_trim;

	cmd_cnt = ARRAY_SIZE(paneltiming_cmd);
	cmds = paneltiming_cmd;

	rc = drm_cmn_panel_cmds_transfer(display, cmds, cmd_cnt);
	if (rc) {
		pr_err("%s:failed to set cmds, rc=%d\n", __func__, rc);
	}

	return rc;
}

static int msm_atomic_dsi_ctrl_update_vid_engine_state(struct dsi_display *display, bool on)
{
	int rc = 0;
	int i;
	struct dsi_ctrl *dsi_ctrl;

	pr_debug("%s\n", __func__);

	for (i = 0; i < display->ctrl_count; i++) {
		dsi_ctrl = display->ctrl[i].ctrl;
		if (!dsi_ctrl) {
			pr_err("%s: no dsi_ctrl\n", __func__);
			return -EINVAL;
		}
		dsi_ctrl_update_vid_engine_state(dsi_ctrl, on);
	}

	return rc;
}

static void msm_atomic_set_phys_cached_mode(struct dsi_display *display,
	struct dsi_display_mode *adj_mode)
{
	struct drm_display_mode drm_mode;
#ifdef CONFIG_ARCH_DIO
	struct sde_encoder_phys_vid *vid_enc = NULL;
#else
	struct sde_encoder_phys_cmd *cmd_enc = NULL;
#endif /* CONFIG_ARCH_DIO */
	struct sde_encoder_phys *phys_enc = NULL;
	int ctrl_count;
	int i = 0;

	pr_debug("%s\n", __func__);

	memset(&drm_mode, 0x00, sizeof(struct drm_display_mode));
	ctrl_count = display->ctrl_count;

	drm_mode.hdisplay = adj_mode->timing.h_active * ctrl_count;
	drm_mode.hsync_start = drm_mode.hdisplay +
				adj_mode->timing.h_front_porch * ctrl_count;
	drm_mode.hsync_end = drm_mode.hsync_start +
				adj_mode->timing.h_sync_width * ctrl_count;
	drm_mode.htotal = drm_mode.hsync_end +
				adj_mode->timing.h_back_porch * ctrl_count;
	drm_mode.hskew = adj_mode->timing.h_skew * ctrl_count;

	drm_mode.vdisplay = adj_mode->timing.v_active;
	drm_mode.vsync_start = drm_mode.vdisplay +
				adj_mode->timing.v_front_porch;
	drm_mode.vsync_end = drm_mode.vsync_start +
				adj_mode->timing.v_sync_width;
	drm_mode.vtotal = drm_mode.vsync_end +
				adj_mode->timing.v_back_porch;

	drm_mode.vrefresh = adj_mode->timing.refresh_rate;
	drm_mode.clock = adj_mode->pixel_clk_khz * ctrl_count;

	drm_mode.private = (int *)adj_mode->priv_info;

	for (i = 0; i < ctrl_count; i++) {
#ifdef CONFIG_ARCH_DIO
		vid_enc = get_sde_encoder_phys_vid(i);
		if (vid_enc) {
			phys_enc = &vid_enc->base;

			phys_enc->cached_mode = drm_mode;
		}
#else
		cmd_enc = get_sde_encoder_phys_cmd(i);
		if (cmd_enc) {
			phys_enc = &cmd_enc->base;

			phys_enc->cached_mode = drm_mode;
		}
#endif /* CONFIG_ARCH_DIO */
	}
}

static void msm_atomic_mipiclk_adjusted_mode(struct dsi_display *display,
	struct dsi_display_mode *adj_mode,
	struct mdp_mipi_clkchg_param *clkchg_param)
{
	struct dsi_display_mode_priv_info *priv_info = NULL;
	int i;

	pr_debug("%s\n", __func__);

	display->panel->host_config.t_clk_post =
				clkchg_param->host.t_clk_post;
	display->panel->host_config.t_clk_pre  =
				clkchg_param->host.t_clk_pre;

	adj_mode->timing.h_active      =
		clkchg_param->host.display_width;
	adj_mode->timing.v_active      =
		clkchg_param->host.display_height;
	adj_mode->timing.h_sync_width  =
		clkchg_param->host.hsync_pulse_width;
	adj_mode->timing.h_back_porch  =
		clkchg_param->host.h_back_porch;
	adj_mode->timing.h_front_porch =
		clkchg_param->host.h_front_porch;
	adj_mode->timing.v_sync_width  =
		clkchg_param->host.vsync_pulse_width;
	adj_mode->timing.v_back_porch  =
		clkchg_param->host.v_back_porch;
	adj_mode->timing.v_front_porch =
		clkchg_param->host.v_front_porch;
	adj_mode->timing.refresh_rate  =
		clkchg_param->host.frame_rate;

	if (display->ctrl_count > 1) {
		adj_mode->timing.h_active /= display->ctrl_count;
	}

	adj_mode->pixel_clk_khz = (DSI_H_TOTAL(&adj_mode->timing) *
			DSI_V_TOTAL(&adj_mode->timing) *
			adj_mode->timing.refresh_rate) / 1000;

	priv_info = (struct dsi_display_mode_priv_info*)adj_mode->priv_info;
	for (i = 0;i < priv_info->phy_timing_len;i++) {
		priv_info->phy_timing_val[i] =
			clkchg_param->host.timing_ctrl[i];
	}
	priv_info->clk_rate_hz = clkchg_param->host.clock_rate;
}

static int msm_atomic_setup_timing_engine(struct drm_crtc *crtc,
	struct dsi_display *display)
{
	struct drm_display_mode *adjusted_mode;
	struct dsi_display_mode *dsi_mode;
	struct sde_encoder_phys_vid *vid_enc = NULL;
	struct sde_encoder_phys *phys_enc = NULL;
	int i;
	int rc = 0;
	int ctrl_count;

	pr_debug("%s\n", __func__);

	dsi_mode = display->panel->cur_mode;
	adjusted_mode = &crtc->state->adjusted_mode;
	if (!adjusted_mode) {
		pr_err("[%s]adjusted_mode is null\n", __func__);
		return -EINVAL;
	}
	ctrl_count = display->ctrl_count;

	adjusted_mode->hdisplay = dsi_mode->timing.h_active * ctrl_count;
	adjusted_mode->hsync_start = adjusted_mode->hdisplay +
				dsi_mode->timing.h_front_porch * ctrl_count;
	adjusted_mode->hsync_end = adjusted_mode->hsync_start +
				dsi_mode->timing.h_sync_width * ctrl_count;
	adjusted_mode->htotal = adjusted_mode->hsync_end +
				dsi_mode->timing.h_back_porch * ctrl_count;
	adjusted_mode->hskew = dsi_mode->timing.h_skew * ctrl_count;

	adjusted_mode->vdisplay = dsi_mode->timing.v_active;
	adjusted_mode->vsync_start = adjusted_mode->vdisplay +
				dsi_mode->timing.v_front_porch;
	adjusted_mode->vsync_end = adjusted_mode->vsync_start +
				dsi_mode->timing.v_sync_width;
	adjusted_mode->vtotal = adjusted_mode->vsync_end +
				dsi_mode->timing.v_back_porch;

	adjusted_mode->vrefresh = dsi_mode->timing.refresh_rate;
	adjusted_mode->clock = dsi_mode->pixel_clk_khz * ctrl_count;

	adjusted_mode->private = (int *)dsi_mode->priv_info;

	for (i = 0; i < display->ctrl_count; i++) {
		vid_enc = get_sde_encoder_phys_vid(i);
		if (vid_enc) {
			phys_enc = &vid_enc->base;

			sde_encoder_phys_vid_setup_timing_engine_wrap(phys_enc);
		}
	}

	return rc;
}

static int msm_atomic_mipiclk_config_dsi(struct dsi_display *display,
	struct drm_crtc *crtc, struct mdp_mipi_clkchg_param *clkchg_param)
{
	struct dsi_display_mode *adj_mode;
	int rc = 0;

	pr_debug("%s\n", __func__);

	if (!display->panel->cur_mode) {
		pr_err("[%s]failed to display->panel->cur_mode is null\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&display->display_lock);
	mutex_lock(&display->panel->panel_lock);

	adj_mode = display->panel->cur_mode;

	msm_atomic_mipiclk_adjusted_mode(display, adj_mode, clkchg_param);

	adj_mode->timing.dsc_enabled = display->config.video_timing.dsc_enabled;
	adj_mode->timing.dsc         = display->config.video_timing.dsc;

	msm_atomic_set_phys_cached_mode(display, adj_mode);

	mutex_unlock(&display->panel->panel_lock);

	rc = dsi_display_set_mode_sub_wrap(display, adj_mode, 0x00);
	if (rc) {
		pr_err("[%s]failed to dsi_display_set_mode, rc=%d\n",
			__func__, rc);
	}

	mutex_unlock(&display->display_lock);

	return rc;
}

static void msm_atomic_update_mipiclk_chg_log(
	struct mdp_mipi_clkchg_param *clkchg_param)
{
	int i;

	pr_debug("[%s]param->host.frame_rate         = %10d", __func__  , clkchg_param->host.frame_rate          );
	pr_debug("[%s]param->host.clock_rate         = %10d", __func__  , clkchg_param->host.clock_rate          );
	pr_debug("[%s]param->host.display_width      = %10d", __func__  , clkchg_param->host.display_width       );
	pr_debug("[%s]param->host.display_height     = %10d", __func__  , clkchg_param->host.display_height      );
	pr_debug("[%s]param->host.hsync_pulse_width  = %10d", __func__  , clkchg_param->host.hsync_pulse_width   );
	pr_debug("[%s]param->host.h_back_porch       = %10d", __func__  , clkchg_param->host.h_back_porch        );
	pr_debug("[%s]param->host.h_front_porch      = %10d", __func__  , clkchg_param->host.h_front_porch       );
	pr_debug("[%s]param->host.vsync_pulse_width  = %10d", __func__  , clkchg_param->host.vsync_pulse_width   );
	pr_debug("[%s]param->host.v_back_porch       = %10d", __func__  , clkchg_param->host.v_back_porch        );
	pr_debug("[%s]param->host.v_front_porch      = %10d", __func__  , clkchg_param->host.v_front_porch       );
	pr_debug("[%s]param->host.t_clk_post         = 0x%02X", __func__, clkchg_param->host.t_clk_post          );
	pr_debug("[%s]param->host.t_clk_pre          = 0x%02X", __func__, clkchg_param->host.t_clk_pre           );
	for (i = 0; i < 12; i++) {
		pr_debug("[%s]param->host.timing_ctrl[%02d]    = 0x%02X", __func__, i, clkchg_param->host.timing_ctrl[i]);
	}
	pr_debug("[%s]param->panel.mipi_vbp_hf[0]    = 0x%02X",__func__, clkchg_param->panel.mipi_vbp_hf[0]     );
	pr_debug("[%s]param->panel.mipi_vbp_hf[1]    = 0x%02X",__func__, clkchg_param->panel.mipi_vbp_hf[1]     );
	pr_debug("[%s]param->panel.mipi_vfp_hf[0]    = 0x%02X",__func__, clkchg_param->panel.mipi_vfp_hf[0]     );
	pr_debug("[%s]param->panel.mipi_vfp_hf[1]    = 0x%02X",__func__, clkchg_param->panel.mipi_vfp_hf[1]     );
	pr_debug("[%s]param->panel.vbp_norm_hf[0]    = 0x%02X",__func__, clkchg_param->panel.vbp_norm_hf[0]     );
	pr_debug("[%s]param->panel.vbp_norm_hf[1]    = 0x%02X",__func__, clkchg_param->panel.vbp_norm_hf[1]     );
	pr_debug("[%s]param->panel.vfp_norm_hf[0]    = 0x%02X",__func__, clkchg_param->panel.vfp_norm_hf[0]     );
	pr_debug("[%s]param->panel.vfp_norm_hf[1]    = 0x%02X",__func__, clkchg_param->panel.vfp_norm_hf[1]     );
	pr_debug("[%s]param->panel.rtn_v_hf[0]       = 0x%02X",__func__, clkchg_param->panel.rtn_v_hf[0]        );
	pr_debug("[%s]param->panel.rtn_v_hf[1]       = 0x%02X",__func__, clkchg_param->panel.rtn_v_hf[1]        );
	pr_debug("[%s]param->panel.rtn_hf[0]         = 0x%02X",__func__, clkchg_param->panel.rtn_hf[0]          );
	pr_debug("[%s]param->panel.rtn_hf[1]         = 0x%02X",__func__, clkchg_param->panel.rtn_hf[1]          );
	pr_debug("[%s]param->panel.stv_delay_hf      = 0x%02X",__func__, clkchg_param->panel.stv_delay_hf       );
	pr_debug("[%s]param->panel.stv_adv_hf        = 0x%02X",__func__, clkchg_param->panel.stv_adv_hf         );
	pr_debug("[%s]param->panel.gck_delay_hf      = 0x%02X",__func__, clkchg_param->panel.gck_delay_hf       );
	pr_debug("[%s]param->panel.gck_adv_hf        = 0x%02X",__func__, clkchg_param->panel.gck_adv_hf         );
	pr_debug("[%s]param->panel.soeht_hf          = 0x%02X",__func__, clkchg_param->panel.soeht_hf           );
	pr_debug("[%s]param->panel.muxs_hf           = 0x%02X",__func__, clkchg_param->panel.muxs_hf            );
	pr_debug("[%s]param->panel.muxw_hf           = 0x%02X",__func__, clkchg_param->panel.muxw_hf            );
	pr_debug("[%s]param->panel.muxs_v_hf         = 0x%02X",__func__, clkchg_param->panel.muxs_v_hf          );
	pr_debug("[%s]param->panel.muxw_v_hf         = 0x%02X",__func__, clkchg_param->panel.muxw_v_hf          );
	pr_debug("[%s]param->panel.oscset1           = 0x%02X",__func__, clkchg_param->panel.oscset1            );
	pr_debug("[%s]param->panel.oscset2           = 0x%02X",__func__, clkchg_param->panel.oscset2            );
	pr_debug("[%s]param->panel.oscscope          = 0x%02X",__func__, clkchg_param->panel.oscscope           );
	pr_debug("[%s]param->panel.osc_fine_trim     = 0x%02X",__func__, clkchg_param->panel.osc_fine_trim      );
}

#ifdef CONFIG_SHARP_DRM_HR_VID /* CUST_ID_00015 */
static int msm_atomic_update_mipiclk_chg(struct dsi_display *display,
	struct msm_drm_private *priv, struct drm_crtc *crtc,
	struct mdp_mipi_clkchg_param *clkchg_param, int* cnt)
#else
static int msm_atomic_update_mipiclk_chg(struct dsi_display *display,
	struct msm_drm_private *priv, struct drm_crtc *crtc,
	struct mdp_mipi_clkchg_param *clkchg_param)
#endif /* CONFIG_SHARP_DRM_HR_VID */
{
	int rc = 0;
	int new_vtotal = 0;
	int old_vtotal = 0;
	bool timing_engine_setup = false;
	int i = 0;
	struct dsi_ctrl *dsi_ctrl= display->ctrl[0].ctrl;

	pr_debug("[%s] in\n", __func__);

	drm_det_pre_panel_off();

	msm_atomic_update_mipiclk_chg_log(clkchg_param);

#ifdef CONFIG_SHARP_DRM_HR_VID /* CUST_ID_00015 */
	drm_mfr_suspend_ctrl(true);
#endif /* CONFIG_SHARP_DRM_HR_VID */

	rc = drm_cmn_stop_video();
	if (rc) {
		pr_err("[%s]failed to video transfer off, rc=%d\n",
			__func__, rc);
		return rc;
	}

	msm_atomic_update_panel_timing(display, clkchg_param);

	if (dsi_ctrl) {
		if (dsi_ctrl->hw.ops.mask_error_intr) {
			dsi_ctrl->hw.ops.mask_error_intr(&dsi_ctrl->hw,
				BIT(DSI_FIFO_OVERFLOW), true);
			dsi_ctrl->hw.ops.mask_error_intr(&dsi_ctrl->hw,
				BIT(DSI_FIFO_UNDERFLOW), true);
		}
	}

	for (i = 0; i < display->ctrl_count; i++) {
		struct msm_dsi_phy *phy = display->ctrl[i].phy;

		if (!phy)
			continue;

		phy->allow_phy_power_off = true;
	}

	rc = dsi_display_clk_ctrl(display->mdp_clk_handle
					, DSI_ALL_CLKS, DSI_CLK_OFF);
	if (rc) {
		pr_err("[%s]failed to disable MDP clocks, rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle
					, DSI_ALL_CLKS, DSI_CLK_OFF);
	if (rc) {
		pr_err("[%s]failed to disable DSI clocks, rc=%d\n",
			__func__, rc);
		return rc;
	}

	old_vtotal = DSI_V_TOTAL(&display->config.video_timing);

	rc = msm_atomic_mipiclk_config_dsi(display, crtc, clkchg_param);
	if (rc) {
		pr_err("[%s]failed to update DSI clocks, rc=%d\n",
			__func__, rc);
		return rc;
	}

	new_vtotal = DSI_V_TOTAL(&display->config.video_timing);

	pr_debug("[%s]new = %d, old = %d\n", __func__, new_vtotal, old_vtotal);

	if (old_vtotal <= new_vtotal) {
		rc = msm_atomic_setup_timing_engine(crtc, display);
		if (rc) {
			pr_err("[%s]failed to msm_atomic_setup_timing_engine, rc=%d\n",
				__func__, rc);
			return rc;
		}
		timing_engine_setup = true;

	} else {
		rc = msm_atomic_dsi_ctrl_update_vid_engine_state(display, true);
		if (rc) {
			pr_err("[%s]failed to update_vid_engine_state, rc=%d\n",
				__func__, rc);
			return rc;
		}
	}

	priv->mipiclkchg_progress = true;

	rc = dsi_display_clk_ctrl(display->dsi_clk_handle
					, DSI_ALL_CLKS, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s]failed to enable DSI clocks, rc=%d\n",
			__func__, rc);
		return rc;
	}

	rc = dsi_display_clk_ctrl(display->mdp_clk_handle
					, DSI_ALL_CLKS, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s]failed to enable MDP clocks, rc=%d\n",
			__func__, rc);
		return rc;
	}

	priv->mipiclkchg_progress = false;

	for (i = 0; i < display->ctrl_count; i++) {
		struct msm_dsi_phy *phy = display->ctrl[i].phy;

		if (!phy)
			continue;

		phy->allow_phy_power_off = false;
	}

	rc = drm_cmn_start_video();
	if (rc) {
		pr_err("[%s]failed to video transfer on, rc=%d\n",
			__func__, rc);
		return rc;
	}

	if (!timing_engine_setup) {
		rc = msm_atomic_setup_timing_engine(crtc, display);
		if (rc) {
			pr_err("[%s]failed to msm_atomic_setup_timing_engine, rc=%d\n",
				__func__, rc);
			return rc;
		}
#ifdef CONFIG_SHARP_DRM_HR_VID /* CUST_ID_00015 */
		*cnt = mipiclkcng_cnt;
		pr_debug("%s B to A. mipiclkcng_cnt=%d\n",__func__, mipiclkcng_cnt);
#endif /* CONFIG_SHARP_DRM_HR_VID */
	}

#ifdef CONFIG_SHARP_DRM_HR_VID /* CUST_ID_00015 */
	drm_mfr_suspend_ctrl(false);
#endif /* CONFIG_SHARP_DRM_HR_VID */

	pr_debug("[%s] out\n", __func__);

	return rc;
}

int msm_atomic_update_mipiclk_resume(struct dsi_display *display,
	struct dsi_display_mode *adj_mode)
{
	struct msm_drm_private *priv;
	struct mdp_mipi_clkchg_param *clkchg_param = NULL;

	pr_debug("%s\n", __func__);

	priv = display->drm_dev->dev_private;

	if (priv) {
		mutex_lock(&priv->mipiclk_lock);
		clkchg_param = &priv->usr_clkchg_param;

		if (clkchg_param->host.clock_rate) {
			msm_atomic_mipiclk_adjusted_mode(display, adj_mode, clkchg_param);

			msm_atomic_set_phys_cached_mode(display, adj_mode);

			priv->mipiclk_pending = false;
		}
		mutex_unlock(&priv->mipiclk_lock);
	}

	return 0;
}

static int msm_atomic_update_mipiclk(struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state = NULL;
	struct msm_drm_private *priv = NULL;
	struct dsi_display *display = NULL;
	struct mdp_mipi_clkchg_param clkchg_param;
	int i, rc = 0;

	display = msm_drm_get_dsi_displey();
	if (!display) {
		pr_err("[%s]Invalid dsi_display\n", __func__);
		return -EINVAL;
	}

	priv = display->drm_dev->dev_private;
	if (!priv) {
		pr_err("[%s]Invalid dev_private\n", __func__);
		return -EINVAL;
	}

#ifdef CONFIG_SHARP_DRM_HR_VID /* CUST_ID_00015 */
	if (priv->mipiclk_pending && priv->mipiclk_cnt < 2) {
#else
	if (priv->mipiclk_pending) {
#endif /* CONFIG_SHARP_DRM_HR_VID */
		for_each_crtc_in_state(state, crtc, crtc_state, i) {
			if (crtc && crtc_state && drm_crtc_index(crtc) == 0) {
				pr_debug("[%s]crtc_state->active=%d\n", __func__, crtc_state->active);

				if(!crtc_state->active)
					continue;

				mutex_lock(&priv->mipiclk_lock);
				memcpy(&clkchg_param, &priv->usr_clkchg_param,
						sizeof(clkchg_param));
#ifdef CONFIG_SHARP_DRM_HR_VID /* CUST_ID_00015 */
				rc = msm_atomic_update_mipiclk_chg(display, priv, crtc,
						&clkchg_param, &priv->mipiclk_cnt);
#else
				rc = msm_atomic_update_mipiclk_chg(display, priv, crtc,
						&clkchg_param);
#endif /* CONFIG_SHARP_DRM_HR_VID */
				priv->mipiclk_pending = false;
				mutex_unlock(&priv->mipiclk_lock);
				break;
			}
		}
	}

	return rc;
}

int msm_atomic_update_panel_timing_resume(struct dsi_display *display)
{
	int rc = 0;
	int clk_rate_hz, default_clk_rate_hz;
	struct msm_drm_private *priv = NULL;

	pr_debug("%s\n", __func__);

	if (!display) {
		pr_err("[%s]Invalid dsi_display\n", __func__);
		return -EINVAL;
	}

	priv = display->drm_dev->dev_private;
	if (!priv) {
		pr_err("[%s]Invalid dev_private\n", __func__);
		return -EINVAL;
	}

	default_clk_rate_hz = drm_cmn_get_default_clk_rate_hz();
	clk_rate_hz = priv->usr_clkchg_param.host.clock_rate;

	pr_debug("[%s]default_clk_rate_hz = %d, clk_rate_hz = %d\n",
			__func__, default_clk_rate_hz, clk_rate_hz);

	if (default_clk_rate_hz && clk_rate_hz) {
		if (default_clk_rate_hz != clk_rate_hz) {
			priv = display->drm_dev->dev_private;
			rc = msm_atomic_update_panel_timing(display,
				&priv->usr_clkchg_param);
		}
	}

	return rc;
}
#endif /* CONFIG_SHARP_DISPLAY */
