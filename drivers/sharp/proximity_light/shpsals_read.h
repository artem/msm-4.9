/**************************************************************************************************/
/** 
	@file		psals_read.h
	@brief		PSALS_SMEM_READ Header
*/
/**************************************************************************************************/

#ifndef PSALS_READ
	#define PSALS_READ

/* IO CONTROL  */
#define PSALS_IOCTL_MAGIC 's'

#define SHPSALS_IOCTL_GET_SMEM_INFO	_IOR(PSALS_IOCTL_MAGIC, 0x01, struct shpsals_smem_info)

#define ALS_ADJ_READ_TIMES		( 2 )

#define SHPSALS_RESULT_FAILURE    -1
#define SHPSALS_RESULT_SUCCESS     0

struct shpsals_smem_info {
    unsigned short als_adj0[2];
    unsigned short als_adj1[2];
    unsigned short als_shift[2];
};

#endif	//PSALS_READ
