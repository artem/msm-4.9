/* drivers/sharp/shbatt/shpwr_log.c
 *
 * Copyright (C) 2017 Sharp Corporation
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

/*+-----------------------------------------------------------------------------+*/
/*| @ DEFINE COMPILE SWITCH :													|*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ INCLUDE FILE :                                                            |*/
/*+-----------------------------------------------------------------------------+*/
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/file.h>
#include "misc/shpwr_log.h"

#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/namei.h>
#include <linux/time.h>
#include <linux/export.h>
#include <linux/dcache.h>
#include <linux/path.h>

#include <stdarg.h>

/*+-----------------------------------------------------------------------------+*/
/*| @ VALUE DEFINE DECLARE :                                                    |*/
/*+-----------------------------------------------------------------------------+*/
#define SHPWR_DBG_LOG_FNAME       ("shpwr_durable.log")
#define SHPWR_DBG_LOG_OLD_FNAME   ("shpwr_durable_old.log")
#define SHPWR_DBG_LOG_FILE        ("/durable/shpwr/shpwr_durable.log")
#define SHPWR_DBG_LOG_OLD_FILE    ("/durable/shpwr/shpwr_durable_old.log")
#define SHPWR_DUMP_REG_FNAME       ("shpwr_dump_reg.txt")
#define SHPWR_DUMP_REG_OLD_FNAME   ("shpwr_dump_reg_old.txt")
#define SHPWR_DUMP_REG_FILE       ("/durable/shpwr/shpwr_dump_reg.txt")
#define SHPWR_DUMP_REG_OLD_FILE   ("/durable/shpwr/shpwr_dump_reg_old.txt")

#define SHPWR_DBG_LOG_LINE_SIZE                   (32)
#define SHPWR_DBG_LOG_MSG_MAX_LENGTH              (256)
#define SHPWR_DBG_LOG_MAX_FILE_SIZE               (1 * 1024 * 1024)    /* 1MB */
#define SHPWR_DBG_LOG_COPY_BUF_SIZE               (1 * 1024)           /* 1KB */

#define SHPWR_DUMP_REG_LINE_SIZE                   (128)
#define SHPWR_DUMP_REG_MSG_MAX_LENGTH              (128)
#define SHPWR_DUMP_REG_MAX_FILE_SIZE               (512 * 1024)    /* 512KB */
#define SHPWR_DUMP_REG_COPY_BUF_SIZE               (1 * 1024)      /* 1KB */

#if defined(SHPWR_DURABLE_LOG_ENABLED) || defined(SHPWR_DUMP_REG_ENABLED)
const static const char* WeekOfDay[] = {
    "Sun"
   ,"Mon"
   ,"Tue"
   ,"Wed"
   ,"Thu"
   ,"Fri"
   ,"Sat"
};
#endif

/*+-----------------------------------------------------------------------------+*/
/*| @ ENUMERATION DECLARE :                                                     |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ STATIC VARIABLE :                                                         |*/
/*+-----------------------------------------------------------------------------+*/

static int						shpwr_batt_level = SHPWR_LOG_LEVEL_ERR;
module_param( shpwr_batt_level, int, 0664 );
static bool						enable_durable_log = true;
module_param( enable_durable_log, bool, 0664 );
static bool						enable_dump_reg = true;
module_param( enable_dump_reg, bool, 0664 );

#ifdef SHPWR_DURABLE_LOG_ENABLED
static char shpwr_dbg_log_buf[SHPWR_DBG_LOG_LINE_SIZE][SHPWR_DBG_LOG_MSG_MAX_LENGTH];
static int shpwr_dbg_log_buf_cur_index = 0;
#endif /* SHPWR_DURABLE_LOG_ENABLED */

#ifdef SHPWR_DUMP_REG_ENABLED
static char shpwr_dump_reg_buf[SHPWR_DUMP_REG_LINE_SIZE][SHPWR_DUMP_REG_MSG_MAX_LENGTH];
static int shpwr_dump_reg_buf_cur_index = 0;
#endif /* SHPWR_DUMP_REG_ENABLED */

static bool						shpwr_log_is_initialized = false;

static int						durable_shpwr_initialized = -1;
module_param(durable_shpwr_initialized, int, 0644);

/*+-----------------------------------------------------------------------------+*/
/*| @ LOCAL MACRO DECLARE :                                                     |*/
/*+-----------------------------------------------------------------------------+*/

#define SHPWR_ERROR(x...)	SHPWR_LOG(SHPWR_LOG_LEVEL_ERR,SHPWR_LOG_TYPE_BATT,x)
#define SHPWR_INFO(x...)	SHPWR_LOG(SHPWR_LOG_LEVEL_INFO,SHPWR_LOG_TYPE_BATT,x)
#define SHPWR_TRACE(x...)	SHPWR_LOG(SHPWR_LOG_LEVEL_DEBUG,SHPWR_LOG_TYPE_BATT,x)

#ifdef SHPWR_DURABLE_LOG_ENABLED
#if 0
#define SHPWR_DBG_LOG_ONE_LINE_FORMAT(length, message) length = snprintf((char *)shpwr_dbg_log_buf + shpwr_dbg_log_buf_cur_index * SHPWR_DBG_LOG_MSG_MAX_LENGTH,\
                       SHPWR_DBG_LOG_MSG_MAX_LENGTH,\
                      "%04d/%02d/%02d(%s), %02d:%02d:%02d, UTC +00h : %s",\
                      (int)(tm1.tm_year+1900), tm1.tm_mon + 1, tm1.tm_mday, WeekOfDay[tm1.tm_wday], tm1.tm_hour, tm1.tm_min, tm1.tm_sec, message);
#else
#define SHPWR_DBG_LOG_ONE_LINE_FORMAT(length, message) length = snprintf((char *)shpwr_dbg_log_buf + shpwr_dbg_log_buf_cur_index * SHPWR_DBG_LOG_MSG_MAX_LENGTH,\
                       SHPWR_DBG_LOG_MSG_MAX_LENGTH,\
                      "%04d/%02d/%02d(%s), %02d:%02d:%02d.%03d, UTC +%02dh : %s",\
                      (int)(tm2.tm_year+1900), tm2.tm_mon + 1, tm2.tm_mday, WeekOfDay[tm2.tm_wday], tm2.tm_hour, tm2.tm_min, tm2.tm_sec, (int)tv.tv_usec/1000,\
                      (int)((-sys_tz.tz_minuteswest) / 60), message);
#endif
#endif /* SHPWR_DURABLE_LOG_ENABLED */

#ifdef SHPWR_DUMP_REG_ENABLED
#define SHPWR_DUMP_REG_ONE_LINE_FORMAT(length, message) length = snprintf((char *)shpwr_dump_reg_buf + shpwr_dump_reg_buf_cur_index * SHPWR_DUMP_REG_MSG_MAX_LENGTH,\
                       SHPWR_DUMP_REG_MSG_MAX_LENGTH,\
                      "%s",message);
#endif /* SHPWR_DUMP_REG_ENABLED */
/*+-----------------------------------------------------------------------------+*/
/*| @ STRUCT & UNION DECLARE :                                                  |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ EXTERN FUNCTION PROTO TYPE DECLARE :                                      |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ LOCAL FUNCTION PROTO TYPE DECLARE :                                       |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ LOCAL FUNCTION'S CODE AREA :                                              |*/
/*+-----------------------------------------------------------------------------+*/

/* NONE.. */

/*+-----------------------------------------------------------------------------+*/
/*| @ PUBLIC FUNCTION'S CODE AREA :                                             |*/
/*+-----------------------------------------------------------------------------+*/

shpwr_log_level shpwr_log_current_level(
	shpwr_log_type				type
) {
	shpwr_log_level				ret;

	switch( type )
	{
	case SHPWR_LOG_TYPE_BATT:
		ret = shpwr_batt_level;
		break;
	default:
		ret = SHPWR_LOG_LEVEL_ERR;
		break;
	}
	if( ( ret < SHPWR_LOG_LEVEL_EMERG ) || ( ret > SHPWR_LOG_LEVEL_DEBUG ) )
	{
		ret = SHPWR_LOG_LEVEL_ERR;
	}
	return ret;
}
EXPORT_SYMBOL(shpwr_log_current_level);

/* ------------------------------------------------------------------------- */
/* VARIABLES                                                                 */
/* ------------------------------------------------------------------------- */
struct shpwr_dbg_log_output_work_command {
    void (* proc)(void);
    struct list_head list;
};

struct shpwr_dump_reg_output_work_command {
    void (* proc)(void);
    struct list_head list;
};
/* ------------------------------------------------------------------------- */
/* FUNCTIONS                                                                 */
/* ------------------------------------------------------------------------- */
#if defined(SHPWR_DURABLE_LOG_ENABLED) || defined(SHPWR_DUMP_REG_ENABLED)
static ssize_t shpwr_dbg_kernel_write(struct file *fp, const char *buf, size_t size);
static ssize_t shpwr_dbg_kernel_seek(struct file *fp, unsigned int offset, int whence);
static int shpwr_dbg_kernel_sync(struct file *fp);
static int shpwr_dbg_kernel_file_check_pointer(const struct file *fp, const char *buf);
#endif

/* for debug log file */
#ifdef SHPWR_DURABLE_LOG_ENABLED
static int shpwr_dbg_log_add_one_line(unsigned char *message);
static int shpwr_dbg_log_summary_init_file(void);
int shpwr_dbg_log_add_file(void);
static int shpwr_dbg_log_output(void);
static void shpwr_workqueue_handler_dbg_log_output(void);
static struct shpwr_dbg_log_output_work_command* shpwr_dbg_log_output_work_alloc_command(void);
static void shpwr_dbg_log_output_work_add_command(struct shpwr_dbg_log_output_work_command* cmd);
static void shpwr_dbg_log_output_work_start(void);
static void shpwr_dbg_log_output_work_free_command(struct shpwr_dbg_log_output_work_command* cmd);
static void shpwr_dbg_log_output_work_worker(struct work_struct *wk);
static DECLARE_WORK(shpwr_dbg_log_output_work_wk, shpwr_dbg_log_output_work_worker);
static DEFINE_SPINLOCK(shpwr_dbg_log_output_work_queue_lock);
static LIST_HEAD(shpwr_dbg_log_output_work_queue);
static struct workqueue_struct* shpwr_dbg_log_output_work_wq = NULL;
static int shpwr_dbg_log_buf_clear(void);
#endif /* SHPWR_DURABLE_LOG_ENABLED */
void shpwr_add_dbg_log(char *fmt, ...);
void shpwr_dbg_log_init(void);

/* for deump reg file */
#ifdef SHPWR_DUMP_REG_ENABLED
static int shpwr_dump_reg_add_one_line(unsigned char *message);
static int shpwr_dump_reg_summary_init_file(void);
int shpwr_dump_reg_add_file(void);
static int shpwr_dump_reg_output(void);
static void shpwr_workqueue_handler_dump_reg_output(void);
static struct shpwr_dump_reg_output_work_command* shpwr_dump_reg_output_work_alloc_command(void);
static void shpwr_dump_reg_output_work_add_command(struct shpwr_dump_reg_output_work_command* cmd);
static void shpwr_dump_reg_output_work_start(void);
static void shpwr_dump_reg_output_work_free_command(struct shpwr_dump_reg_output_work_command* cmd);
static void shpwr_dump_reg_output_work_worker(struct work_struct *wk);
static DECLARE_WORK(shpwr_dump_reg_output_work_wk, shpwr_dump_reg_output_work_worker);
static DEFINE_SPINLOCK(shpwr_dump_reg_output_work_queue_lock);
static LIST_HEAD(shpwr_dump_reg_output_work_queue);
static struct workqueue_struct* shpwr_dump_reg_output_work_wq = NULL;
static int shpwr_dump_reg_buf_clear(void);
#endif /* SHPWR_DUMP_REG_ENABLED */
void shpwr_add_dump_reg(bool force_save_flg, char *fmt, ...);
void shpwr_dump_reg_init(void);

/* ------------------------------------------------------------------------- */
/* shpwr_is_initialized                                                   */
/* ------------------------------------------------------------------------- */
bool shpwr_is_initialized(void)
{
	return ((shpwr_log_is_initialized == true) && (durable_shpwr_initialized == 1));
}
EXPORT_SYMBOL(shpwr_is_initialized);

#if defined(SHPWR_DURABLE_LOG_ENABLED) || defined(SHPWR_DUMP_REG_ENABLED)
/* ------------------------------------------------------------------------- */
/* shpwr_dbg_kernel_write                                                   */
/* ------------------------------------------------------------------------- */
static ssize_t shpwr_dbg_kernel_write(struct file *fp, const char *buf, size_t size)
{
    mm_segment_t old_fs;
    ssize_t res = 1;

    if(! shpwr_dbg_kernel_file_check_pointer(fp, buf)){
        return 0;
    }

    old_fs = get_fs();
    set_fs(get_ds());
    res = __vfs_write(fp, buf, size, &fp->f_pos);
    set_fs(old_fs);

    return res;
}


/* ------------------------------------------------------------------------- */
/* shpwr_dbg_kernel_seek                                                    */
/* ------------------------------------------------------------------------- */
static ssize_t shpwr_dbg_kernel_seek(struct file *fp, unsigned int offset, int whence)
{
    ssize_t res = 0;

    loff_t fpos;

    if(! shpwr_dbg_kernel_file_check_pointer(fp, SHPWR_DBG_KERNEL_FILE_CHECK_POINTER_ALWAYS_OK)){
        return 0;
    }

    fpos = offset;
    if (fp->f_op->llseek != NULL) {
        res = fp->f_op->llseek(fp, fpos, whence);
    } else {
        SHPWR_ERROR("llseek is null\n");
    }

    return res;
}


/* ------------------------------------------------------------------------- */
/* shpwr_dbg_kernel_sync                                                    */
/* ------------------------------------------------------------------------- */
static int shpwr_dbg_kernel_sync(struct file *fp)
{
    int res = 1;

    if(! shpwr_dbg_kernel_file_check_pointer(fp, SHPWR_DBG_KERNEL_FILE_CHECK_POINTER_ALWAYS_OK)){
        return 0;
    }

    if (fp->f_op->fsync != NULL) {
        res = fp->f_op->fsync(fp, 0, LLONG_MAX, 0);
    } else {
        SHPWR_ERROR("fsync is null\n");
    }

    return res;
}


/* ------------------------------------------------------------------------- */
/* shpwr_dbg_kernel_file_check_pointer                                      */
/* ------------------------------------------------------------------------- */
static int shpwr_dbg_kernel_file_check_pointer(const struct file *fp, const char *buf)
{
    int res = 1;

    if( IS_ERR_OR_NULL(fp) ){
        SHPWR_ERROR("<fp_INVALID_POINTER>\n");
        res = 0;
    }

    if( buf == NULL ){
        SHPWR_ERROR("<buf_NULL_POINTER>\n");
        res = 0;
    }

    return res;
}
#endif

/*---------------------------------------------------------------------------*/
/*      shpwr_add_dbg_log                                                   */
/*---------------------------------------------------------------------------*/
void shpwr_add_dbg_log(char *fmt, ...)
{
#ifdef SHPWR_DURABLE_LOG_ENABLED
    va_list argp;
    char buf[SHPWR_DBG_LOG_MSG_MAX_LENGTH];
    int index = 0;

    SHPWR_TRACE("[%s]in\n", __func__);

    if (enable_durable_log) {
        va_start(argp, fmt);
        vsprintf(buf, fmt, argp);
        va_end(argp);

        index = shpwr_dbg_log_add_one_line(buf);
        if ((index >= SHPWR_DBG_LOG_LINE_SIZE) && shpwr_is_initialized()) {
            shpwr_dbg_log_output();
        }
    }

    SHPWR_TRACE("[%s]out\n", __func__);
#endif /* SHPWR_DURABLE_LOG_ENABLED */
}
EXPORT_SYMBOL(shpwr_add_dbg_log);

/* ------------------------------------------------------------------------- */
/* shpwr_dbg_log_init                                                           */
/* ------------------------------------------------------------------------- */
void shpwr_dbg_log_init(void)
{
#ifdef SHPWR_DURABLE_LOG_ENABLED
    SHPWR_TRACE("[%s]in\n", __func__);

    shpwr_dbg_log_output_work_wq = create_singlethread_workqueue("shpwr_dbg_log_output_work_wq");
    if (!shpwr_dbg_log_output_work_wq) {
        SHPWR_ERROR("shpwr_dbg_log_output_work_wq create failed.\n" );
    }

    shpwr_dbg_log_buf_clear();

    SHPWR_TRACE("[%s]out\n", __func__);
#endif /* SHPWR_DURABLE_LOG_ENABLED */
}
EXPORT_SYMBOL(shpwr_dbg_log_init);

#ifdef SHPWR_DURABLE_LOG_ENABLED
/*---------------------------------------------------------------------------*/
/*      shpwr_dbg_log_output                                          */
/*---------------------------------------------------------------------------*/
static int shpwr_dbg_log_output(void)
{
    struct shpwr_dbg_log_output_work_command* cmd = NULL;
    int error = -EINVAL;

    SHPWR_TRACE("[%s]in\n", __func__);

    cmd = shpwr_dbg_log_output_work_alloc_command();
    if (cmd == NULL) {
        SHPWR_ERROR("allocate command error. no memory\n");
        error = -ENOMEM;
        goto errout;
    }

    cmd->proc = shpwr_workqueue_handler_dbg_log_output;

    shpwr_dbg_log_output_work_add_command(cmd);
    shpwr_dbg_log_output_work_start();

    SHPWR_TRACE("[%s]out normaly finished.\n", __func__);
    return 0;

errout:
    if (cmd != NULL) {
        shpwr_dbg_log_output_work_free_command(cmd);
    }

    SHPWR_ERROR("abnormaly finished. error=%d\n", error);
    return error;
}

/*---------------------------------------------------------------------------*/
/*      shpwr_workqueue_handler_dbg_log_output                              */
/*---------------------------------------------------------------------------*/
static void shpwr_workqueue_handler_dbg_log_output(void)
{

    SHPWR_TRACE("in\n");

    shpwr_dbg_log_add_file();

    SHPWR_TRACE("out\n");
}

/* ------------------------------------------------------------------------- */
/* shpwr_dbg_log_output_work_alloc_command                                      */
/* ------------------------------------------------------------------------- */
struct shpwr_dbg_log_output_work_command* shpwr_dbg_log_output_work_alloc_command(void)
{
    struct shpwr_dbg_log_output_work_command* cmd;
    cmd = (struct shpwr_dbg_log_output_work_command*)kzalloc(sizeof(struct shpwr_dbg_log_output_work_command), GFP_ATOMIC);
    if (cmd == NULL) {
        SHPWR_ERROR("kzalloc() failure.\n");
    }
    return cmd;
}

/* ------------------------------------------------------------------------- */
/* shpwr_dbg_log_output_work_free_command                                       */
/* ------------------------------------------------------------------------- */
static void shpwr_dbg_log_output_work_free_command(struct shpwr_dbg_log_output_work_command* cmd)
{
    if (cmd != NULL) {
        kfree(cmd);
    } else {
        SHPWR_ERROR("null pointer.\n");
    }
}

/* ------------------------------------------------------------------------- */
/* shpwr_dbg_log_output_work_add_command                                         */
/* ------------------------------------------------------------------------- */
static void shpwr_dbg_log_output_work_add_command(struct shpwr_dbg_log_output_work_command* cmd)
{
    SHPWR_TRACE("add queue: proc=%pS\n", cmd->proc);

    spin_lock(&shpwr_dbg_log_output_work_queue_lock);
    list_add_tail(&cmd->list, &shpwr_dbg_log_output_work_queue);
    spin_unlock(&shpwr_dbg_log_output_work_queue_lock);
}

/* ------------------------------------------------------------------------- */
/* shpwr_dbg_log_output_work_start                                              */
/* ------------------------------------------------------------------------- */
static void shpwr_dbg_log_output_work_start(void)
{
    SHPWR_TRACE("in\n");

    if (shpwr_dbg_log_output_work_wq != NULL) {
        if (queue_work(shpwr_dbg_log_output_work_wq, &shpwr_dbg_log_output_work_wk) == 0) {
            SHPWR_TRACE("work already pending.\n");
        }
    } else {
        SHPWR_ERROR("workqueue not created.\n");
    }
}

/* ------------------------------------------------------------------------- */
/* shpwr_dbg_log_output_work_worker                                             */
/* ------------------------------------------------------------------------- */
static void shpwr_dbg_log_output_work_worker(struct work_struct *wk)
{
    struct list_head* item;
    struct shpwr_dbg_log_output_work_command* cmd;

    SHPWR_TRACE("in\n");

    for (;;) {
        item = NULL;

        spin_lock(&shpwr_dbg_log_output_work_queue_lock);
        if (!list_empty(&shpwr_dbg_log_output_work_queue)) {
            item = shpwr_dbg_log_output_work_queue.next;
            list_del(item);
        }
        spin_unlock(&shpwr_dbg_log_output_work_queue_lock);

        if (item == NULL) {
            break;
        }

        cmd = list_entry(item, struct shpwr_dbg_log_output_work_command, list);
        SHPWR_TRACE("execute: proc=%pS\n", cmd->proc);
        cmd->proc();

        shpwr_dbg_log_output_work_free_command(cmd);
    }

    SHPWR_TRACE("out\n");
}

/* ------------------------------------------------------------------------- */
/* shpwr_dbg_log_add_file                                                   */
/* ------------------------------------------------------------------------- */
int shpwr_dbg_log_add_file(void)
{
    struct path  path;
    struct file *fp, *fp_old;
    int ret = -EINVAL;
    int index = 0;
    char *bufp;
    char *copy_bufp;
    ssize_t res;
    mm_segment_t old_fs;

    SHPWR_TRACE("in\n");
    ret = kern_path(SHPWR_DBG_LOG_FILE, LOOKUP_OPEN, &path);
    if (ret != 0) {
        ret = shpwr_dbg_log_summary_init_file();
        if (ret != SHPWR_RESULT_SUCCESS) {
            return SHPWR_RESULT_FAIL;
        }
    } else {
        path_put(&path);
    }

    fp = filp_open(SHPWR_DBG_LOG_FILE, O_RDWR | O_CREAT | O_APPEND, 0660);
    if (IS_ERR_OR_NULL(fp)) {
        SHPWR_ERROR("Cannot open file: %s err=%p\n", SHPWR_DBG_LOG_FILE, fp);
        return SHPWR_RESULT_FAIL;
    }

    if (shpwr_dbg_kernel_seek(fp, 0, SEEK_END)
            > (SHPWR_DBG_LOG_MAX_FILE_SIZE - (SHPWR_DBG_LOG_LINE_SIZE * SHPWR_DBG_LOG_MSG_MAX_LENGTH))) {

        /* Rename to SHPWR_DBG_LOG_OLD_FNAME */
        do {
            fp_old = filp_open(SHPWR_DBG_LOG_OLD_FILE, O_RDWR | O_CREAT | O_TRUNC, 0660);
            if (IS_ERR_OR_NULL(fp_old)) {
                SHPWR_ERROR("Cannot create file: %s err=%p\n", SHPWR_DBG_LOG_OLD_FILE, fp);
                return SHPWR_RESULT_FAIL;
            }

            /* copy file to SHPWR_DBG_LOG_OLD_FNAME */
            shpwr_dbg_kernel_seek(fp, 0, SEEK_SET);
            copy_bufp = kmalloc((size_t)SHPWR_DBG_LOG_COPY_BUF_SIZE, GFP_KERNEL);
            old_fs = get_fs();
            set_fs(get_ds());
            SHPWR_INFO("shpwr_dbg_log_add_file copy start\n");
            while(1) {
                res = __vfs_read(fp, copy_bufp, (size_t)SHPWR_DBG_LOG_COPY_BUF_SIZE, &fp->f_pos);
                if (!res) {
                    /* read file end */
                    break;
                }
                res = __vfs_write(fp_old, copy_bufp, (size_t)res, &fp_old->f_pos);
                if (!res) {
                    SHPWR_ERROR("Cannot write file:%s\n", SHPWR_DBG_LOG_OLD_FILE);
                    break;
                }
            }
            SHPWR_INFO("shpwr_dbg_log_add_file copy end\n");
            set_fs(old_fs);

            kfree(copy_bufp);
            filp_close(fp_old, NULL);
            filp_close(fp, NULL);

            /* reopen SHPWR_DBG_LOG_FILE */
            fp = filp_open(SHPWR_DBG_LOG_FILE, O_RDWR | O_CREAT | O_APPEND, 0660);
            if (IS_ERR_OR_NULL(fp)) {
                SHPWR_ERROR("Cannot open file: %s err=%p\n", SHPWR_DBG_LOG_FILE, fp);
                return SHPWR_RESULT_FAIL;
            }
        } while (0);

        ret = kern_path(SHPWR_DBG_LOG_FILE, LOOKUP_OPEN, &path);
        if (ret != 0) {
            ret = shpwr_dbg_log_summary_init_file();
            if (ret != SHPWR_RESULT_SUCCESS) {
                return SHPWR_RESULT_FAIL;
            }
        } else {
            truncate_setsize(path.dentry->d_inode, (off_t)0);
            path_put(&path);
        }
    }

    for (index = 0; index < SHPWR_DBG_LOG_LINE_SIZE; index++) {
        bufp = (char *)shpwr_dbg_log_buf + index * SHPWR_DBG_LOG_MSG_MAX_LENGTH;
        shpwr_dbg_kernel_write(fp, bufp, strlen(shpwr_dbg_log_buf[index]));
    }

    shpwr_dbg_log_buf_clear();

    {
        int res;
        if ((res = shpwr_dbg_kernel_sync(fp)) != 0) {
            SHPWR_ERROR("fsync result: %d\n", res);
        }
    }
    filp_close(fp, NULL);

    SHPWR_TRACE("out\n");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* shpwr_dbg_log_add_one_line                                           */
/* ------------------------------------------------------------------------- */
static int shpwr_dbg_log_add_one_line(unsigned char *message)
{
    struct timeval tv;
    struct tm tm1, tm2;
    int length = 0;

    SHPWR_TRACE("[%s]in\n", __func__);

    do_gettimeofday(&tv);
    time_to_tm((time_t)tv.tv_sec, 0, &tm1);
    time_to_tm((time_t)tv.tv_sec, (sys_tz.tz_minuteswest*60*(-1)), &tm2);

    if (shpwr_dbg_log_buf_cur_index < SHPWR_DBG_LOG_LINE_SIZE) {
        SHPWR_DBG_LOG_ONE_LINE_FORMAT(length, message)
    }

    if (length > 0) {
        SHPWR_TRACE("[%s]out index:%d\n", __func__, shpwr_dbg_log_buf_cur_index);
        return shpwr_dbg_log_buf_cur_index++;
    }

    SHPWR_TRACE("[%s]out index:%d is full.\n", __func__, shpwr_dbg_log_buf_cur_index);
    return shpwr_dbg_log_buf_cur_index;
}

/* ------------------------------------------------------------------------- */
/* shpwr_dbg_log_summary_init_file                                           */
/* ------------------------------------------------------------------------- */
static int shpwr_dbg_log_summary_init_file(void)
{
    struct file *fp;

    SHPWR_TRACE("[%s]in\n", __func__);

    SHPWR_TRACE("open file: %s pid=%d tgid=%d comm=%s\n", SHPWR_DBG_LOG_FILE, current->pid, current->tgid,
                                                                                                       current->comm);
    fp = filp_open(SHPWR_DBG_LOG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0660);
    if (IS_ERR_OR_NULL(fp)) {
        SHPWR_ERROR("Cannot create file: %s err=%p pid=%d tgid=%d comm=%s\n", SHPWR_DBG_LOG_FILE, fp, current->pid, current->tgid, current->comm);
        return SHPWR_RESULT_FAIL;
    }

    filp_close(fp, NULL);

    {
        int res;
        if ((res = shpwr_dbg_kernel_sync(fp)) != 0) {
            SHPWR_ERROR("fsync result: %d\n", res);
        }
    }

    SHPWR_TRACE("[%s]out\n", __func__);
    return SHPWR_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shpwr_dbg_log_buf_clear                                                  */
/* ------------------------------------------------------------------------- */
static int shpwr_dbg_log_buf_clear(void)
{
    size_t size = 0;
    SHPWR_TRACE("[%s]in\n", __func__);

    size = (SHPWR_DBG_LOG_LINE_SIZE * SHPWR_DBG_LOG_MSG_MAX_LENGTH);
    memset(&shpwr_dbg_log_buf, 0, size);
    shpwr_dbg_log_buf_cur_index = 0;

    SHPWR_TRACE("[%s]out\n", __func__);
    return SHPWR_RESULT_SUCCESS;
}
#endif /* SHPWR_DURABLE_LOG_ENABLED */

/*---------------------------------------------------------------------------*/
/*      shpwr_add_dump_reg                                                   */
/*---------------------------------------------------------------------------*/
void shpwr_add_dump_reg(bool force_save_flg, char *fmt, ...)
{
#ifdef SHPWR_DUMP_REG_ENABLED
    va_list argp;
    char buf[SHPWR_DUMP_REG_MSG_MAX_LENGTH];

    if (enable_dump_reg) {
        va_start(argp, fmt);
        vsprintf(buf, fmt, argp);
        va_end(argp);

        shpwr_dump_reg_add_one_line(buf);
        if ((force_save_flg) && shpwr_is_initialized()) {
            shpwr_dump_reg_output();
        }
    }
#endif /* SHPWR_DUMP_REG_ENABLED */
}
EXPORT_SYMBOL(shpwr_add_dump_reg);

/* ------------------------------------------------------------------------- */
/* shpwr_dump_reg_init                                                           */
/* ------------------------------------------------------------------------- */
void shpwr_dump_reg_init(void)
{
#ifdef SHPWR_DUMP_REG_ENABLED
    SHPWR_TRACE("in\n");

    shpwr_dump_reg_output_work_wq = create_singlethread_workqueue("shpwr_dump_reg_output_work_wq");
    if (!shpwr_dump_reg_output_work_wq) {
        SHPWR_ERROR("shpwr_dump_reg_output_work_wq create failed.\n" );
    }

    shpwr_dump_reg_buf_clear();

    SHPWR_TRACE("out\n");
#endif /* SHPWR_DUMP_REG_ENABLED */
}
EXPORT_SYMBOL(shpwr_dump_reg_init);

#ifdef SHPWR_DUMP_REG_ENABLED
/*---------------------------------------------------------------------------*/
/*      shpwr_dump_reg_output                                          */
/*---------------------------------------------------------------------------*/
static int shpwr_dump_reg_output(void)
{
    struct shpwr_dump_reg_output_work_command* cmd = NULL;
    int error = -EINVAL;

    SHPWR_TRACE("in\n");

    cmd = shpwr_dump_reg_output_work_alloc_command();
    if (cmd == NULL) {
        SHPWR_ERROR("allocate command error. no memory\n");
        error = -ENOMEM;
        goto errout;
    }

    cmd->proc = shpwr_workqueue_handler_dump_reg_output;

    shpwr_dump_reg_output_work_add_command(cmd);
    shpwr_dump_reg_output_work_start();

    SHPWR_TRACE("out normaly finished.\n");
    return 0;

errout:
    if (cmd != NULL) {
        shpwr_dump_reg_output_work_free_command(cmd);
    }

    SHPWR_ERROR("abnormaly finished. error=%d\n", error);
    return error;
}

/*---------------------------------------------------------------------------*/
/*      shpwr_workqueue_handler_dump_reg_output                              */
/*---------------------------------------------------------------------------*/
static void shpwr_workqueue_handler_dump_reg_output(void)
{

    SHPWR_TRACE("in\n");

    shpwr_dump_reg_add_file();

    SHPWR_TRACE("out\n");
}

/* ------------------------------------------------------------------------- */
/* shpwr_dump_reg_output_work_alloc_command                                      */
/* ------------------------------------------------------------------------- */
struct shpwr_dump_reg_output_work_command* shpwr_dump_reg_output_work_alloc_command(void)
{
    struct shpwr_dump_reg_output_work_command* cmd;
    cmd = (struct shpwr_dump_reg_output_work_command*)kzalloc(sizeof(struct shpwr_dump_reg_output_work_command), GFP_ATOMIC);
    if (cmd == NULL) {
        SHPWR_ERROR("kzalloc() failure.\n");
    }
    return cmd;
}

/* ------------------------------------------------------------------------- */
/* shpwr_dump_reg_output_work_free_command                                       */
/* ------------------------------------------------------------------------- */
static void shpwr_dump_reg_output_work_free_command(struct shpwr_dump_reg_output_work_command* cmd)
{
    if (cmd != NULL) {
        kfree(cmd);
    } else {
        SHPWR_ERROR("null pointer.\n");
    }
}

/* ------------------------------------------------------------------------- */
/* shpwr_dump_reg_output_work_add_command                                         */
/* ------------------------------------------------------------------------- */
static void shpwr_dump_reg_output_work_add_command(struct shpwr_dump_reg_output_work_command* cmd)
{
    SHPWR_TRACE("add queue: proc=%pS\n", cmd->proc);

    spin_lock(&shpwr_dump_reg_output_work_queue_lock);
    list_add_tail(&cmd->list, &shpwr_dump_reg_output_work_queue);
    spin_unlock(&shpwr_dump_reg_output_work_queue_lock);
}

/* ------------------------------------------------------------------------- */
/* shpwr_dump_reg_output_work_start                                              */
/* ------------------------------------------------------------------------- */
static void shpwr_dump_reg_output_work_start(void)
{
    SHPWR_TRACE("in\n");

    if (shpwr_dump_reg_output_work_wq != NULL) {
        if (queue_work(shpwr_dump_reg_output_work_wq, &shpwr_dump_reg_output_work_wk) == 0) {
            SHPWR_TRACE("work already pending.\n");
        }
    } else {
        SHPWR_ERROR("workqueue not created.\n");
    }
}

/* ------------------------------------------------------------------------- */
/* shpwr_dump_reg_output_work_worker                                             */
/* ------------------------------------------------------------------------- */
static void shpwr_dump_reg_output_work_worker(struct work_struct *wk)
{
    struct list_head* item;
    struct shpwr_dump_reg_output_work_command* cmd;

    SHPWR_TRACE("in\n");

    for (;;) {
        item = NULL;

        spin_lock(&shpwr_dump_reg_output_work_queue_lock);
        if (!list_empty(&shpwr_dump_reg_output_work_queue)) {
            item = shpwr_dump_reg_output_work_queue.next;
            list_del(item);
        }
        spin_unlock(&shpwr_dump_reg_output_work_queue_lock);

        if (item == NULL) {
            break;
        }

        cmd = list_entry(item, struct shpwr_dump_reg_output_work_command, list);
        SHPWR_TRACE("execute: proc=%pS\n", cmd->proc);
        cmd->proc();

        shpwr_dump_reg_output_work_free_command(cmd);
    }

    SHPWR_TRACE("out\n");
}

/* ------------------------------------------------------------------------- */
/* shpwr_dump_reg_add_file                                                   */
/* ------------------------------------------------------------------------- */
int shpwr_dump_reg_add_file(void)
{
    struct path  path;
    struct file *fp, *fp_old;
    int ret = -EINVAL;
    int index = 0;
    char *bufp;
    char *copy_bufp;
    ssize_t res;
    mm_segment_t old_fs;
    char buf[SHPWR_DUMP_REG_MSG_MAX_LENGTH];
    struct timeval tv;
    struct tm tm1, tm2;

    SHPWR_TRACE("in\n");
    ret = kern_path(SHPWR_DUMP_REG_FILE, LOOKUP_OPEN, &path);
    if (ret != 0) {
        ret = shpwr_dump_reg_summary_init_file();
        if (ret != SHPWR_RESULT_SUCCESS) {
            return SHPWR_RESULT_FAIL;
        }
    } else {
        path_put(&path);
    }

    fp = filp_open(SHPWR_DUMP_REG_FILE, O_RDWR | O_CREAT | O_APPEND, 0660);
    if (IS_ERR_OR_NULL(fp)) {
        SHPWR_ERROR("Cannot open file: %s err=%p\n", SHPWR_DUMP_REG_FILE, fp);
        return SHPWR_RESULT_FAIL;
    }

    if (shpwr_dbg_kernel_seek(fp, 0, SEEK_END)
            > (SHPWR_DUMP_REG_MAX_FILE_SIZE - (SHPWR_DUMP_REG_LINE_SIZE * SHPWR_DUMP_REG_MSG_MAX_LENGTH))) {

        /* Rename to SHPWR_DUMP_REG_OLD_FNAME */
        do {
            fp_old = filp_open(SHPWR_DUMP_REG_OLD_FILE, O_RDWR | O_CREAT | O_TRUNC, 0660);
            if (IS_ERR_OR_NULL(fp_old)) {
                SHPWR_ERROR("Cannot create file: %s err=%p\n", SHPWR_DUMP_REG_OLD_FILE, fp);
                return SHPWR_RESULT_FAIL;
            }

            /* copy file to SHPWR_DUMP_REG_OLD_FNAME */
            shpwr_dbg_kernel_seek(fp, 0, SEEK_SET);
            copy_bufp = kmalloc((size_t)SHPWR_DUMP_REG_COPY_BUF_SIZE, GFP_KERNEL);
            old_fs = get_fs();
            set_fs(get_ds());
            SHPWR_INFO("shpwr_dump_reg_add_file copy start\n");
            while(1) {
                res = __vfs_read(fp, copy_bufp, (size_t)SHPWR_DUMP_REG_COPY_BUF_SIZE, &fp->f_pos);
                if (!res) {
                    /* read file end */
                    break;
                }
                res = __vfs_write(fp_old, copy_bufp, (size_t)res, &fp_old->f_pos);
                if (!res) {
                    SHPWR_ERROR("Cannot write file:%s\n", SHPWR_DUMP_REG_OLD_FILE);
                    break;
                }
            }
            SHPWR_INFO("shpwr_dump_reg_add_file copy end\n");
            set_fs(old_fs);

            kfree(copy_bufp);
            filp_close(fp_old, NULL);
            filp_close(fp, NULL);

            /* reopen SHPWR_DUMP_REG_FILE */
            fp = filp_open(SHPWR_DUMP_REG_FILE, O_RDWR | O_CREAT | O_APPEND, 0660);
            if (IS_ERR_OR_NULL(fp)) {
                SHPWR_ERROR("Cannot open file: %s err=%p\n", SHPWR_DUMP_REG_FILE, fp);
                return SHPWR_RESULT_FAIL;
            }
        } while (0);

        ret = kern_path(SHPWR_DUMP_REG_FILE, LOOKUP_OPEN, &path);
        if (ret != 0) {
            ret = shpwr_dump_reg_summary_init_file();
            if (ret != SHPWR_RESULT_SUCCESS) {
                return SHPWR_RESULT_FAIL;
            }
        } else {
            truncate_setsize(path.dentry->d_inode, (off_t)0);
            path_put(&path);
        }
    }

    do_gettimeofday(&tv);
    time_to_tm((time_t)tv.tv_sec, 0, &tm1);
    time_to_tm((time_t)tv.tv_sec, (sys_tz.tz_minuteswest*60*(-1)), &tm2);

#if 0
    snprintf((char *)buf, SHPWR_DUMP_REG_MSG_MAX_LENGTH, "%04d/%02d/%02d(%s), %02d:%02d:%02d, UTC +00h : \n",
            (int)(tm1.tm_year+1900),
            tm1.tm_mon + 1,
            tm1.tm_mday,
            WeekOfDay[tm1.tm_wday],
            tm1.tm_hour,
            tm1.tm_min,
            tm1.tm_sec);
#else
    snprintf((char *)buf, SHPWR_DUMP_REG_MSG_MAX_LENGTH, "%04d/%02d/%02d(%s), %02d:%02d:%02d.%03d, UTC +%02dh : \n",
            (int)(tm2.tm_year+1900),
            tm2.tm_mon + 1,
            tm2.tm_mday,
            WeekOfDay[tm2.tm_wday],
            tm2.tm_hour,
            tm2.tm_min,
            tm2.tm_sec,
            (int)tv.tv_usec/1000,
            (int)((-sys_tz.tz_minuteswest) / 60));
#endif
    shpwr_dbg_kernel_write(fp, (char *)buf, strlen(buf));

    for (index = 0; index < SHPWR_DUMP_REG_LINE_SIZE; index++) {
        bufp = (char *)shpwr_dump_reg_buf + index * SHPWR_DUMP_REG_MSG_MAX_LENGTH;
        shpwr_dbg_kernel_write(fp, bufp, strlen(shpwr_dump_reg_buf[index]));
    }

    shpwr_dump_reg_buf_clear();

    {
        int res;
        if ((res = shpwr_dbg_kernel_sync(fp)) != 0) {
            SHPWR_ERROR("fsync result: %d\n", res);
        }
    }
    filp_close(fp, NULL);

    SHPWR_TRACE("out\n");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* shpwr_dump_reg_add_one_line                                           */
/* ------------------------------------------------------------------------- */
static int shpwr_dump_reg_add_one_line(unsigned char *message)
{
    int length = 0;

    if (shpwr_dump_reg_buf_cur_index < SHPWR_DUMP_REG_LINE_SIZE) {
        SHPWR_DUMP_REG_ONE_LINE_FORMAT(length, message)
    }

    if (length > 0) {
        return shpwr_dump_reg_buf_cur_index++;
    }
    return shpwr_dump_reg_buf_cur_index;
}

/* ------------------------------------------------------------------------- */
/* shpwr_dump_reg_summary_init_file                                          */
/* ------------------------------------------------------------------------- */
static int shpwr_dump_reg_summary_init_file(void)
{
    struct file *fp;

    SHPWR_TRACE("[%s]in\n", __func__);

    SHPWR_TRACE("open file: %s pid=%d tgid=%d comm=%s\n", SHPWR_DUMP_REG_FILE, current->pid, current->tgid,
                                                                                                       current->comm);
    fp = filp_open(SHPWR_DUMP_REG_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0660);
    if (IS_ERR_OR_NULL(fp)) {
        SHPWR_ERROR("Cannot create file: %s err=%p pid=%d tgid=%d comm=%s\n", SHPWR_DUMP_REG_FILE, fp, current->pid, current->tgid, current->comm);
        return SHPWR_RESULT_FAIL;
    }

    filp_close(fp, NULL);

    {
        int res;
        if ((res = shpwr_dbg_kernel_sync(fp)) != 0) {
            SHPWR_ERROR("fsync result: %d\n", res);
        }
    }

    SHPWR_TRACE("[%s]out\n", __func__);
    return SHPWR_RESULT_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* shpwr_dump_reg_buf_clear                                                  */
/* ------------------------------------------------------------------------- */
static int shpwr_dump_reg_buf_clear(void)
{
    size_t size = 0;
    SHPWR_TRACE("in\n");

    size = (SHPWR_DUMP_REG_LINE_SIZE * SHPWR_DUMP_REG_MSG_MAX_LENGTH);
    memset(&shpwr_dump_reg_buf, 0, size);
    shpwr_dump_reg_buf_cur_index = 0;

    SHPWR_TRACE("out\n");
    return SHPWR_RESULT_SUCCESS;
}
#endif /* SHPWR_DUMP_REG_ENABLED */

static int __init shpwr_drv_module_init( void )
{
    SHPWR_TRACE("[%s]in\n", __func__);

    shpwr_dbg_log_init();
    shpwr_dump_reg_init();

	shpwr_log_is_initialized = true;

    SHPWR_TRACE("[%s]out\n", __func__);

	return 0;
}

static void __exit shpwr_drv_module_exit( void )
{
    SHPWR_TRACE("[%s]in\n", __func__);

    SHPWR_TRACE("[%s]out\n", __func__);
}

module_init(shpwr_drv_module_init);
module_exit(shpwr_drv_module_exit);

MODULE_DESCRIPTION("SH Power Log Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("SHARP CORPORATION");
MODULE_VERSION("1.0");
