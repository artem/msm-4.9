/* drivers/gpu/drm/msm/sharp/drm_mfr_pwrsave.c  (Display Driver)
 *
 * Copyright (C) 2017 SHARP CORPORATION
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/* ------------------------------------------------------------------------- */
/* INCLUDE FILES                                                             */
/* ------------------------------------------------------------------------- */
#define pr_fmt(fmt)	"drm_mfr:[%s] " fmt, __func__
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>

#include "drm_mfr.h"
#include "drm_mfr_pwrsave.h"
#include <drm/drmP.h>

/* ------------------------------------------------------------------------- */
/* MACROS                                                                    */
/* ------------------------------------------------------------------------- */
#define DRMPSV_DBG(fmt, ...) DRM_DEBUG_MFR(fmt, ##__VA_ARGS__)
#define DRMPSV_ERR(fmt, ...) pr_err(fmt"\n", ##__VA_ARGS__)

#define WAIT_FOR_PSV_ASYNC_US (100*1000)

/* ------------------------------------------------------------------------- */
/* STRUCTURE                                                                 */
/* ------------------------------------------------------------------------- */
struct drm_psv_ctx {
	const struct drm_mfr_psv_controller *psv_ctrl;

	struct drm_psv_req req;
	struct drm_psv_req cur;
	bool requested;
	bool should_stop;
	bool waiting;
	bool processing;

	spinlock_t psv_lock;
	wait_queue_head_t psv_wq;
	struct completion psv_comp;

	struct pm_qos_request pm_qos;

	struct task_struct *psv_task;
};

/* ------------------------------------------------------------------------- */
/* FUNCTION                                                                  */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_psv_validate_interface(
				const struct drm_mfr_psv_controller *psv_ctrl);
static int drm_mfr_psv_has_event(struct drm_psv_ctx *psv_ctx);
static int drm_mfr_psv_qos_ctrl(struct drm_psv_ctx *psv_ctx, int en);
static int drm_mfr_psv_thread(void *data);

/* ------------------------------------------------------------------------- */
/* EXTERNAL FUNCTION                                                         */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* CONTEXT                                                                   */
/* ------------------------------------------------------------------------- */
//static struct pm_qos_request drm_mfr_psv_pm_qos;

static int drm_mfr_psv_qos_enable_value = 1;
//module_param(drm_mfr_psv_qos_enable_value, int, 0600);

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
struct drm_psv_ctx *drm_mfr_psv_create_ctx(void)
{
	struct drm_psv_ctx *psv_ctx = NULL;

	psv_ctx = kzalloc(sizeof(*psv_ctx), GFP_KERNEL);
	if (!psv_ctx) {
		DRMPSV_ERR("failed to alloc psv_ctx");
		return NULL;
	}
	spin_lock_init(&psv_ctx->psv_lock);
	init_waitqueue_head(&psv_ctx->psv_wq);
	init_completion(&psv_ctx->psv_comp);
	return psv_ctx;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void drm_mfr_psv_destroy_ctx(struct drm_psv_ctx *psv_ctx)
{
	if (psv_ctx) {
		kfree(psv_ctx);
	}
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_mfr_psv_start(struct drm_psv_ctx *psv_ctx,
					struct drm_psv_req *init_states)
{
	bool failed = false;
	int ret = 0;
	if (!psv_ctx->psv_ctrl) {
		psv_ctx->psv_ctrl = drm_get_psv_controller();
		if (!drm_mfr_psv_validate_interface(psv_ctx->psv_ctrl)) {
			psv_ctx->psv_ctrl = NULL;
			failed = true;
			ret = -ENODEV;
		}
	}

	memcpy(&psv_ctx->req, init_states, sizeof(psv_ctx->req));
	memcpy(&psv_ctx->cur, init_states, sizeof(psv_ctx->req));
	DRMPSV_DBG(" is called: "
		"init_states vsync_intr = %d, underrun_intr = %d, "
		"clocks = %d, qos = %d",
		init_states->vsync_intr, init_states->underrun_intr,
		init_states->clocks, init_states->qos);

	reinit_completion(&psv_ctx->psv_comp);

	if (!failed) {
		psv_ctx->pm_qos.type = PM_QOS_REQ_AFFINE_CORES;
		psv_ctx->pm_qos.cpus_affine.bits[0] = 0x0f; /* little cluster(CPU0-3) */
		//psv_ctx->pm_qos.cpus_affine.bits[0] = 0xff; /* all cluster(CPU0-3, 4-7) */
		pm_qos_add_request(&psv_ctx->pm_qos, PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);

		psv_ctx->psv_task =
			kthread_run(drm_mfr_psv_thread,
					(void *)psv_ctx, "psv_task");
		if (IS_ERR(psv_ctx->psv_task)) {
			DRMPSV_ERR("kthread_run returns %d\n",
				(int)PTR_ERR(psv_ctx->psv_task));
			psv_ctx->psv_task = NULL;
			ret = -ENOMEM;
			pm_qos_remove_request(&psv_ctx->pm_qos);
		}
	}
	return ret;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void drm_mfr_psv_stop(struct drm_psv_ctx *psv_ctx,
					struct drm_psv_req *stop_states)
{
	DRMPSV_DBG(": stop_states vsync_intr = %d, underrun_intr = %d, "
		"clocks = %d, qos = %d",
		stop_states->vsync_intr, stop_states->underrun_intr,
		stop_states->clocks, stop_states->qos);
	// request all on.. and async.
	drm_mfr_psv_req_clocks(psv_ctx, stop_states->clocks);
	drm_mfr_psv_req_vsync_intr(psv_ctx, stop_states->vsync_intr);
	drm_mfr_psv_req_underrun_intr(psv_ctx, stop_states->underrun_intr);
	drm_mfr_psv_req_qos(psv_ctx, stop_states->qos);
	drm_mfr_psv_req_commit_sync(psv_ctx);

	// stop kernel-thread
	psv_ctx->should_stop = true;
	if (psv_ctx->psv_task) {
		kthread_stop(psv_ctx->psv_task);
		psv_ctx->psv_task = NULL;
		pm_qos_remove_request(&psv_ctx->pm_qos);
	}
	psv_ctx->should_stop = false;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_psv_validate_interface(
				const struct drm_mfr_psv_controller *psv_ctrl)
{
	if (!psv_ctrl)
		goto invalidate_interface;

	if (!psv_ctrl->vsync_intr_ctrl)
		goto invalidate_interface;

	if (!psv_ctrl->underrun_intr_ctrl)
		goto invalidate_interface;

	if (!psv_ctrl->clocks_ctrl) {
		goto invalidate_interface;
	}

	return true;
invalidate_interface:
	DRMPSV_ERR(": psv_ctrl is invalidate interface!!"
				" can't control pwrsave");
	return false;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_psv_has_event(struct drm_psv_ctx *psv_ctx)
{
	return psv_ctx->requested
		|| psv_ctx->waiting
		|| psv_ctx->should_stop;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_psv_qos_ctrl(struct drm_psv_ctx *psv_ctx, int en)
{
	int request_val;
	if (en) {
		request_val = drm_mfr_psv_qos_enable_value;
	} else {
		request_val = -1;
	}
	DRMPSV_DBG(": call pm_qos_update_request(,%d)\n", request_val);
	pm_qos_update_request(&psv_ctx->pm_qos, request_val);

	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
// _OPRATESTATE: PWR_ON/PWR_OFF
// _FLD: 	: vsync_intr/clocks	(drm_psv_req menbers)
// _FLD##_ctrl	: vsync_intr_ctrl/clocks_ctrl(psv_ctrl functions)
// _PSVCTX	: psv_ctx
// _PSVCTRL	: psv_ctrl

// this macro should use by drm_mfr_psv_thread();
#define RSC_CTRL_OPERATION(_OPERATE, _FLD, _PSVCTX, _PSVCTRL)			\
	do {									\
		if ((_PSVCTX->cur._FLD != last._FLD)				\
				&& (_PSVCTX->cur._FLD == _OPERATE)) {		\
			DRMPSV_DBG(": update " #_FLD " to %d from %d",		\
					_PSVCTX->cur._FLD, last._FLD);		\
			_PSVCTRL->_FLD##_ctrl(_PSVCTRL->priv_data,		\
					_PSVCTX->cur._FLD);			\
		}								\
	} while(0)

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static int drm_mfr_psv_thread(void *data)
{
	unsigned long flags = 0;
	struct drm_psv_req last;
	struct drm_psv_ctx *psv_ctx = data;
	bool update = false;
	struct sched_param param;
	const struct drm_mfr_psv_controller *psv_ctrl = psv_ctx->psv_ctrl;
	int ret = 0;
	DRMPSV_DBG("is started");
	param.sched_priority = 16;
	ret = sched_setscheduler_nocheck(current, SCHED_FIFO, &param);

	while (!kthread_should_stop()) {
		if (wait_event_interruptible(psv_ctx->psv_wq,
					drm_mfr_psv_has_event(psv_ctx))) {
			continue;
		}

		DRMPSV_DBG(": >>>");
		spin_lock_irqsave(&psv_ctx->psv_lock, flags);
		psv_ctx->processing = true;
		update = memcmp(&psv_ctx->req, &psv_ctx->cur,
						sizeof(psv_ctx->req));
		if (update) {
			memcpy(&last, &psv_ctx->cur, sizeof(last));
			memcpy(&psv_ctx->cur, &psv_ctx->req,
						sizeof(psv_ctx->cur));
		}
		psv_ctx->requested = false;
		spin_unlock_irqrestore(&psv_ctx->psv_lock, flags);

		if (!update) {
			DRMPSV_DBG("havn't changing params. "
				"state: requested = %d, waiting = %d, "
				"should_stop = %d",
				psv_ctx->requested,
				psv_ctx->waiting, psv_ctx->should_stop);
		} else {
			// enable qos ifneeded.
			if ((psv_ctx->cur.qos != last.qos) &&
				(psv_ctx->cur.qos == PWR_ON)) {
				drm_mfr_psv_qos_ctrl(psv_ctx, psv_ctx->cur.qos);
			}

			// disable intrs ifneeded
			RSC_CTRL_OPERATION(PWR_OFF, vsync_intr,
							psv_ctx, psv_ctrl);
			RSC_CTRL_OPERATION(PWR_OFF, underrun_intr,
							psv_ctx, psv_ctrl);

			// disable/enable clocks/power ifneeded
			RSC_CTRL_OPERATION(PWR_OFF, clocks, psv_ctx, psv_ctrl);
			RSC_CTRL_OPERATION(PWR_ON,  clocks, psv_ctx, psv_ctrl);

			// enable intrs ifneeded
			RSC_CTRL_OPERATION(PWR_ON,  vsync_intr,
							psv_ctx, psv_ctrl);
			RSC_CTRL_OPERATION(PWR_ON,  underrun_intr,
							psv_ctx, psv_ctrl);

			// disable qos ifneeded.
			if ((psv_ctx->cur.qos != last.qos) &&
				(psv_ctx->cur.qos == PWR_OFF)) {
				drm_mfr_psv_qos_ctrl(psv_ctx, psv_ctx->cur.qos);
			}
		}

		spin_lock_irqsave(&psv_ctx->psv_lock, flags);
		if (psv_ctx->waiting && !psv_ctx->requested) {
			DRMPSV_DBG(": send complete_all for waiting threads");
			complete_all(&psv_ctx->psv_comp);
			psv_ctx->waiting = false;
		}
		psv_ctx->processing = false;
		spin_unlock_irqrestore(&psv_ctx->psv_lock, flags);

		DRMPSV_DBG(": <<<");
	}

	DRMPSV_DBG("is stopped");
	return 0;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void drm_mfr_psv_req_vsync_intr(struct drm_psv_ctx *psv_ctx, int en)
{
	unsigned long flags;
	spin_lock_irqsave(&psv_ctx->psv_lock, flags);
	psv_ctx->req.vsync_intr = en ? PWR_ON : PWR_OFF;
	spin_unlock_irqrestore(&psv_ctx->psv_lock, flags);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void drm_mfr_psv_req_underrun_intr(struct drm_psv_ctx *psv_ctx, int en)
{
	unsigned long flags;
	spin_lock_irqsave(&psv_ctx->psv_lock, flags);
	psv_ctx->req.underrun_intr = en ? PWR_ON : PWR_OFF;
	spin_unlock_irqrestore(&psv_ctx->psv_lock, flags);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void drm_mfr_psv_req_qos(struct drm_psv_ctx *psv_ctx, int en)
{
	unsigned long flags;
	spin_lock_irqsave(&psv_ctx->psv_lock, flags);
	psv_ctx->req.qos = en ? PWR_ON : PWR_OFF;
	spin_unlock_irqrestore(&psv_ctx->psv_lock, flags);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void drm_mfr_psv_req_clocks(struct drm_psv_ctx *psv_ctx, int en)
{
	unsigned long flags;
	spin_lock_irqsave(&psv_ctx->psv_lock, flags);
	psv_ctx->req.clocks = en ? PWR_ON : PWR_OFF;
	spin_unlock_irqrestore(&psv_ctx->psv_lock, flags);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
static bool drm_mfr_psv_req_commit_locked(struct drm_psv_ctx *psv_ctx)
{
	bool update = false;
	update = memcmp(&psv_ctx->req, &psv_ctx->cur, sizeof(psv_ctx->req));
	if (update) {
		psv_ctx->requested = true;
		wake_up_interruptible(&psv_ctx->psv_wq);
	}
	return update;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_mfr_psv_req_commit(struct drm_psv_ctx *psv_ctx)
{
	unsigned long flags;
	int rc = 0;
	if (!psv_ctx->psv_task) {
		return -ENOMEM;
	}
	spin_lock_irqsave(&psv_ctx->psv_lock, flags);
	drm_mfr_psv_req_commit_locked(psv_ctx);
	spin_unlock_irqrestore(&psv_ctx->psv_lock, flags);
	return rc;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_mfr_psv_req_commit_sync(struct drm_psv_ctx *psv_ctx)
{
	bool need_wait = false;
	unsigned long flags;
	int to = 1;
	int rc = 0;
	if (!psv_ctx->psv_task) {
		return -ENOMEM;
	}

	spin_lock_irqsave(&psv_ctx->psv_lock, flags);
	need_wait = drm_mfr_psv_req_commit_locked(psv_ctx);
	need_wait = true;
	need_wait &= (psv_ctx->psv_task != NULL);
	if (need_wait && !psv_ctx->waiting) {
		psv_ctx->waiting = true;
		reinit_completion(&psv_ctx->psv_comp);
		wake_up_interruptible(&psv_ctx->psv_wq);
	}
	spin_unlock_irqrestore(&psv_ctx->psv_lock, flags);
	if (need_wait) {
		DRMPSV_DBG(": wait >>>");
		to = wait_for_completion_timeout(
			&psv_ctx->psv_comp, WAIT_FOR_PSV_ASYNC_US);
		if (to <= 0) {
			DRMPSV_ERR(": failed to wait async psv_thread");
			rc = -ETIMEDOUT;
		}
		DRMPSV_DBG(": wait <<<");
	}
	return rc;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int drm_mfr_psv_is_clocks_enabled(struct drm_psv_ctx *psv_ctx)
{
	unsigned long flags;
	int ret = 0;
	int wait_retry = 0;
	spin_lock_irqsave(&psv_ctx->psv_lock, flags);

	if (!psv_ctx->processing) {
		ret = (psv_ctx->cur.clocks == PWR_ON);
	} else if ((psv_ctx->req.clocks == PWR_ON) &&
				(psv_ctx->req.clocks != psv_ctx->cur.clocks)) {
		for (wait_retry = 0; wait_retry < 2; ++wait_retry) {
			spin_unlock_irqrestore(&psv_ctx->psv_lock, flags);
			udelay(300);
			spin_lock_irqsave(&psv_ctx->psv_lock, flags);
			ret = (!psv_ctx->processing &&
				psv_ctx->cur.clocks == PWR_ON);
			if (ret)
			    break;
		}
	} else /* (psv_ctx->req.clocks == PWR_OFF) */ {
		// call clock algorithm has some bugs...
		DRMPSV_ERR(": isn't requested clocks on...");
	}

	spin_unlock_irqrestore(&psv_ctx->psv_lock, flags);
	return ret;
}

MODULE_AUTHOR("SHARP CORPORATION");
MODULE_DESCRIPTION("DRM MFR PSV Driver");
MODULE_LICENSE("GPL");