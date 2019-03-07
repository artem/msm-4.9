/**************************************************************************************************/
/** 
	@file		gpio_def_fseg.h
	@brief		GPIO Definition Header
*/
/**************************************************************************************************/

#ifndef GPIO_DEF
	#define GPIO_DEF

#include "gpio_def_ext_fseg.h"

typedef struct GPIO_DEF {
	unsigned int id;		/* GPIO Number (ID) */
	int direction;			/* I/O Direction */
	int out_val;			/* Initialized Value */
	int init_done;			/* GPIO Initialized ? 1:Complete (Don't Care) */
	char *label;			/* labels may be useful for diagnostics */
} stGPIO_DEF;

#define DirctionIn (0)
#define DirctionOut (1)

#endif	//GPIO_DEF
