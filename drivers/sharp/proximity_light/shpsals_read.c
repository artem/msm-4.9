/* drivers/sharp/shpsals/shpsals_read.c
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
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/ioctl.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <soc/qcom/sh_smem.h>
#include "shpsals_read.h"

/* ------------------------------------------------------------------------- */
/* TYPE                                                                      */
/* ------------------------------------------------------------------------- */

static char		*buffer = NULL;
static size_t	buffer_size = 0;

static int shpsals_ioctl_read_smem_info(unsigned long arg)
{
	int i;
	struct shpsals_smem_info data;
	sharp_smem_common_type *sh_smem = sh_smem_get_common_address();

	if( sh_smem != 0 )
	{
		for(i = 0; i < ALS_ADJ_READ_TIMES; i ++)
		{
			data.als_adj0[i]  = sh_smem->sh_als_adj0[i];
			data.als_adj1[i]  = sh_smem->sh_als_adj1[i];
			data.als_shift[i] = sh_smem->sh_als_shift[i];
		}
		if( copy_to_user((int __user *)arg, &data, sizeof(struct shpsals_smem_info)) != 0 )
		{
			printk("[%s]: copy_to_user shpsals_smem_info FAIL\n",__func__);
			return SHPSALS_RESULT_FAILURE;
		}
	}
	else
	{
		printk("[%s]: sh_smem_get_common_address() FAIL\n",__func__);
		return SHPSALS_RESULT_FAILURE;
	}
	return SHPSALS_RESULT_SUCCESS;
}

static long shpsals_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = SHPSALS_RESULT_SUCCESS;
	
	switch(cmd) {
	case SHPSALS_IOCTL_GET_SMEM_INFO:
		ret = shpsals_ioctl_read_smem_info(arg);
		break;
	default:
		printk("[SH]shpsals_ioctl: cmd FAILE\n");
		ret = SHPSALS_RESULT_FAILURE;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long shpsals_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return shpsals_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
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
			return SHPSALS_RESULT_FAILURE;
		}
		
		memset( (void *)buffer, 0x00, buffer_size );
	}
	else
	{
		printk( KERN_ERR "%s : Buffer Already Allocated \n", __func__);
	}
	return SHPSALS_RESULT_SUCCESS;
}

static int shpsals_read_mmap(struct file *filep, struct vm_area_struct *vma)
{
	int ret = SHPSALS_RESULT_SUCCESS;
	unsigned long pfn = 0x00000000;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long length = vma->vm_end - vma->vm_start;
	unsigned long start = vma->vm_start;
	unsigned long pos = 0;

	ret = create_buffer( length );
	if( ret != SHPSALS_RESULT_SUCCESS )
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
			return SHPSALS_RESULT_FAILURE;
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

static struct file_operations shpsals_fops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.unlocked_ioctl = shpsals_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = shpsals_compat_ioctl,
#endif /* CONFIG_COMPAT */
	.mmap		= shpsals_read_mmap,
};

static struct miscdevice shpsals_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "shpsals_read",
	.fops = &shpsals_fops,
};

static int __init shpsals_init( void )
{
	int ret = SHPSALS_RESULT_SUCCESS;

	ret = misc_register(&shpsals_dev);
	if (SHPSALS_RESULT_SUCCESS != ret) {
		printk("fail to misc_register (shpsals_dev)\n");
		return ret;
	}
	printk("shpsals loaded.\n");
	return ret;
}

module_init(shpsals_init);

MODULE_DESCRIPTION("shpsals_read");
MODULE_LICENSE("GPL v2");

