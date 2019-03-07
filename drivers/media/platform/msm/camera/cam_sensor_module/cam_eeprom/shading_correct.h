#ifndef __SHADING_CORRECT__
#define __SHADING_CORRECT__

#include "cam_eeprom_dev.h"

// #define _DEBUG

#define OFFSET_MODE_AVERAGE 1
#define OFFSET_MODE_MINIMUM 2

// #defilne
#define CHECK_SPEC  100
#define OFFSET_COEF 10000 // 1.0f * 10000
#define OFFSET_MODE OFFSET_MODE_AVERAGE
#define OFFSET_SPEC 100  //0.01f * 10000
#define EN_CHECK_SETTING   1
#define EN_CORRECT_SETTING 1
#define EN_REPEAT_SETTING  0

//
#define H_MAX        17
#define V_MAX        13
#define COLOR_MAX     4
#define H_LEFT_START  3
#define H_RIGHT_START (H_MAX - H_LEFT_START - 1)


#ifndef _DEBUG
int shading_correction(unsigned short int *lsc_dst, unsigned short int *lsc_src);

#else
int shading_correction(unsigned short int *lsc_dst, unsigned short int *lsc_src,
	int in_en_check, int in_en_correct, int in_en_repeat, int in_check_spec, float in_offset_coef, int in_offset_mode, float in_offset_spec,
	int *out_check_result, int *out_sabun_max, int *out_sabun_min, int *out_sabun1_dat, int *out_sabun2_dat, int *out_sum_right, int *out_sum_left,
	float *out_offset_value, float *out_shift_mode_ave, float *out_shift_val_ave, float *out_shift_val_min);

#endif

void cam_eeprom_shading_correction_imx318(uint8_t *otp_buf, uint32_t exlsc_offset);

#endif // __SHADING_CORRECT__