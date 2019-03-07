/* drivers/soc/qcom/sharp/sharp-soc-smd_read.c
 *
 * Copyright (C) 2010 Sharp Corporation
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

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/poll.h>

#include <soc/qcom/sh_smem.h>
#include <soc/qcom/sharp/shdiag_smd.h>

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>

#define D_SHSOFTUP_F_MASK	0x00000010		/* bit4 */
#define D_SHSOFTUP_F_SHIFT	4

static char		*buffer = NULL;
static size_t	buffer_size = 0;
static int		msmfb_overlay_id = -1;
static int		msmfb_overlay2_id = -1;

static int smd_mode_open(struct inode *inode, struct file *filp)
{
/*	printk("%s\n", __func__);*/
	return 0;
}


static ssize_t smd_mode_read(struct file *filp, char __user *buf,size_t count, loff_t *ppos)
{
	sharp_smem_common_type *p_sh_smem_common_type = NULL;
	struct smem_comm_mode  smem_comm_data;
	uint32_t UpDateFlgStatus;

/*	printk("%s\n", __func__);*/
	if(count != sizeof(smem_comm_data)){
		return -EINVAL;
	}

	p_sh_smem_common_type = sh_smem_get_common_address();
	if( p_sh_smem_common_type != NULL){
		smem_comm_data.BootMode  = p_sh_smem_common_type->shdiag_BootMode;

		UpDateFlgStatus = p_sh_smem_common_type->shdiag_FlagData;
		smem_comm_data.UpDateFlg = ( UpDateFlgStatus & D_SHSOFTUP_F_MASK )>>D_SHSOFTUP_F_SHIFT;

		/* user aera */
		if( copy_to_user( buf, (void *)&smem_comm_data, sizeof(smem_comm_data) ) ){
			printk( "copy_to_user failed\n" );
			return -EFAULT;
		}
	} else {
		printk("[SH]smd_read_probe: smem_alloc FAILE\n");
	}
	return count;
}

static ssize_t smd_mode_write(struct file *filp, const char __user *buf,size_t count, loff_t *ppos)
{
/*	printk("%s\n", __func__);*/
	return count;
}

static int smd_mode_release(struct inode *inode, struct file *filp)
{
/*	printk("%s\n", __func__);*/
	return 0;
}

static int smd_ioctl_set_qxdmflg(unsigned long arg)
{
	int ret = 0;
	unsigned char qxdm_work;
	sharp_smem_common_type *sharp_smem;

	if(copy_from_user(&qxdm_work, (unsigned short __user *)arg, sizeof(unsigned char)) != 0)
	{
		printk("[SH]smd_ioctl_set_qxdmflg: copy_to_user FAILE\n");
		ret = -EFAULT;
	}
	else
	{
		sharp_smem = sh_smem_get_common_address();
		sharp_smem->shusb_qxdm_ena_flag = qxdm_work;
	}

	return ret;
}

static int smd_ioctl_set_proadj(unsigned long arg)
{
	int ret = 0;
	struct shdiag_procadj procadj_work;
	sharp_smem_common_type *sharp_smem;

	if(copy_from_user(&procadj_work, (unsigned short __user *)arg, sizeof(struct shdiag_procadj)) != 0)
	{
		printk("[SH]smd_ioctl_set_proadj: copy_to_user FAILE\n");
		ret = -EFAULT;
	}
	else
	{
		sharp_smem = sh_smem_get_common_address();
		sharp_smem->shdiag_proxadj[0] = (unsigned short)(procadj_work.proxcheckdata_min & 0x0000FFFF);
		sharp_smem->shdiag_proxadj[1] = (unsigned short)(procadj_work.proxcheckdata_max & 0x0000FFFF);
	}

	return ret;
}

static int smd_ioctl_get_hw_revision(unsigned long arg)
{
	int ret = 0;
	sharp_smem_common_type *sharp_smem;
	uint32_t hw_revision;

	sharp_smem = sh_smem_get_common_address();
	if( sharp_smem != 0 )
	{
		hw_revision = sharp_smem->sh_hw_revision;
		if(copy_to_user((uint32_t __user *)arg, &hw_revision, sizeof(uint32_t)) != 0)
		{
			printk("[SH]smd_ioctl_get_hw_revision: copy_to_user FAILE\n");
			ret = -EFAULT;
		}
	}else{
		printk("[SH]smd_ioctl_get_hw_revision: get smem_addr FAILE\n");
		return -EFAULT;
	}

	return ret;
}

static int smd_ioctl_set_hapticscal(unsigned long arg)
{
	int ret = 0;
	struct shdiag_hapticscal hapticscal_work;
	sharp_smem_common_type *sharp_smem;

	if(copy_from_user(&hapticscal_work, (unsigned short __user *)arg, sizeof(struct shdiag_hapticscal)) != 0)
	{
		printk("[SH]smd_ioctl_set_hapticscal: copy_to_user FAILE\n");
		ret = -EFAULT;
	}
	else
	{
		sharp_smem = sh_smem_get_common_address();
		memcpy( (void*)(sharp_smem->shdiag_tspdrv_acal_data), hapticscal_work.buf, SHDIAG_HAPTICSCAL_SIZE );
	}

	return ret;
}

static int smd_ioctl_set_msmfb_overlay_id(unsigned long arg)
{
	int ret = 0;

	if(copy_from_user(&msmfb_overlay_id, (int __user *)arg, sizeof(int)) != 0)
	{
		printk("[SH]smd_ioctl_set_msmfb_overlay_id: copy_from_user FAILE\n");
		ret = -EFAULT;
	}

	return ret;
}

static int smd_ioctl_get_msmfb_overlay_id(unsigned long arg)
{
	int ret = 0;

	if( msmfb_overlay_id != -1 )
	{
		if(copy_to_user((int __user *)arg, &msmfb_overlay_id, sizeof(int)) != 0)
		{
			printk("[SH]smd_ioctl_get_msmfb_overlay_id: copy_to_user FAILE\n");
			ret = -EFAULT;
		}
	}
	else
	{
		printk("[SH]smd_ioctl_get_msmfb_overlay_id: not set\n");
		ret = -EFAULT;
	}

	return ret;
}

static int smd_ioctl_set_msmfb_overlay2_id(unsigned long arg)
{
	int ret = 0;

	if(copy_from_user(&msmfb_overlay2_id, (int __user *)arg, sizeof(int)) != 0)
	{
		printk("[SH]smd_ioctl_set_msmfb_overlay2_id: copy_from_user FAILE\n");
		ret = -EFAULT;
	}

	return ret;
}

static int smd_ioctl_get_msmfb_overlay2_id(unsigned long arg)
{
	int ret = 0;

	if( msmfb_overlay2_id != -1 )
	{
		if(copy_to_user((int __user *)arg, &msmfb_overlay2_id, sizeof(int)) != 0)
		{
			printk("[SH]smd_ioctl_get_msmfb_overlay2_id: copy_to_user FAILE\n");
			ret = -EFAULT;
		}
	}
	else
	{
		printk("[SH]smd_ioctl_get_msmfb_overlay2_id: not set\n");
		ret = -EFAULT;
	}

	return ret;
}

static int smd_ioctl_get_lcd_upper_info(unsigned long arg)
{
	int upper_info = 0;
	sharp_smem_common_type *sh_smem = NULL;

	sh_smem = sh_smem_get_common_address();

	if( sh_smem != 0 )
	{
		upper_info = sh_smem->shdiag_upperunit;

		if(copy_to_user((int __user *)arg, &upper_info, sizeof(int)) != 0)
		{
			printk("[%s]: copy_to_user FAIL\n",__func__);
			return -EFAULT;
		}
	}
	else
	{
		printk("[%s]: sh_smem_get_common_address() FAIL\n",__func__);
		return -EFAULT;
	}
	return 0;
}

static long smd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;

	switch(cmd) {
	case SHDIAG_IOCTL_SET_QXDMFLG:
		ret = smd_ioctl_set_qxdmflg(arg);
		break;
	case SHDIAG_IOCTL_SET_PROADJ:
		ret = smd_ioctl_set_proadj(arg);
		break;
	case SHDIAG_IOCTL_GET_HW_REVISION:
		ret = smd_ioctl_get_hw_revision(arg);
		break;
	case SHDIAG_IOCTL_SET_HAPTICSCAL:
		ret = smd_ioctl_set_hapticscal(arg);
		break;
	case SHDIAG_IOCTL_SET_MSMFB_OVERLAY_ID:
		ret = smd_ioctl_set_msmfb_overlay_id(arg);
		break;
	case SHDIAG_IOCTL_GET_MSMFB_OVERLAY_ID:
		ret = smd_ioctl_get_msmfb_overlay_id(arg);
		break;
	case SHDIAG_IOCTL_SET_MSMFB_OVERLAY2_ID:
		ret = smd_ioctl_set_msmfb_overlay2_id(arg);
		break;
	case SHDIAG_IOCTL_GET_MSMFB_OVERLAY2_ID:
		ret = smd_ioctl_get_msmfb_overlay2_id(arg);
		break;
	case SHDIAG_IOCTL_GET_LCD_UPPER_INFO:
		ret = smd_ioctl_get_lcd_upper_info(arg);
		break;
	default:
		printk("[SH]smd_ioctl: cmd FAILE\n");
		ret = -EFAULT;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long smd_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return smd_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif /* CONFIG_COMPAT */

static int create_buffer( unsigned long length )
{

	printk( KERN_ERR "%s : Start\n", __func__);

	buffer_size = PAGE_ALIGN(length);

	if(buffer == NULL)
	{
		if( (buffer = vmalloc(buffer_size)) == NULL )
		{
			printk("%s : Error vmalloc\n", __func__);
			return -EFAULT;
		}

		memset( (void *)buffer, 0x00, buffer_size );
	}
	else
	{
		printk( KERN_ERR "%s : Buffer Already Allocated \n", __func__);
	}
	return 0;
}

static int smd_read_mmap(struct file *filep, struct vm_area_struct *vma)
{
	int ret = 0;
	unsigned long pfn = 0x00000000;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long length = vma->vm_end - vma->vm_start;
	unsigned long start = vma->vm_start;
	unsigned long pos = 0;

	ret = create_buffer( length );
	if( ret != 0 )
	{
		printk( KERN_ERR "%s : Error Create Buffer \n", __func__);
		return ret;
	}

	if( (offset >= buffer_size) || (length > (buffer_size - offset)) )
	{
		printk( KERN_ERR "%s : Error EINVAL \n", __func__);
		ret = -EINVAL;
		return ret;
	}

	vma->vm_flags |= VM_SHARED;
	while (length > 0)
	{
		pfn = vmalloc_to_pfn((void *)buffer + offset + pos);
		if (remap_pfn_range(vma, start, pfn, PAGE_SIZE, PAGE_SHARED))
		{
		    printk( KERN_ERR "%s : Error fail to remap \n", __func__);
			return -EAGAIN;
		}

		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (length > PAGE_SIZE)
		{
			length -= PAGE_SIZE;
		}
		else
		{
			length = 0;
		}
	}

	return ret;

}

#ifdef CONFIG_OF
	static struct of_device_id smd_mode_table[] = {
		{ .compatible = "sharp,sh_smd_read" ,},
		{},
	};
#else
	#define smd_mode_table NULL
#endif


static struct file_operations smd_mode_fops = {
	.owner		= THIS_MODULE,
	.read		= smd_mode_read,
	.write		= smd_mode_write,
	.open		= smd_mode_open,
	.release	= smd_mode_release,
	.unlocked_ioctl = smd_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = smd_compat_ioctl,
#endif /* CONFIG_COMPAT */
	.mmap		= smd_read_mmap,
};

static struct miscdevice smd_mode_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "smd_read",
	.fops = &smd_mode_fops,
};

static int smd_mode_probe( struct platform_device *pdev )
{
	int r = 0;

	if( !pdev )
	{
		return -EFAULT;
	}

	if( !pdev->dev.of_node )
	{
		return -ENODEV;
	}

	r = misc_register( &smd_mode_dev );

	if( r )
	{
		return -ENODEV;
	}

	return 0;
}

static struct platform_driver smd_mode_driver = {
	.probe = smd_mode_probe,
	.driver = {
		.owner = THIS_MODULE,
		.name = "smd_read",
		.of_match_table = smd_mode_table,
	},
};

static int __init smd_mode_init( void )
{
	return platform_driver_register( &smd_mode_driver );
}

static void __exit smd_mode_exit( void )
{
	platform_driver_unregister( &smd_mode_driver );
}

module_init( smd_mode_init );
module_exit( smd_mode_exit );

MODULE_DESCRIPTION("smd_read");
MODULE_LICENSE("GPL v2");

