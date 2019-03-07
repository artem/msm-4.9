#include <linux/kernel.h>
#include <media/cam_sensor.h>

#include "shading_correct.h"
#include "cam_debug_util.h"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif


// **************************************************
//
//  Private
//
// **************************************************
int check_lsc(uint16_t *lsc, int check_spec, int32_t *sabun_max, int32_t *sabun_min, int32_t *sabun1_dat, int32_t *sabun2_dat, int32_t *sum_left, int32_t *sum_right)
{
	int ret = -1;

	int32_t *sabun1 = kzalloc(((V_MAX - 2) * H_MAX) * sizeof(int32_t), GFP_KERNEL);
	int32_t *sabun2 = kzalloc(((V_MAX - 4) * H_MAX) * sizeof(int32_t), GFP_KERNEL);


	int row, col;
	int sum, sum_max, sum_min;

	unsigned short int *LSC[V_MAX];

	CAM_DBG(CAM_EEPROM, "check_lsc start");

	for (row = 0; row < V_MAX; row++)
	{
		LSC[row] = lsc + H_MAX * row;
	}

	for (row = 0; row < V_MAX - 2; row++)
	{
		for (col = 0; col < H_MAX; col++)
		{
			sabun1[row * H_MAX + col] = (LSC[row + 1][col] << 1) - LSC[row][col] - LSC[row + 2][col];
		}
	}

	for (row = 0; row < V_MAX - 4; row++)
	{
		for (col = 0; col < H_MAX; col++)
		{
			sabun2[row * H_MAX + col] = (sabun1[(row + 1) * H_MAX + col] << 1) - sabun1[row * H_MAX + col] - sabun1[(row + 2) * H_MAX + col];
		}
	}

	for (row = 0; row < V_MAX - 4; row++)
	{
		sum = 0;
		for (col = 0; col < 5; col++)
		{
			sum += sabun2[row * H_MAX + col];
		}

		sum_left[row] = sum;

		if (row == 0)
		{
			sum_max = sum;
			sum_min = sum;
		}
		else
		{
			sum_max = MAX(sum_max, sum);
			sum_min = MIN(sum_min, sum);
		}

		sum = 0;
		for (col = H_MAX - 5; col < H_MAX; col++)
		{
			sum += sabun2[row * H_MAX + col];
		}

		sum_right[row] = sum;

		sum_max = MAX(sum_max, sum);
		sum_min = MIN(sum_min, sum);
	}

	for (row = 0; row < V_MAX - 2; row++)
	{
		for (col = 0; col < H_MAX; col++)
		{
			sabun1_dat[row * H_MAX + col] = sabun1[row * H_MAX + col];
		}
	}
	for (row = 0; row < V_MAX - 4; row++)
	{
		for (col = 0; col < H_MAX; col++)
		{
			sabun2_dat[row * H_MAX + col] = sabun2[row * H_MAX + col];
		}
	}

	*sabun_max = sum_max;
	*sabun_min = sum_min;

	if (check_spec * -1 <= sum_min && sum_max <= check_spec) {
		ret = 0;
	}


	kfree(sabun1);
	kfree(sabun2);

	CAM_DBG(CAM_EEPROM, "check_lsc end. ret = %d", ret);

	return ret;
}

int get_correction_parameter(int32_t *offset_value, int32_t *lsc_src, int offset_mode, int32_t *shift_mode_ave, 
							int32_t *shift_val_ave, int32_t *shift_val_min) {
	int ret = 0;
	int32_t left_ref_val[2];
	int32_t right_ref_val[2];
	int32_t right_val[2][3];
	int32_t left_val[2][3];
	int32_t shift_mode_color[COLOR_MAX];	/* 20180723 modified */
	int32_t shift_val_color[COLOR_MAX];
	int32_t shift_val[2];
	int32_t shift_mode_sum;
	int32_t shift_val_sum;


	int v = 0;
	int color = 0;
	int num = 0;
	int offset_idx;
	
	int	plus_mode_cnt;
	int minus_mode_cnt;
	int shift_direction;
	int32_t tmp_shift_val_min;

	memset(shift_val_color, 0, sizeof(shift_val_color));
	memset(shift_mode_color, 0, sizeof(shift_mode_color));

	for (v = 0; v < V_MAX; v++)
	{
		for (color = 0; color < COLOR_MAX; color++)
		{
			for (num = 0; num < 2; num++)
			{
				offset_idx = color * H_MAX * V_MAX + v * H_MAX;
				left_ref_val[num] = lsc_src[offset_idx + H_LEFT_START + num];
				right_val[num][0] = lsc_src[offset_idx + H_RIGHT_START - num];
				right_val[num][1] = lsc_src[offset_idx + H_RIGHT_START - num + 1];
				right_val[num][2] = lsc_src[offset_idx + H_RIGHT_START - num + 2];

				right_ref_val[num] = lsc_src[offset_idx + H_RIGHT_START - num];
				left_val[num][0] = lsc_src[offset_idx + H_LEFT_START + num];
				left_val[num][1] = lsc_src[offset_idx + H_LEFT_START + num - 1];
				left_val[num][2] = lsc_src[offset_idx + H_LEFT_START + num - 2];
			}

			if (left_ref_val[0] < right_ref_val[0] && left_ref_val[1] < right_ref_val[1])
			{
				shift_mode_color[color] = 1;

				for (num = 0; num < 2; num++)
				{
					if (left_ref_val[num] <= right_val[num][2])
					{
						shift_val[num] = 2 * 10000;
					}
					else if (left_ref_val[num] <= right_val[num][1])
					{
						shift_val[num] = ((right_val[num][1] - left_ref_val[num]) * 10000) / (right_val[num][1] - right_val[num][2]) + 1 * 10000;
					}
					else
					{
						shift_val[num] = ((right_val[num][0] - left_ref_val[num]) * 10000) / (right_val[num][0] - right_val[num][1]);
					}
				}
			}
			else if (left_ref_val[0] > right_ref_val[0] && left_ref_val[1] > right_ref_val[1])
			{
				shift_mode_color[color] = -1;

				for (num = 0; num < 2; num++)
				{
					if (left_val[num][2] >= right_ref_val[num])
					{
						shift_val[num] = 2 * 10000;
					}
					else if (left_val[num][1] >= right_ref_val[num])
					{
						shift_val[num] = ((left_val[num][1] - right_ref_val[num]) * 10000) / (left_val[num][1] - left_val[num][2]) + 1 * 10000;
					}
					else
					{
						shift_val[num] = ((left_val[num][0] - right_ref_val[num]) * 10000) / (left_val[num][0] - left_val[num][1]);
					}
				}
			}
			else
			{
				shift_mode_color[color] = 0;
				shift_val[0] = 0;
				shift_val[1] = 0;
			}

			shift_val_color[color] = (shift_val[0] + shift_val[1]) / 2;

		}

		/*  */
		shift_mode_sum = 0;
		shift_val_sum = 0;
		plus_mode_cnt = 0;
		minus_mode_cnt = 0;
		
		tmp_shift_val_min = 2 * 10000;
		for (color = 0; color < COLOR_MAX; color++)
		{
			shift_mode_sum += shift_mode_color[color] * 10000;
			shift_val_sum  += shift_val_color[color];

			if (shift_mode_color[color] == 1) {
				plus_mode_cnt++;
			}
			if (shift_mode_color[color] == -1) {
				minus_mode_cnt++;
			}
			if (shift_val_color[color] > 0) {
				tmp_shift_val_min = MIN(tmp_shift_val_min, shift_val_color[color]);
			}
		}
		
		if ( (plus_mode_cnt >= 2) && (minus_mode_cnt == 0) ) {
			shift_direction = 1;
			shift_mode_ave[v] = shift_mode_sum / COLOR_MAX;
			shift_val_ave[v] = shift_val_sum / COLOR_MAX;
			shift_val_min[v] = tmp_shift_val_min * plus_mode_cnt / COLOR_MAX;
		}
		else if ( (minus_mode_cnt >= 2) && (plus_mode_cnt == 0) ) {
			shift_direction = -1;
			shift_mode_ave[v] = shift_mode_sum / COLOR_MAX;
			shift_val_ave[v] = shift_val_sum / COLOR_MAX;
			shift_val_min[v] = tmp_shift_val_min * minus_mode_cnt / COLOR_MAX;
		}
		else {
			shift_direction = 0;
			shift_mode_ave[v] = 0;
			shift_val_ave[v] = 0;
			shift_val_min[v] = 0;
		}
		
		if (offset_mode == OFFSET_MODE_AVERAGE)	{
			offset_value[v] = shift_direction * shift_val_ave[v] / 2;
		}
		else if (offset_mode == OFFSET_MODE_MINIMUM) {
			offset_value[v] = shift_direction * shift_val_min[v] / 2;
		}
		else {
			offset_value[v] = 0;
		}
		
	}

	return ret;
}

int _shading_correction(int32_t *lsc_dst, int32_t *lsc_src,
	int32_t *offset_value, int32_t offset_coef, int32_t offset_spec) {

	/* lsc_dst, lsc_src 10000 */
	int ret = 0;
	int color = 0;
	int v = 0;
	int h = 0;
	int exp_h = 0;
	int32_t *lsc_src_exp;
	int32_t exp_offset_value;
	int32_t value_max_lsc;
	int32_t idx_offset = 0;

	lsc_src_exp = kzalloc((H_MAX + 2) * sizeof(int), GFP_KERNEL);


	for (color = 0; color < COLOR_MAX; color++)
	{
		/* offset_val */
		for (v = 0; v < V_MAX; v++)
		{
			idx_offset = color * V_MAX * H_MAX + v * H_MAX;

			/* create lsc_src_exp */
			value_max_lsc = 0;
			for (exp_h = 1; exp_h < H_MAX + 1; exp_h++)
			{
				lsc_src_exp[exp_h] = lsc_src[idx_offset + exp_h - 1];
				if (value_max_lsc < lsc_src_exp[exp_h]) {
						value_max_lsc = lsc_src_exp[exp_h];
				}
			}
			lsc_src_exp[0] = (lsc_src[idx_offset] * 2 - lsc_src[idx_offset + 1]);
			lsc_src_exp[H_MAX + 1] = (lsc_src[idx_offset + H_MAX - 1] * 2 - lsc_src[idx_offset + H_MAX - 2]);
			
			if (offset_value[v] > offset_spec)
			{
				exp_offset_value = offset_value[v] * offset_coef / 10000;
				for (h = 0; h < H_MAX; h++)
				{
					exp_h = h + 1;
					lsc_dst[idx_offset + h] = lsc_src_exp[exp_h] * (1 * 10000 - exp_offset_value) + lsc_src_exp[exp_h + 1] * exp_offset_value;
					lsc_dst[idx_offset + h] = lsc_dst[idx_offset + h] /10000;
				}
			}
			else if (offset_value[v] < (-1)*offset_spec)
			{
				exp_offset_value = 10000 + (offset_value[v] * offset_coef) / 10000;
				for (h = 0; h < H_MAX; h++)
				{
					exp_h = h;
					lsc_dst[idx_offset + h] = lsc_src_exp[exp_h] * (1 * 10000 - exp_offset_value) + lsc_src_exp[exp_h + 1] * exp_offset_value;
					lsc_dst[idx_offset + h] = lsc_dst[idx_offset + h] /10000;
				}
			}
			else
			{
				for (h = 0; h < H_MAX; h++)
				{
					exp_h = h + 1;
					lsc_dst[idx_offset + h] = lsc_src_exp[exp_h];
				}
			}
			lsc_dst[idx_offset + H_MAX / 2] = value_max_lsc;
		}
	}

	kfree(lsc_src_exp);
	
	return ret;
}

// **************************************************
//
//  Public
//
// **************************************************
int shading_correction(uint16_t *lsc_dst, uint16_t *lsc_src)
{
	int ret = 0;
	
	int check_spec = CHECK_SPEC;
	int offset_coef = OFFSET_COEF;
	int offset_mode = OFFSET_MODE;
	int offset_spec = OFFSET_SPEC;
	int en_check = EN_CHECK_SETTING;
	int en_correct = EN_CORRECT_SETTING;
	int en_repeat = EN_REPEAT_SETTING;

	int check_result = -1;
	int32_t *offset_value;
	int32_t *shift_mode_ave;
	int32_t *shift_val_ave;
	int32_t *shift_val_min;

	int32_t sabun_max = 0;
	int32_t sabun_min = 0;
	int32_t *sabun1_dat;
	int32_t *sabun2_dat;
	int32_t *sum_right;
	int32_t *sum_left;

	int32_t *tmp_lsc_src;
	int32_t *tmp_lsc_work;
	int32_t *tmp_lsc_dst;

	int idx_color =0, idx_row = 0, idx_column = 0, idx = 0;

	// parameter check
	if (en_correct == 0 && en_repeat == 1) {
		ret = -1;
		return ret;
	}

	// initialization
	sabun1_dat = kzalloc(((V_MAX - 2) * H_MAX) * sizeof(int32_t), GFP_KERNEL);
	sabun2_dat = kzalloc(((V_MAX - 4) * H_MAX) * sizeof(int32_t), GFP_KERNEL);
	sum_right = kzalloc((V_MAX - 4) * sizeof(int32_t), GFP_KERNEL);
	sum_left = kzalloc((V_MAX - 4) * sizeof(int32_t), GFP_KERNEL);

	tmp_lsc_src = kzalloc((COLOR_MAX * V_MAX * H_MAX) * sizeof(int32_t), GFP_KERNEL);
	tmp_lsc_work = kzalloc((COLOR_MAX * V_MAX * H_MAX) * sizeof(int32_t), GFP_KERNEL);
	tmp_lsc_dst = kzalloc((COLOR_MAX * V_MAX * H_MAX) * sizeof(int32_t), GFP_KERNEL);

	offset_value = kzalloc(V_MAX * sizeof(int32_t), GFP_KERNEL);
	shift_mode_ave = kzalloc(V_MAX * sizeof(int32_t), GFP_KERNEL);
	shift_val_ave = kzalloc(V_MAX * sizeof(int32_t), GFP_KERNEL);
	shift_val_min = kzalloc(V_MAX * sizeof(int32_t), GFP_KERNEL);


	memset(sabun1_dat, 0, ((V_MAX - 2) * H_MAX) * sizeof(int32_t));
	memset(sabun2_dat, 0, ((V_MAX - 4) * H_MAX) * sizeof(int32_t));
	memset(sum_right, 0, (V_MAX - 4) * sizeof(int32_t));
	memset(sum_left, 0, (V_MAX - 4) * sizeof(int32_t));

	memset(offset_value, 0, V_MAX * sizeof(int32_t));
	memset(shift_mode_ave, 0, V_MAX * sizeof(int32_t));
	memset(shift_val_ave, 0, V_MAX * sizeof(int32_t));
	memset(shift_val_min, 0, V_MAX * sizeof(int32_t));

	

	for (idx_color = 0; idx_color < COLOR_MAX; idx_color++) {
		for (idx_row = 0; idx_row < H_MAX; idx_row++) {
			for (idx_column = 0; idx_column < V_MAX; idx_column++) {
				idx = idx_color * H_MAX * V_MAX +
						  idx_row * V_MAX +
						  idx_column;
				lsc_dst[idx] = lsc_src[idx];

				tmp_lsc_src[idx] = lsc_src[idx];
				tmp_lsc_work[idx] = lsc_src[idx];
				tmp_lsc_dst[idx] = lsc_src[idx];
			}
		}
	}

	//
	// process
	//
	
	// check
	if (en_check == 1) {
		check_result = check_lsc(lsc_src, check_spec, &sabun_max, &sabun_min, sabun1_dat, sabun2_dat, sum_left, sum_right);
	}

	// need to correction
	if (check_result < 0) {

		CAM_DBG(CAM_EEPROM, "need to correction.");
		if (en_repeat == 1) {
			get_correction_parameter(offset_value, tmp_lsc_src, offset_mode, shift_mode_ave, shift_val_ave, shift_val_min);
			_shading_correction(tmp_lsc_work, tmp_lsc_src, offset_value, offset_coef, offset_spec);

			get_correction_parameter(offset_value, tmp_lsc_work, offset_mode, shift_mode_ave, shift_val_ave, shift_val_min);
			_shading_correction(tmp_lsc_dst, tmp_lsc_work, offset_value, offset_coef, offset_spec);
		}
		else {
			get_correction_parameter(offset_value, tmp_lsc_src, offset_mode, shift_mode_ave, shift_val_ave, shift_val_min);
			_shading_correction(tmp_lsc_dst, tmp_lsc_src, offset_value, offset_coef, offset_spec);
		}

		/* set to lsc_dst */
		for (idx_color = 0; idx_color < COLOR_MAX; idx_color++) {
			for (idx_row = 0; idx_row < H_MAX; idx_row++) {
				for (idx_column = 0; idx_column < V_MAX; idx_column++) {
					idx = idx_color * H_MAX * V_MAX +
						idx_row * V_MAX +
						idx_column;

					lsc_dst[idx] = tmp_lsc_dst[idx];
				}
			}
		}
	}

	kfree(sabun1_dat);
	kfree(sabun2_dat);
	kfree(sum_right);
	kfree(sum_left);
	kfree(tmp_lsc_src);
	kfree(tmp_lsc_work);
	kfree(tmp_lsc_dst);
	kfree(offset_value);
	kfree(shift_mode_ave);
	kfree(shift_val_ave);
	kfree(shift_val_min);


	return ret;
}

void cam_eeprom_shading_correction_imx318(uint8_t *otp_buf, uint32_t exlsc_offset)
{

	uint8_t *otp_exlsc_buf = otp_buf + exlsc_offset;
	uint16_t *lsc_src;
	uint16_t *lsc_dst;
	int i;

	lsc_src = kzalloc((COLOR_MAX * V_MAX * H_MAX) * sizeof(uint16_t), GFP_KERNEL);
	lsc_dst = kzalloc((COLOR_MAX * V_MAX * H_MAX) * sizeof(uint16_t), GFP_KERNEL);

	for(i = 0; i < (COLOR_MAX * V_MAX * H_MAX); i++){
		lsc_src[i] = (*(otp_exlsc_buf + 1) << 8) | *otp_exlsc_buf;
		CAM_DBG(CAM_EEPROM, "lsc_src[%03d] = 0x%04x, eeprom[%04d] = 0x%02x, eeprom[%04d] = 0x%02x,",
		      i, lsc_src[i], i*2, *otp_exlsc_buf, i*2+1, *(otp_exlsc_buf + 1));
		otp_exlsc_buf = otp_exlsc_buf +2;
	}


	shading_correction(lsc_dst, lsc_src);

	otp_exlsc_buf = otp_buf + exlsc_offset;
	
	for(i = 0; i < (COLOR_MAX * V_MAX * H_MAX); i++){
		*otp_exlsc_buf = (uint8_t)(lsc_dst[i] & 0x00FF);
		*(otp_exlsc_buf + 1) = (uint8_t)((lsc_dst[i] & 0x0300)>> 8);
		
		CAM_DBG(CAM_EEPROM, "lsc_dst[%03d] = 0x%04x, eeprom[%04d] = 0x%02x, eeprom[%04d] = 0x%02x,",
		      i, lsc_dst[i], i*2, *otp_exlsc_buf, i*2+1, *(otp_exlsc_buf + 1));
		otp_exlsc_buf = otp_exlsc_buf +2;
		
	}

	kfree(lsc_src);
	kfree(lsc_dst);

	return;
}



