/* drivers/input/touchscreen/shtps/sy3000/shtps_rmi.h
 *
 * Copyright (c) 2017, Sharp. All rights reserved.
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
#ifndef __SHTPS_RMI_H__
#define __SHTPS_RMI_H__
/* --------------------------------------------------------------------------- */
/* COORDINATOR Qualcomm_CS1.1 BUILDERR MODIFY start */
/*#include <linux/wakelock.h>*/
#include <linux/ktime.h>
#include <linux/device.h>

/* A wake_lock prevents the system from entering suspend or other low power
 * states when active. If the type is set to WAKE_LOCK_SUSPEND, the wake_lock
 * prevents a full system suspend.
 */

enum {
	WAKE_LOCK_SUSPEND, /* Prevent suspend */
	WAKE_LOCK_TYPE_COUNT
};

struct wake_lock {
	struct wakeup_source ws;
};

static inline void wake_lock_init(struct wake_lock *lock, int type,
				  const char *name)
{
	wakeup_source_init(&lock->ws, name);
}

static inline void wake_lock_destroy(struct wake_lock *lock)
{
	wakeup_source_trash(&lock->ws);
}

static inline void wake_lock(struct wake_lock *lock)
{
	__pm_stay_awake(&lock->ws);
}

static inline void wake_lock_timeout(struct wake_lock *lock, long timeout)
{
	__pm_wakeup_event(&lock->ws, jiffies_to_msecs(timeout));
}

static inline void wake_unlock(struct wake_lock *lock)
{
	__pm_relax(&lock->ws);
}

static inline int wake_lock_active(struct wake_lock *lock)
{
	return lock->ws.active;
}
/* COORDINATOR Qualcomm_CS1.1 BUILDERR MODIFY end */
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/io.h>
#ifdef CONFIG_DRM_MSM
	#include <linux/notifier.h>
	#include <linux/msm_drm_notify.h>
#else
	#ifdef CONFIG_FB
		#include <linux/notifier.h>
		#include <linux/fb.h>
	#endif
#endif

#include <linux/input/shtps_dev.h>
#include <soc/qcom/sh_smem.h>
//#include <sharp/sh_boot_manager.h>

#include "shtps_cfg.h"
#include "shtps_fwctl.h"
#include "shtps_filter.h"
#include "shtps_rmi_debug.h"

#if defined(SHTPS_DEF_FWCTL_PROTOCOL_I2C)
	#include "shtps_i2c.h"
#else
	#include "shtps_spi.h"
#endif /* SHTPS_DEF_FWCTL_PROTOCOL_I2C */

/* ===================================================================================
 * Debug
 */

/* ===================================================================================
 * Common
 */
#define SPI_ERR_CHECK(check, label) \
	if((check)) goto label

#define SHTPS_POSTYPE_X (0)
#define SHTPS_POSTYPE_Y (1)

#define SHTPS_TOUCH_CANCEL_COORDINATES_X (0)
#define SHTPS_TOUCH_CANCEL_COORDINATES_Y (9999)

#define SHTPS_ABS_CALC(A,B)	(((A)>(B)) ? ((A)-(B)) : ((B)-(A)))

/* ===================================================================================
 * [ STFLIB kernel event request ]
 */
#define SHTPS_DEF_STFLIB_KEVT_SIZE_MAX		0xFFFF
#define SHTPS_DEF_STFLIB_KEVT_DUMMY			0x0000
#define SHTPS_DEF_STFLIB_KEVT_SCANRATE_MODE	0x0001
#define SHTPS_DEF_STFLIB_KEVT_CHARGER_ARMOR	0x0002
#define SHTPS_DEF_STFLIB_KEVT_CLEAR_REQUEST	0x0003
#define SHTPS_DEF_STFLIB_KEVT_TIMEOUT		0x0004
#define SHTPS_DEF_STFLIB_KEVT_CHANGE_PARAM	0x0005
#define SHTPS_DEF_STFLIB_KEVT_CALIBRATION	0x0006
#define SHTPS_DEF_STFLIB_KEVT_CHANGE_PANEL_SIZE	0x0007
#define SHTPS_DEF_STFLIB_KEVT_COVER_MODE	0x0008

#define SHTPS_DEF_STFLIB_KEVT_SCANRATE_MODE_NORMAL	0		/* Normal */
#define SHTPS_DEF_STFLIB_KEVT_SCANRATE_MODE_LOW		1		/* Low */
#define SHTPS_DEF_STFLIB_KEVT_SCANRATE_MODE_HIGH	2		/* High */

#define SHTPS_DEF_STFLIB_KEVT_CLEAR_REQUEST_SLEEP	0		/* SLEEP */
#define SHTPS_DEF_STFLIB_KEVT_CLEAR_REQUEST_REZERO	1		/* REZERO */

/* ===================================================================================
 * proximity sensor cooperation
 */
#if defined(SHTPS_PROXIMITY_SUPPORT_ENABLE)
	#if defined(SHTPS_PROXIMITY_USE_API)
		#include <sharp/proximity.h>
		#define SHTPS_PROXIMITY_NEAR			SH_PROXIMITY_NEAR
		#define SHTPS_PROXIMITY_FAR				SH_PROXIMITY_FAR
	#else
		#define SHTPS_PROXIMITY_DEVICE_NAME_STR	"proximity_sensor"
		#define SHTPS_PROXIMITY_ERROR_STR		"-1"
		#define SHTPS_PROXIMITY_NEAR			0
		#define SHTPS_PROXIMITY_FAR				7
	#endif /* SHTPS_PROXIMITY_USE_API */
#endif /* SHTPS_PROXIMITY_SUPPORT_ENABLE */

/* ===================================================================================
 * Structure / enum
 */
struct rmi_pdt {
	u8							queryBaseAddr;
	u8							commandBaseAddr;
	u8							controlBaseAddr;
	u8							dataBaseAddr;
	u8							interruptSrcCount;
	u8							functionNumber;
};

struct shtps_touch_state {
	u8							numOfFingers;
};

struct shtps_irq_info{
	int							irq;
	u8							state;
	u8							wake;
};

struct shtps_state_info{
	int							state;
	int							mode;
	int							starterr;
	unsigned long				starttime;
	int							startup_min_time;
};

struct shtps_loader_info{
	int							ack;
	wait_queue_head_t			wait_ack;
};

struct shtps_diag_info{
	u8							pos_mode;
	u8							tm_mode;
	u8							tm_data[SHTPS_TM_TXNUM_MAX * SHTPS_TM_RXNUM_MAX * 2];
	int							event;
	int							tm_ack;
	int							tm_stop;
	wait_queue_head_t			wait;
	wait_queue_head_t			tm_wait_ack;
};

struct shtps_facetouch_info{
	int							mode;
	int							state;
	int							detect;
	int							palm_thresh;
	int							wake_sig;
	u8							palm_det;
	u8							touch_num;
	wait_queue_head_t			wait_off;
	
	u8							wakelock_state;
	struct wake_lock            wake_lock;
	struct pm_qos_request		qos_cpu_latency;
};

struct shtps_offset_info{
	int							enabled;
	u16							base[5];
	signed short				diff[12];
};

struct shtps_polling_info{
	int							boot_rezero_flag;
	int							stop_margin;
	int							stop_count;
	int							single_fingers_count;
	int							single_fingers_max;
	u8							single_fingers_enable;
};

struct shtps_lpwg_ctrl{		//**********	SHTPS_LPWG_MODE_ENABLE
	u8							lpwg_switch;
	u8							lpwg_state;
	u8							lpwg_sweep_on_req_state;
	u8							lpwg_double_tap_req_state;
	#if defined(SHTPS_ALWAYS_ON_DISPLAY_ENABLE)
		u8						lpwg_double_tap_aod_req_state;
	#endif /* SHTPS_ALWAYS_ON_DISPLAY_ENABLE */
	u8							lpwg_set_state;
	u8							is_notified;
	u8							notify_enable;

	struct wake_lock			wake_lock;
	struct pm_qos_request		pm_qos_lock_idle;
	signed long					pm_qos_idle_value;
	unsigned long				notify_time;
	struct delayed_work			notify_interval_delayed_work;
	u8							block_touchevent;
	unsigned long				wakeup_time;
	u8							tu_rezero_req;
	
	#if defined( SHTPS_LPWG_GRIP_SUPPORT_ENABLE )
		u8							grip_state;
	#endif /* SHTPS_LPWG_GRIP_SUPPORT_ENABLE */

	#if defined( SHTPS_HOST_LPWG_MODE_ENABLE )
		struct shtps_touch_info		pre_info;
		unsigned long				swipe_check_time;
	#endif /* SHTPS_HOST_LPWG_MODE_ENABLE */

};


enum{	//**********	SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE
	SHTPS_DETER_SUSPEND_SPI_PROC_IRQ = 0x00,
	SHTPS_DETER_SUSPEND_SPI_PROC_SETSLEEP,
	SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETLPWG,
	SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETLPMODE,
	SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETCONLPMODE,
	SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETLCDBRIGHTLPMODE,
	SHTPS_DETER_SUSPEND_SPI_PROC_OPEN,
	SHTPS_DETER_SUSPEND_SPI_PROC_CLOSE,
	SHTPS_DETER_SUSPEND_SPI_PROC_ENABLE,
	SHTPS_DETER_SUSPEND_SPI_PROC_GRIP,
	SHTPS_DETER_SUSPEND_SPI_PROC_COVER,
	SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETLPWG_DOUBLETAP,
	SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SETGLOVE,
	SHTPS_DETER_SUSPEND_SPI_PROC_IOCTL_SET_HIGH_REPORT_MODE,

	SHTPS_DETER_SUSPEND_SPI_PROC_NUM,
};

struct shtps_deter_suspend_spi{	//**********	SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE
	u8							suspend;
	struct work_struct			pending_proc_work;
	u8							wake_lock_state;
	struct wake_lock			wake_lock;
	struct pm_qos_request		pm_qos_lock_idle;
	
	#ifdef SHTPS_DEVELOP_MODE_ENABLE
		struct delayed_work			pending_proc_work_delay;
	#endif /* SHTPS_DEVELOP_MODE_ENABLE */
	
	struct shtps_deter_suspend_spi_pending_info{
		u8						pending;
		u8						param;
	} pending_info[SHTPS_DETER_SUSPEND_SPI_PROC_NUM];

	u8							suspend_irq_state;
	u8							suspend_irq_wake_state;
	u8							suspend_irq_detect;
};

#ifdef CONFIG_OF
	; // nothing
#else /* CONFIG_OF */
struct shtps_platform_data {
	int (*setup)(struct device *);
	void (*teardown)(struct device *);
	int gpio_rst;
};
#endif /* CONFIG_OF */

/* -------------------------------------------------------------------------- */
struct shtps_rmi_spi {
	struct input_dev*			input;
	int							rst_pin;
	int							tpin;
	#ifdef CONFIG_OF
		struct pinctrl				*pinctrl;
		struct pinctrl_state		*tpin_state_active;
		struct pinctrl_state		*tpin_state_suspend;
		#if defined( SHTPS_CHECK_LCD_RESOLUTION_ENABLE )
			int							lcd_det_gpio;
			struct pinctrl_state		*lcd_det_state_active;
			struct pinctrl_state		*lcd_det_state_suspend;
		#endif /* SHTPS_CHECK_LCD_RESOLUTION_ENABLE */
	#endif /* CONFIG_OF */
	struct shtps_irq_info		irq_mgr;
	#ifdef CONFIG_DRM_MSM
		struct notifier_block		drm_notif;
	#else
		#ifdef CONFIG_FB
			struct notifier_block		fb_notif;
		#endif
	#endif

	struct shtps_touch_info		fw_report_info;
	struct shtps_touch_info		fw_report_info_store;
	struct shtps_touch_info		report_info;

	struct shtps_state_info		state_mgr;
	struct shtps_loader_info	loader;
	struct shtps_diag_info		diag;
	struct shtps_facetouch_info	facetouch;
	struct shtps_polling_info	poll_info;
	struct shtps_touch_state	touch_state;
	wait_queue_head_t			wait_start;
	struct delayed_work 		tmo_check;
	unsigned char				finger_state[3];	/* SHTPS_FINGER_MAX/4+1 */
	struct hrtimer				rezero_delayed_timer;
	struct work_struct			rezero_delayed_work;
	char						phys[32];
	struct kobject				*kobj;
	#ifdef SHTPS_DEVELOP_MODE_ENABLE
		char					stflib_param_str[100];
	#endif /* SHTPS_DEVELOP_MODE_ENABLE */


	struct shtps_offset_info	offset;

	#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
		u8							lpmode_req_state;
		u8							lpmode_continuous_req_state;
	#endif /* #if defined( SHTPS_LOW_POWER_MODE_ENABLE ) */

	#if defined(SHTPS_SYSTEM_BOOT_MODE_CHECK_ENABLE)
		u16							system_boot_mode;
	#endif /* SHTPS_SYSTEM_BOOT_MODE_CHECK_ENABLE */

	#if defined(SHTPS_LPWG_MODE_ENABLE)
		struct shtps_lpwg_ctrl		lpwg;

		struct input_dev*			input_key;
		u16							keycodes[2];
		u8							key_state;
		#if defined(SHTPS_PROXIMITY_SUPPORT_ENABLE)
			int							lpwg_proximity_get_data;
			struct wake_lock            wake_lock_proximity;
			struct pm_qos_request		pm_qos_lock_idle_proximity;
			struct delayed_work			proximity_check_delayed_work;
		#endif /* SHTPS_PROXIMITY_SUPPORT_ENABLE */
	#endif /* SHTPS_LPWG_MODE_ENABLE */

	#if defined(SHTPS_MULTI_FW_ENABLE)
		u8							multi_fw_type;
	#endif /* SHTPS_MULTI_FW_ENABLE */

	#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
		struct shtps_deter_suspend_spi		deter_suspend_spi;
	#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */

	#if defined(SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE)
		u8							wakeup_touch_event_inhibit_state;
		u8							wakeup_fail_touch_event_reject_rezero_exec;
	#endif /* SHTPS_WAKEUP_FAIL_TOUCH_EVENT_REJECTION_ENABLE */

	#if defined(SHTPS_COVER_ENABLE)
		u8							cover_state;
		#if defined(SHTPS_DEF_COVER_CHANGE_FINGER_AMP_THRESH_ENABLE)
			u8							cover_change_finger_amp_thresh_for_film;
		#endif /* SHTPS_DEF_COVER_CHANGE_FINGER_AMP_THRESH_ENABLE */
	#endif /* SHTPS_COVER_ENABLE */

	#if defined(SHTPS_CTRL_FW_REPORT_RATE)
		u8							fw_report_rate_cur;
		u8							fw_report_rate_req_state;
	#endif /* SHTPS_CTRL_FW_REPORT_RATE */

	#if defined(SHTPS_GLOVE_DETECT_ENABLE)
		u8							glove_enable_state;
	#endif /* SHTPS_GLOVE_DETECT_ENABLE */

	#if defined(SHTPS_PROXIMITY_SUPPORT_ENABLE)
		int							proximity_input_index;
	#endif /* SHTPS_PROXIMITY_SUPPORT_ENABLE */

	/* ------------------------------------------------------------------------ */
	/* acync */
	struct workqueue_struct		*workqueue_p;
	struct work_struct			work_data;
	struct list_head			queue;
	spinlock_t					queue_lock;
	struct shtps_req_msg		*cur_msg_p;
	u8							work_wake_lock_state;
	struct wake_lock			work_wake_lock;
	struct pm_qos_request		work_pm_qos_lock_idle;

	#if defined(SHTPS_CPU_CLOCK_CONTROL_ENABLE)
		struct shtps_cpu_clock_ctrl_info					*cpu_clock_ctrl_p;
	#endif /* SHTPS_CPU_CLOCK_CONTROL_ENABLE */

	#if defined(SHTPS_CPU_IDLE_SLEEP_CONTROL_ENABLE)
		struct shtps_cpu_idle_sleep_ctrl_info				*cpu_idle_sleep_ctrl_p;
	#endif /* SHTPS_CPU_IDLE_SLEEP_CONTROL_ENABLE */

	#if defined(SHTPS_CPU_SLEEP_CONTROL_FOR_FWUPDATE_ENABLE)
		struct shtps_cpu_sleep_ctrl_fwupdate_info			*cpu_sleep_ctrl_fwupdate_p;
	#endif /* SHTPS_CPU_SLEEP_CONTROL_FOR_FWUPDATE_ENABLE */

	#if defined(SHTPS_DEF_RECORD_LOG_FILE_ENABLE)
		struct shtps_record_log_file_info					*record_log_file_info_p;
	#endif /* SHTPS_DEF_RECORD_LOG_FILE_ENABLE */

	/* ------------------------------------------------------------------------ */
	struct device				*ctrl_dev_p;
	struct shtps_fwctl_info		*fwctl_p;
};

/* ----------------------------------------------------------------------------
*/
enum{
	SHTPS_FWTESTMODE_V01 = 0x00,
	SHTPS_FWTESTMODE_V02,
	SHTPS_FWTESTMODE_V03,
};

enum{
	SHTPS_REZERO_REQUEST_REZERO				= 0x01,
	SHTPS_REZERO_REQUEST_WAKEUP_REZERO		= 0x02,
};

enum{
	SHTPS_REZERO_HANDLE_EVENT_MTD = 0,
	SHTPS_REZERO_HANDLE_EVENT_TOUCH,
	SHTPS_REZERO_HANDLE_EVENT_TOUCHUP,
};

enum{
	SHTPS_REZERO_TRIGGER_BOOT = 0,
	SHTPS_REZERO_TRIGGER_WAKEUP,
	SHTPS_REZERO_TRIGGER_ENDCALL,
};

enum{
	SHTPS_EVENT_TU,
	SHTPS_EVENT_TD,
	SHTPS_EVENT_DRAG,
	SHTPS_EVENT_MTDU,
};

enum{
	SHTPS_TOUCH_STATE_NO_TOUCH		= 0x00,
	SHTPS_TOUCH_STATE_FINGER		= 0x01,
	SHTPS_TOUCH_STATE_PEN			= 0x02,
	SHTPS_TOUCH_STATE_PALM			= 0x03,
	SHTPS_TOUCH_STATE_UNKNOWN		= 0x04,
	SHTPS_TOUCH_STATE_HOVER			= 0x05,
	SHTPS_TOUCH_STATE_GLOVE			= 0x06,
	SHTPS_TOUCH_STATE_NARROW_OBJECT	= 0x07,
	SHTPS_TOUCH_STATE_HAND_EDGE		= 0x08,

	SHTPS_TOUCH_STATE_COVER			= 0x0A,
	SHTPS_TOUCH_STATE_STYLUS		= 0x0B,
	SHTPS_TOUCH_STATE_ERASER		= 0x0C,
	SHTPS_TOUCH_STATE_SMALL_OBJECT	= 0x0D,
};

enum{
	SHTPS_STARTUP_SUCCESS,
	SHTPS_STARTUP_FAILED
};

enum{
	SHTPS_IRQ_WAKE_DISABLE,
	SHTPS_IRQ_WAKE_ENABLE,
};

enum{
	SHTPS_IRQ_STATE_DISABLE,
	SHTPS_IRQ_STATE_ENABLE,
};

enum{
	SHTPS_MODE_NORMAL,
	SHTPS_MODE_LOADER,
};

enum{
	SHTPS_EVENT_START,
	SHTPS_EVENT_STOP,
	SHTPS_EVENT_SLEEP,
	SHTPS_EVENT_WAKEUP,
	SHTPS_EVENT_STARTLOADER,
	SHTPS_EVENT_STARTTM,
	SHTPS_EVENT_STOPTM,
	SHTPS_EVENT_FACETOUCHMODE_ON,
	SHTPS_EVENT_FACETOUCHMODE_OFF,
	SHTPS_EVENT_INTERRUPT,
	SHTPS_EVENT_TIMEOUT,
};

enum{
	SHTPS_STATE_IDLE,
	SHTPS_STATE_WAIT_WAKEUP,
	SHTPS_STATE_WAIT_READY,
	SHTPS_STATE_ACTIVE,
	SHTPS_STATE_BOOTLOADER,
	SHTPS_STATE_FACETOUCH,
	SHTPS_STATE_FWTESTMODE,
	SHTPS_STATE_SLEEP,
	SHTPS_STATE_SLEEP_FACETOUCH,
};

enum{
	SHTPS_IRQ_FLASH		= 0x01,
	SHTPS_IRQ_STATE		= 0x02,
	SHTPS_IRQ_ABS 		= 0x04,
	SHTPS_IRQ_ANALOG 	= 0x08,
	SHTPS_IRQ_BUTTON 	= 0x10,
	SHTPS_IRQ_SENSOR 	= 0x20,
	SHTPS_IRQ_ALL		= (  SHTPS_IRQ_FLASH
							| SHTPS_IRQ_STATE
							| SHTPS_IRQ_ABS
							| SHTPS_IRQ_ANALOG
							| SHTPS_IRQ_BUTTON
							| SHTPS_IRQ_SENSOR),
};

enum{
	SHTPS_LPMODE_TYPE_NON_CONTINUOUS = 0,
	SHTPS_LPMODE_TYPE_CONTINUOUS,
};

enum{
	SHTPS_LPMODE_REQ_NONE		= 0x00,
	SHTPS_LPMODE_REQ_COMMON		= 0x01,
	SHTPS_LPMODE_REQ_ECO		= 0x02,
	SHTPS_LPMODE_REQ_LCD_BRIGHT	= 0x04,
};

enum{
	SHTPS_FUNC_REQ_EVENT_OPEN = 0,
	SHTPS_FUNC_REQ_EVENT_CLOSE,
	SHTPS_FUNC_REQ_EVENT_ENABLE,
	SHTPS_FUNC_REQ_EVENT_DISABLE,
	SHTPS_FUNC_REQ_EVENT_CHECK_CRC_ERROR,
	SHTPS_FUNC_REQ_EVENT_PROXIMITY_CHECK,
	SHTPS_FUNC_REQ_EVENT_LCD_ON,
	SHTPS_FUNC_REQ_EVENT_LCD_OFF,
	SHTPS_FUNC_REQ_EVENT_GRIP_ON,
	SHTPS_FUNC_REQ_EVENT_GRIP_OFF,
	SHTPS_FUNC_REQ_EVENT_COVER_ON,
	SHTPS_FUNC_REQ_EVENT_COVER_OFF,
	SHTPS_FUNC_REQ_BOOT_FW_UPDATE,
};

enum{
	SHTPS_DRAG_DIR_NONE = 0,
	SHTPS_DRAG_DIR_PLUS,
	SHTPS_DRAG_DIR_MINUS,
};

enum{
	SHTPS_LPWG_DETECT_GESTURE_TYPE_NONE			= 0x00,
	SHTPS_LPWG_DETECT_GESTURE_TYPE_DOUBLE_TAP	= 0x01,
	SHTPS_LPWG_DETECT_GESTURE_TYPE_SWIPE		= 0x02,
};

enum{
	SHTPS_GESTURE_TYPE_NONE						= 0x00,
	SHTPS_GESTURE_TYPE_ONE_FINGER_SINGLE_TAP	= 0x01,
	SHTPS_GESTURE_TYPE_ONE_FINGER_TAP_AND_HOLD	= 0x02,
	SHTPS_GESTURE_TYPE_ONE_FINGER_DOUBLE_TAP	= 0x03,
	SHTPS_GESTURE_TYPE_ONE_FINGER_EARLY_TAP		= 0x04,
	SHTPS_GESTURE_TYPE_ONE_FINGER_FLICK			= 0x05,
	SHTPS_GESTURE_TYPE_ONE_FINGER_PRESS			= 0x06,
	SHTPS_GESTURE_TYPE_ONE_FINGER_SWIPE			= 0x07,
	SHTPS_GESTURE_TYPE_ONE_FINGER_CIRCLE		= 0x08,
	SHTPS_GESTURE_TYPE_ONE_FINGER_TRIANGLE		= 0x09,
	SHTPS_GESTURE_TYPE_ONE_FINGER_VEE			= 0x0A,

	SHTPS_GESTURE_TYPE_TRIPLE_TAP				= 0x0C,
	SHTPS_GESTURE_TYPE_CLICK					= 0x0D,

	SHTPS_GESTURE_TYPE_PINCH					= 0x80,
	SHTPS_GESTURE_TYPE_ROTATE					= 0x81,
};

enum{
	SHTPS_LPWG_SWEEP_ON_REQ_OFF			= 0x00,
	SHTPS_LPWG_SWEEP_ON_REQ_ON			= 0x01,
	SHTPS_LPWG_SWEEP_ON_REQ_GRIP_ONLY	= 0x02,
};

enum{
	SHTPS_LPWG_DOUBLE_TAP_REQ_OFF	= 0x00,
	SHTPS_LPWG_DOUBLE_TAP_REQ_ON	= 0x01,
};

enum{
	SHTPS_LPWG_REQ_NONE			= 0x00,
	SHTPS_LPWG_REQ_SWEEP_ON		= 0x01,
	SHTPS_LPWG_REQ_DOUBLE_TAP	= 0x02,
};

enum{
	SHTPS_LPWG_SET_STATE_OFF		= SHTPS_LPWG_REQ_NONE,
	SHTPS_LPWG_SET_STATE_SWEEP_ON	= SHTPS_LPWG_REQ_SWEEP_ON,
	SHTPS_LPWG_SET_STATE_DOUBLE_TAP	= SHTPS_LPWG_REQ_DOUBLE_TAP,
};

enum{
	SHTPS_HW_TYPE_BOARD = 0,
	SHTPS_HW_TYPE_HANDSET,
};

enum{
	SHTPS_DEV_STATE_SLEEP = 0,
	SHTPS_DEV_STATE_DOZE,
	SHTPS_DEV_STATE_ACTIVE,
	SHTPS_DEV_STATE_LPWG,
	SHTPS_DEV_STATE_LOADER,
	SHTPS_DEV_STATE_TESTMODE,
};

enum shtps_diag_tm_mode {
	SHTPS_TMMODE_NONE,
	SHTPS_TMMODE_FRAMELINE,
	SHTPS_TMMODE_BASELINE,
	SHTPS_TMMODE_BASELINE_RAW,
	SHTPS_TMMODE_HYBRID_ADC,
	SHTPS_TMMODE_ADC_RANGE,
	SHTPS_TMMODE_MOISTURE,
	SHTPS_TMMODE_MOISTURE_NO_MASK,
};

/* ----------------------------------------------------------------------------
*/
extern struct shtps_rmi_spi*	gShtps_rmi_spi;

extern int gShtps_panel_size_x;
extern int gShtps_panel_size_y;

void shtps_mutex_lock_ctrl(void);
void shtps_mutex_unlock_ctrl(void);
void shtps_mutex_lock_loader(void);
void shtps_mutex_unlock_loader(void);
void shtps_mutex_lock_proc(void);
void shtps_mutex_unlock_proc(void);
void shtps_mutex_lock_facetouch_qos_ctrl(void);
void shtps_mutex_unlock_facetouch_qos_ctrl(void);

#if defined(SHTPS_CHECK_HWID_ENABLE)
	int shtps_system_get_hw_revision(void);
	int shtps_system_get_hw_type(void);
#endif /* SHTPS_CHECK_HWID_ENABLE */

void shtps_system_set_sleep(struct shtps_rmi_spi *ts);
void shtps_system_set_wakeup(struct shtps_rmi_spi *ts);

#if defined(SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE)
	void shtps_ioctl_setlpwg_proc(struct shtps_rmi_spi *ts, u8 on);
	void shtps_ioctl_setlpmode_proc(struct shtps_rmi_spi *ts, u8 on);
	void shtps_ioctl_setconlpmode_proc(struct shtps_rmi_spi *ts, u8 on);
	void shtps_ioctl_setlcdbrightlpmode_proc(struct shtps_rmi_spi *ts, u8 on);
	int shtps_check_suspend_state(struct shtps_rmi_spi *ts, int proc, u8 param);
	void shtps_set_suspend_state(struct shtps_rmi_spi *ts);
	void shtps_clr_suspend_state(struct shtps_rmi_spi *ts);
	void shtps_ioctl_setlpwg_doubletap_proc(struct shtps_rmi_spi *ts, u8 on);
	void shtps_ioctl_setglove_proc(struct shtps_rmi_spi *ts, u8 on);
	void shtps_ioctl_set_high_report_mode_proc(struct shtps_rmi_spi *ts, u8 on);
#endif /* SHTPS_GUARANTEE_SPI_ACCESS_IN_WAKE_ENABLE */

void shtps_irq_disable(struct shtps_rmi_spi *ts);
void shtps_irq_enable(struct shtps_rmi_spi *ts);

void shtps_rezero(struct shtps_rmi_spi *ts);
void shtps_rezero_request(struct shtps_rmi_spi *ts, u8 request, u8 trigger);

int shtps_start(struct shtps_rmi_spi *ts);
void shtps_shutdown(struct shtps_rmi_spi *ts);
void shtps_reset(struct shtps_rmi_spi *ts);
int shtps_wait_startup(struct shtps_rmi_spi *ts);
int request_event(struct shtps_rmi_spi *ts, int event, int param);
void shtps_sleep(struct shtps_rmi_spi *ts, int on);
void shtps_set_startup_min_time(struct shtps_rmi_spi *ts, int time);

int shtps_fwdate(struct shtps_rmi_spi *ts, u8 *year, u8 *month);
u16 shtps_fwver(struct shtps_rmi_spi *ts);
u16 shtps_fwver_builtin(struct shtps_rmi_spi *ts);
int shtps_fwsize_builtin(struct shtps_rmi_spi *ts);
unsigned char* shtps_fwdata_builtin(struct shtps_rmi_spi *ts);
int shtps_enter_bootloader(struct shtps_rmi_spi *ts);
int shtps_lockdown_bootloader(struct shtps_rmi_spi *ts, u8* fwdata);
int shtps_flash_erase(struct shtps_rmi_spi *ts);
int shtps_flash_writeImage(struct shtps_rmi_spi *ts, u8 *fwdata);
int shtps_flash_writeConfig(struct shtps_rmi_spi *ts, u8 *fwdata);
int shtps_fw_update(struct shtps_rmi_spi *ts, const unsigned char *fw_data);
int shtps_fwupdate_builtin_enable(struct shtps_rmi_spi *ts);

int shtps_get_tm_rxsize(struct shtps_rmi_spi *ts);
int shtps_get_tm_txsize(struct shtps_rmi_spi *ts);
int shtps_baseline_offset_disable(struct shtps_rmi_spi *ts);
void shtps_read_tmdata(struct shtps_rmi_spi *ts, u8 mode);

int shtps_get_fingermax(struct shtps_rmi_spi *ts);
int shtps_get_diff(unsigned short pos1, unsigned short pos2, unsigned long factor);
int shtps_get_fingerwidth(struct shtps_rmi_spi *ts, int num, struct shtps_touch_info *info);
void shtps_set_eventtype(u8 *event, u8 type);
void shtps_report_touch_on(struct shtps_rmi_spi *ts, int finger, int x, int y, int w, int wx, int wy, int z);
void shtps_report_touch_off(struct shtps_rmi_spi *ts, int finger, int x, int y, int w, int wx, int wy, int z);
void shtps_event_report(struct shtps_rmi_spi *ts, struct shtps_touch_info *info, u8 event);
void shtps_read_touchevent(struct shtps_rmi_spi *ts, int state);
void shtps_event_force_touchup(struct shtps_rmi_spi *ts);
#ifdef CONFIG_DRM_MSM
	int shtps_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
#else
	#ifdef CONFIG_FB
		int shtps_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data);
	#endif
#endif

#if defined(SHTPS_LPWG_MODE_ENABLE)
	void shtps_set_lpwg_mode_on(struct shtps_rmi_spi *ts);
	void shtps_set_lpwg_mode_off(struct shtps_rmi_spi *ts);
	int shtps_is_lpwg_active(struct shtps_rmi_spi *ts);
	#if defined(SHTPS_PROXIMITY_SUPPORT_ENABLE)
		void shtps_notify_cancel_wakeup_event(struct shtps_rmi_spi *ts);
	#endif /* SHTPS_PROXIMITY_SUPPORT_ENABLE */
	#if defined(SHTPS_LPWG_GRIP_SUPPORT_ENABLE)
		void shtps_lpwg_grip_set_state(struct shtps_rmi_spi *ts, u8 request);
	#endif /* SHTPS_LPWG_GRIP_SUPPORT_ENABLE */
	int shtps_set_lpwg_sleep_check(struct shtps_rmi_spi *ts);
	int shtps_set_lpwg_wakeup_check(struct shtps_rmi_spi *ts);
#endif /* SHTPS_LPWG_MODE_ENABLE */

#if defined( SHTPS_BOOT_FWUPDATE_ENABLE )
	int shtps_boot_fwupdate_enable_check(struct shtps_rmi_spi *ts);
#endif /* #if defined( SHTPS_BOOT_FWUPDATE_ENABLE ) */

int shtps_set_veilview_state(struct shtps_rmi_spi *ts, unsigned long arg);
int shtps_get_veilview_pattern(struct shtps_rmi_spi *ts);
int shtps_get_serial_number(struct shtps_rmi_spi *ts, u8 *buf);

void shtps_report_stflib_kevt(struct shtps_rmi_spi *ts, int event, int value);

int shtps_get_logflag(void);
long shtps_sys_getpid(void);
#if defined( SHTPS_DEVELOP_MODE_ENABLE )
int shtps_read_touchevent_from_outside(void);
int shtps_update_inputdev(struct shtps_rmi_spi *ts, int x, int y);
#endif /* #if defined( SHTPS_DEVELOP_MODE_ENABLE ) */

#if defined( SHTPS_LOW_POWER_MODE_ENABLE )
	void shtps_set_lpmode(struct shtps_rmi_spi *ts, int type, int req, int on);
	int shtps_check_set_doze_enable(void);
#endif	/* SHTPS_LOW_POWER_MODE_ENABLE */

#if defined(SHTPS_GLOVE_DETECT_ENABLE)
	void shtps_report_touch_glove_on(struct shtps_rmi_spi *ts, int finger, int x, int y, int w, int wx, int wy, int z);
	void shtps_report_touch_glove_off(struct shtps_rmi_spi *ts, int finger, int x, int y, int w, int wx, int wy, int z);
#endif /* SHTPS_GLOVE_DETECT_ENABLE */

#if defined( CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT )
	void shtps_facetouch_wakelock(struct shtps_rmi_spi *ts, u8 on);
#endif /* CONFIG_SHTPS_SY3000_FACETOUCH_OFF_DETECT */

#if defined(SHTPS_HOST_LPWG_MODE_ENABLE)
	int shtps_check_host_lpwg_enable(void);
#endif /* SHTPS_HOST_LPWG_MODE_ENABLE */

#if defined(SHTPS_CTRL_FW_REPORT_RATE)
	void shtps_set_fw_report_rate(struct shtps_rmi_spi *ts);
#endif /* SHTPS_CTRL_FW_REPORT_RATE */

#if defined(SHTPS_GLOVE_DETECT_ENABLE)
	int shtps_set_glove_detect_enable(struct shtps_rmi_spi *ts);
	int shtps_set_glove_detect_disable(struct shtps_rmi_spi *ts);
#endif /* SHTPS_GLOVE_DETECT_ENABLE */

#if defined(SHTPS_COVER_ENABLE)
	void shtps_cover_set_state(struct shtps_rmi_spi *ts, u8 request);
	void shtps_cover_notifier_callback(struct shtps_rmi_spi *ts, int on);
#endif /* SHTPS_COVER_ENABLE */

int shtpsif_init(void);
void shtpsif_exit(void);

#endif /* __SHTPS_RMI_H__ */
