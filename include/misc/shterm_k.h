/* include/misc/shterm_k.h
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

#ifndef _SHTERM_K_H_
#define _SHTERM_K_H_

#include <uapi/shterm.h>

extern int shterm_k_set_info( unsigned long int shterm_info_id, unsigned long int shterm_info_value );
extern int shterm_k_set_event( shbattlog_info_t *info );
extern int shterm_flip_status_set( int state );
extern int shterm_get_music_info( void );

#endif /* _SHTERM_K_H_ */
