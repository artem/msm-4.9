/* drivers/input/touchscreen/shtps/sy3000/shtps_fwctl_s3700.h
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
#ifndef __SHTPS_FWCTL_S3700_H__
#define __SHTPS_FWCTL_S3700_H__
/* -------------------------------------------------------------------------- */
#include "shtps_param_extern.h"

/* -------------------------------------------------------------------------- */
struct rmi_reg_type_num_info{
	u8	enable;
	u16	addr;
	u8	sub_size;
	u8	sub_enable[20];
};

/* -------------------------------------------------------------------------- */
#define SHTPS_F01_RMI_CTRL_ADR	0x0b

#define SHTPS_PAGESELECT_ADR	0xff

#define F01_QUERY_MANUFACTURERID(data)			data[0]
#define F01_QUERY_CUSTOMMAP(data)				(data[1] & 0x01)
#define F01_QUERY_NONCOMPLIANT(data)			((data[1] >> 1) & 0x01)
#define F01_QUERY_HASSENSORID(data)				((data[1] >> 3) & 0x01)
#define F01_QUERY_HASAJUSTABLEDOZE(data)		((data[1] >> 5) & 0x01)
#define F01_QUERY_HASADJDOZEHOLDOFF(data)		((data[1] >> 6) & 0x01)
#define F01_QUERY_HASPRODUCTPROPERTIES2(data)	((data[1] >> 7) & 0x01)
#define F01_QUERY_PRODUCTINFO0(data)			(data[2] & 0x7F)
#define F01_QUERY_PRODUCTINFO1(data)			(data[3] & 0x7F)
#define F01_QUERY_DATECODEYEAR(data)			(data[4] & 0x1F)
#define F01_QUERY_DATECODEMONTH(data)			(((data[4] >> 5) & 0x07) | (data[5] & 0x01))
#define F01_QUERY_DATECODEDAY(data)				((data[5] >> 1) & 0x1F)
#define F01_QUERY_CP1(data)						((data[5] >> 6) & 0x01)
#define F01_QUERY_CP2(data)						((data[5] >> 7) & 0x01)
#define F01_QUERY_WAFERLOTID0(data)				((data[7] << 0x08) | data[6])
#define F01_QUERY_WAFERLOTID1(data)				((data[9] << 0x08) | data[8])
#define F01_QUERY_WAFERLOTID2(data)				data[10]
#define F01_QUERY_PRODUCTID(data)				data[11]
#define F01_QUERY_HASDS4QUERIES(data)			(data[21] & 0x01)
#define F01_QUERY_DS4_LENGTH(data)				(data[22] & 0x0F)
#define F01_QUERY_DS4_HASPACKAGEIDQUERY(data)	(data[23] & 0x01)
#define F01_QUERY_DS4_HASPACKRATQUERY(data)		((data[23] >> 1) & 0x01)
#define F01_QUERY_DS4_HASRESETQUERY(data)		((data[23] >> 2) & 0x01)
#define F01_QUERY_DS4_HASMASKREVQUERY(data)		((data[23] >> 3) & 0x01)
#define F01_QUERY_DS4_HASI2CCONTROL(data)		(data[24] & 0x01)
#define F01_QUERY_DS4_HASSPICONTROL(data)		((data[24] >> 1) & 0x01)
#define F01_QUERY_DS4_HASATTNCONTROL(data)		((data[24] >> 2) & 0x01)
#define F01_QUERY_DS4_HASTOOLIDPACKETQUERY(data)(data[25] & 0x01)
#define F01_QUERY_DS4_HASFIRMWAREREVISIONIDPACKETQUERY(data)	((data[25] >> 1) & 0x01)
#define F01_QUERY_RESET_ENABLED(data)			(data[26] & 0x01)
#define F01_QUERY_RESET_POLARITY(data)			((data[26] >> 1) & 0x01)
#define F01_QUERY_RESET_PULLUPENABLED(data)		((data[26] >> 2) & 0x01)

struct rmi_f01Query {
	u8	data[27];
};

struct rmi_f01 {
	u8					enable;
	u32					queryBase;
	u32					ctrlBase;
	u32					dataBase;
	u32					commandBase;
	struct rmi_f01Query	query;
};

#define F05_QUERY_NUMOFRCVEL(data)				(data[0] & 0x3F)
#define F05_QUERY_NUMOFTRANSEL(data)			(data[1] & 0x3F)
#define F05_QUERY_HAS16BIT(data)				((data[3] >> 7) & 0x01)
#define F05_QUERY_IMAGEWINDOWSIZE(data)			(data[4])

struct rmi_f05Query {
	u8	data[6];
};

struct rmi_f05 {
	u8					enable;
	u32					queryBase;
	u32					ctrlBase;
	u32					dataBase;
	u32					commandBase;
	struct rmi_f05Query	query;
};

#define F11_QUERY_NUMOFSENSORS(data)				(data[0] & 0x07)
#define F11_QUERY_HASQUERY9(data)					((data[0] >> 0x03) & 0x01)
#define F11_QUERY_HASQUERY11(data)					((data[0] >> 0x04) & 0x01)
#define F11_QUERY_HASQUERY12(data)					((data[0] >> 0x05) & 0x01)
#define F11_QUERY_NUMOFFINGERS(data)				(data[1] & 0x07)
#define F11_QUERY_HASREL(data)						((data[1] >> 0x03) & 0x01)
#define F11_QUERY_HASABS(data)						((data[1] >> 0x04) & 0x01)
#define F11_QUERY_HASGESTURES(data)					((data[1] >> 0x05) & 0x01)
#define F11_QUERY_HASSENSADJUST(data)				((data[1] >> 0x06) & 0x01)
#define F11_QUERY_CONFIGURABLE(data)				((data[1] >> 0x07) & 0x01)
#define F11_QUERY_NUMOFXELEC(data)					(data[2] & 0x1F)
#define F11_QUERY_NUMOFYELEC(data)					(data[3] & 0x1F)
#define F11_QUERY_MAXELEC(data)						(data[4] & 0x1F)
#define F11_QUERY_ABSOLUTEDATASIZE(data)			(data[5] & 0x03)
#define F11_QUERY_ANCHOREDFINGER(data)				((data[5] >> 2) & 0x01)
#define F11_QUERY_HASDRIBBLE(data)					((data[5] >> 4) & 0x01)
#define F11_QUERY_HASBENDINGCORRECTION(data)		((data[5] >> 5) & 0x01)
#define F11_QUERY_HASLARGEOBJECTSUPPRESSION(data)	((data[5] >> 6) & 0x01)
#define F11_QUERY_HASJITTERFILTER(data)				((data[5] >> 7) & 0x01)
#define F11_QUERY_HASPEN(data)						(data[6] & 0x01)
#define F11_QUERY_HASFINGERPROXIMITY(data)			((data[6] >> 1) & 0x01)
#define F11_QUERY_HASLARGEOBJECTSENSITIVITY(data)	((data[6] >> 2) & 0x01)
#define F11_QUERY_HASTWOPENTHRESHOLDS(data)			((data[6] >> 4) & 0x01)
#define F11_QUERY_HASPENHOVERDISCRIMINATION(data)	((data[6] >> 6) & 0x01)
#define F11_QUERY_HASNEWPENREGISTERS(data)			((data[6] >> 7) & 0x01)
#define F11_QUERY_HASZTUNING(data)					(data[7] & 0x01)
#define F11_QUERY_HASPOSITIONIPTUNING(data)			((data[7] >> 1) & 0x01)
#define F11_QUERY_HASWTUNING(data)					((data[7] >> 2) & 0x01)
#define F11_QUERY_HASPITCHINFO(data)				((data[7] >> 3) & 0x01)
#define F11_QUERY_HASDEFAULTFINGERWIDTH(data)		((data[7] >> 4) & 0x01)
#define F11_QUERY_HASSEGAGGRESSIVENESS(data)		((data[7] >> 5) & 0x01)
#define F11_QUERY_HASTXRXCLIP(data)					((data[7] >> 6) & 0x01)
#define F11_QUERY_HASDRUMMINGCORRECTION(data)		((data[7] >> 7) & 0x01)
#define F11_QUERY_HAS8BITW(data)					((data[8] >> 2) & 0x01)
#define F11_QUERY_HAS2DAJSTMAPPING(data)			((data[8] >> 3) & 0x01)

struct rmi_f11Query {
	u8	data[9];
};
struct rmi_f11Ctrl {
	u16 maxXPosition;
	u16 maxYPosition;
};

struct rmi_f11 {
	u8					enable;
	u32					queryBase;
	u32					ctrlBase;
	u32					dataBase;
	u32					commandBase;
	struct rmi_f11Query	query;
	struct rmi_f11Ctrl	ctrl;
};

#define F12_QUERY_HASREGISTERDESCRIPTOR(data)		(data[0] & 0x01)
#define F12_QUERY_SIZEOFQUERYPRESENCE(data)			(data[1])
#define F12_QUERY_SIZEOFQUERYSTRUCTURE(data)		(data[2])
#define F12_QUERY_QUERYSTRUCTURE(data)				(data[3])
#define F12_QUERY_SIZEOFCONTROLPRESENCE(data)		(data[4])
#define F12_QUERY_SIZEOFCONTROLSTRUCTURE(data)		(data[5])
#define F12_QUERY_CONTROLSTRUCTURE(data)			(data[6])
#define F12_QUERY_SIZEOFDATAPRESENCE(data)			(data[7])
#define F12_QUERY_SIZEOFDATASTRUCTURE(data)			(data[8])
#define F12_QUERY_DATASTRUCTURE(data)				(data[9])
#define F12_QUERY_SUPPORTEDOBJECTTYPES(data)		(data[10])

struct rmi_f12Query {
	u8	data[11];
};

struct rmi_f12Ctrl {
	u8  maxFingerNum;
	u16 maxXPosition;
	u16 maxYPosition;
	u8 support_object_glove;
	struct rmi_reg_type_num_info num[36];
};

struct rmi_f12Data {
	struct rmi_reg_type_num_info num[16];
};

struct rmi_f12 {
	u8					enable;
	u32					queryBase;
	u32					ctrlBase;
	u32					dataBase;
	u32					commandBase;
	struct rmi_f12Query	query;
	struct rmi_f12Ctrl	ctrl;
	struct rmi_f12Data	data;
};

#define FLASH_COMMAND_ENTER_BOOTLOADER		0x01
#define FLASH_COMMAND_READ					0x02
#define FLASH_COMMAND_WRITE					0x03
#define FLASH_COMMAND_ERASE					0x04

#define FLASH_STATUS_SUCCESS				0x00
#define FLASH_STATUS_NOT_ERASED				0x06

#define PARTITION_ID_BOOTLOADER				0x01
#define PARTITION_ID_FLASHCONFIG			0x03
#define PARTITION_ID_GUESTSERIALIZATION		0x05
#define PARTITION_ID_CORECODE				0x07
#define PARTITION_ID_CORECONFIG				0x08

#define PARTITION_TABLE_READ_SIZE			0x05
#define FLASH_BLOCK_SIZE					0x01

struct rmi_f34PartitionData {
	int coreCodeLength;
	int coreConfigLength;
};

#define F34_QUERY_BOOTLOADERID0(data)		data[1]
#define F34_QUERY_BOOTLOADERID1(data)		data[2]
#define F34_QUERY_FIRMWAREID0(data)			data[3]
#define F34_QUERY_FIRMWAREID1(data)			data[4]
#define F34_QUERY_FIRMWAREID2(data)			data[5]
#define F34_QUERY_FIRMWAREID3(data)			data[6]
#define F34_QUERY_MINIMUMWRITESIZE(data)	data[7]
#define F34_QUERY_BLOCKSIZE(data)			((data[8] & 0xff) | ((data[9] << 0x08) & 0xff00))
#define F34_QUERY_FLASHPAGESIZE(data)		((data[10] & 0xff) | ((data[11] << 0x08) & 0xff00))
#define F34_QUERY_ADJUSTPARTITIONSIZE(data)	((data[12] & 0xff) | ((data[13] << 0x08) & 0xff00))
#define F34_QUERY_FLASHCONFIGLENGTH(data)	((data[14] & 0xff) | ((data[15] << 0x08) & 0xff00))
#define F34_QUERY_PAYLOAD_LENGTH(data)		((data[16] & 0xff) | ((data[17] << 0x08) & 0xff00))
#define F34_QUERY_GUESTSERIALIZATION(data)	((data[18] >> 7) & 0x01)
#define F34_QUERY_DEVICECONFIG(data)		((data[18] >> 6) & 0x01)
#define F34_QUERY_DISPLAYCONFIG(data)		((data[18] >> 5) & 0x01)
#define F34_QUERY_GUESTCODE(data)			((data[18] >> 4) & 0x01)
#define F34_QUERY_CORECODE(data)			((data[18] >> 3) & 0x01)
#define F34_QUERY_CORECONFIG(data)			((data[18] >> 2) & 0x01)
#define F34_QUERY_FLASHCONFIG(data)			((data[18] >> 1) & 0x01)
#define F34_QUERY_MFGBLK(data)				((data[19] >> 2) & 0x01)
#define F34_QUERY_BOOTLOADER(data)			((data[19] >> 1) & 0x01)
#define F34_QUERY_GLOBALPARAMS(data)		(data[19] & 0x01)

struct rmi_f34Query {
	u8	data[22];
};

struct rmi_f34 {
	u8					enable;
	u32					queryBase;
	u32					ctrlBase;
	u32					dataBase;
	struct rmi_f34Query	query;
	struct rmi_f34PartitionData	pl;
};

#define F54_QUERY_NUMOFRCVEL(data)						data[0]
#define F54_QUERY_NUMOFTRANSEL(data)					data[1]
#define F54_QUERY_HASBASELINE(data)						((data[2] >> 2) & 0x01)
#define F54_QUERY_HAS8BIT(data)							((data[2] >> 3) & 0x01)
#define F54_QUERY_HAS16BIT(data)						((data[2] >> 6) & 0x01)
#define F54_QUERY_CLOCKRATE(data)						((data[4] << 0x08) | data[3])
#define F54_QUERY_ANALOGHARDWAREFAMILY(data)			data[5]
#define F54_QUERY_HASPIXELTOUCHTHRESHOLDTUNING(data)	(data[6] & 0x01)
#define F54_QUERY_HASARBITRARYSENSORASSIGNMENT(data)	(data[7] & 0x01)
#define F54_QUERY_HASINTERFERENCEMETRIC(data)			((data[7] >> 1) & 0x01)
#define F54_QUERY_HASSENSEFREQCONTROL(data)				((data[7] >> 2) & 0x01)
#define F54_QUERY_HASFWNOISEMITIGATION(data)			((data[7] >> 3) & 0x01)
#define F54_QUERY_HASLOWPOERCTRL(data)					((data[7] >> 4) & 0x01)
#define F54_QUERY_HASTWOBYTEREPORTRATEREPORTING(data)	((data[7] >> 5) & 0x01)
#define F54_QUERY_HASONEBYTEREPORTRATEREPORTING(data)	((data[7] >> 6) & 0x01)
#define F54_QUERY_HASRELAXATIONCTRL(data)				((data[7] >> 7) & 0x01)
#define F54_QUERY_AXISCOMPENSATIONMODE(data)			(data[8] & 0x03)
#define F54_QUERY_HASIIRFILTER(data)					((data[9] >> 1) & 0x01)
#define F54_QUERY_HASCMNREMOVAL(data)					((data[9] >> 2) & 0x01)
#define F54_QUERY_HASCMNCAPSCALEFACTOR(data)			((data[9] >> 3) & 0x01)
#define F54_QUERY_HASPIXCELTHRESHHYSTERESIS(data)		((data[9] >> 4) & 0x01)
#define F54_QUERY_HASEDGECOMPENSATION(data)				((data[9] >> 5) & 0x01)
#define F54_QUERY_HASPERFREQNOISECTRL(data)				((data[9] >> 6) & 0x01)
#define F54_QUERY_HASFORCEFASTRELAXATION(data)			(data[10] & 0x01)
#define F54_QUERY_HASMMSTATEMITIGATION(data)			((data[10] >> 1) & 0x01)
#define F54_QUERY_HASCDM4(data)							((data[10] >> 2) & 0x01)
#define F54_QUERY_HASVARIANCEMETRIC(data)				((data[10] >> 3) & 0x01)
#define F54_QUERY_HAS0DRELAXATIONCTRL(data)				((data[10] >> 4) & 0x01)
#define F54_QUERY_HAS0DACQUISITIONCTRL(data)			((data[10] >> 5) & 0x01)
#define F54_QUERY_MAXNUMOFSENSINGFREQ(data)				((data[14] >> 5) & 0x01)
#define F54_QUERY_BURSTSPERCLUSTER(data)				((data[15] >> 5) & 0x01)

struct rmi_f54Query {
	u8	data[21];
};

struct rmi_f54 {
	u8					enable;
	u32					queryBase;
	u32					ctrlBase;
	u32					dataBase;
	u32					commandBase;
	struct rmi_f54Query	query;
};

#define F19_QUERY_HASSENSITIVITYADJUST(data)	((data[0] >> 1) & 0x01)
#define F19_QUERY_HASHYSTERESISTHRESHOLD(data)	((data[0] >> 2) & 0x01)
#define F19_QUERY_BUTTONCOUNT(data)				(data[1] & 0x1F)

struct rmi_f19Query {
	u8	data[2];
};

struct rmi_f19 {
	u8					enable;
	u32					queryBase;
	u32					ctrlBase;
	u32					dataBase;
	u32					commandBase;
	struct rmi_f19Query	query;
};

#define F1A_QUERY_MAX_BUTTONCOUNT(data)					(data[0] & 0x07)
#define F1A_QUERY_HASGENERALCONTROL(data)				((data[1] >> 0) & 0x01)
#define F1A_QUERY_HASINTERRUPTENABLE(data)				((data[1] >> 1) & 0x01)
#define F1A_QUERY_HASMULTIBUTTONSELECT(data)			((data[1] >> 2) & 0x01)
#define F1A_QUERY_HASTXRXMAPPING(data)					((data[1] >> 3) & 0x01)
#define F1A_QUERY_HASPERBUTTONTHRESHOLD(data)			((data[1] >> 4) & 0x01)
#define F1A_QUERY_HASRELEASETHRESHOLD(data)				((data[1] >> 5) & 0x01)
#define F1A_QUERY_HASSTRONGESTBUTTONHYSTERESIS(data)	((data[1] >> 6) & 0x01)
#define F1A_QUERY_HASFILTERSTRENGTH(data)				((data[1] >> 7) & 0x01)

struct rmi_f1AQuery {
	u8	data[2];
};

struct rmi_f1A {
	u8					enable;
	u32					queryBase;
	u32					ctrlBase;
	u32					dataBase;
	u32					commandBase;
	struct rmi_f1AQuery	query;
};

#define F51_QUERY_QUERYREGISTERCOUNT(data)		data[0]
#define F51_QUERY_DATAREGISTERCOUNT(data)		data[1]
#define F51_QUERY_CONTROLREGISTERCOUNT(data)	data[2]
#define F51_QUERY_COMMANDREGISTERCOUNT(data)	data[3]
#define F51_QUERY_HASMULTITAPLPWG(data)			(data[4] & 0x01)
#define F51_QUERY_HASMISCCONTROL(data)			(data[5] & 0x01)
#define F51_QUERY_HASLGMCORRECTOT(data)			(data[6] & 0x01)
#define F51_QUERY_COVERMODEREGISTERADDRESS(data)		data[9]
#define F51_QUERY_COVERMODEREGISTERCOUNT(data)			data[10]
#define F51_QUERY_CUSTOMREPORTRATEREGISTERADDRESS(data)	data[11]

#define SHTPS_F51_CUSTOM_REPORTRATE_ADR		0x0401

struct rmi_f51Query {
	u8	data[12];
};

struct rmi_f51 {
	u8					enable;
	u32					queryBase;
	u32					ctrlBase;
	u32					dataBase;
	u32					commandBase;
	struct rmi_f51Query	query;
};

struct rmi_map {
	struct rmi_f01	fn01;
	struct rmi_f05	fn05;
	struct rmi_f11	fn11;
	struct rmi_f12	fn12;
	struct rmi_f34	fn34;
	struct rmi_f54	fn54;
	struct rmi_f19	fn19;
	struct rmi_f1A	fn1A;
	struct rmi_f51	fn51;
	//
	u8		reg_segmentation_aggressiveness_def[3];
	u8		reg_F12_CTRL11_val[9];
	u8		reg_F12_CTRL15_val[7];
	u8		reg_F12_CTRL22_val;
	u8		reg_F12_CTRL23_object_report_enable;
	u8		reg_F54_CTRL_pixel_touch_threshold_def_val;

	/* for LPWG */
	u8			fn12_ctrl8_enable_settings[SHTPS_LPWG_F12_CTRL08_BUFF_SIZE];
	u8			fn12_ctrl8_disable_settings[SHTPS_LPWG_F12_CTRL08_BUFF_SIZE];
	u8			fn12_ctrl20_enable_settings[3];
	u8			fn12_ctrl20_disable_settings[3];
	#if defined(SHTPS_LPWG_ALLOWED_SWIPES_ENABLE)
		u8		fn12_ctrl27_enable_settings[SHTPS_LPWG_F12_CTRL27_BUFF_SIZE];
		u8		fn12_ctrl27_disable_settings[SHTPS_LPWG_F12_CTRL27_BUFF_SIZE];
	#else
		u8		fn12_ctrl27_enable_settings[1];
		u8		fn12_ctrl27_disable_settings[1];
	#endif /* SHTPS_LPWG_ALLOWED_SWIPES_ENABLE */
	u8			fn12_ctrl18_disable_settings[SHTPS_LPWG_F12_CTRL18_BUFF_SIZE];
	u8			fn12_ctrl18_enable_settings[SHTPS_LPWG_F12_CTRL18_BUFF_SIZE];
	u8			fn12_ctrl15_enable_settings[7];
	u8			fn12_ctrl15_disable_settings[7];
	u8			fn12_ctrl22_enable_settings[1];
	u8			fn12_ctrl22_disable_settings[1];
	#if defined(SHTPS_LPWG_F51_REPORT_BEYOND_ACTIVE_AREA_ENABLE)
		u8		fn51_report_beyond_active_area_disable_settings;
		u8		fn51_report_beyond_active_area_enable_settings;
	#endif /* SHTPS_LPWG_F51_REPORT_BEYOND_ACTIVE_AREA_ENABLE */
	#if defined(SHTPS_CTRL_FW_REPORT_RATE)
		u8		fn51_custom_report_rate_def;
	#endif /* SHTPS_CTRL_FW_REPORT_RATE */
	#if defined(SHTPS_DEF_GLOVE_CHANGE_THERMAL_UPDATE_INTERVAL_ENABLE)
		u8		reg_fn54_ctrl133_TX_thermal_update_interval_def[2];
		u8		reg_fn54_ctrl133_RX_thermal_update_interval_def[2];
	#endif /* SHTPS_DEF_GLOVE_CHANGE_THERMAL_UPDATE_INTERVAL_ENABLE */
	#if defined(SHTPS_DEF_GLOVE_CHANGE_FREQ_SCAN_THRESHOLD_ENABLE)
		u8		reg_fn54_ctrl89_freq_scan_threshold_def[2];
	#endif /* SHTPS_DEF_GLOVE_CHANGE_FREQ_SCAN_THRESHOLD_ENABLE */
	#if defined(SHTPS_DEF_GLOVE_DISABLE_MOISTURE_ENABLE)
		u8		reg_F12_CTRL33_def;
	#endif /* SHTPS_DEF_GLOVE_DISABLE_MOISTURE_ENABLE */
};

//#define F11_DATA_FINGERSTATE0(data)		data[0]
#define F11_DATA_XPOS(data)				(((data[0] << 0x04) & 0x0FF0) | (data[2] & 0x0F))
#define F11_DATA_YPOS(data)				(((data[1] << 0x04) & 0x0FF0) | ((data[2] >> 0x04) & 0x0F))
#define F11_DATA_WX(data)				(data[3] & 0x0F)
#define F11_DATA_WY(data)				((data[3] >> 0x04) & 0x0F)
#define F11_DATA_Z(data)				data[4]

#define F12_DATA_FINGERSTATE(data)		(data[0])
#define F12_DATA_XPOS(data)				(data[1] | (data[2] << 8))
#define F12_DATA_YPOS(data)				(data[3] | (data[4] << 8))
#define F12_DATA_Z(data)				(data[5])
#define F12_DATA_WX(data)				(data[6])
#define F12_DATA_WY(data)				(data[7])

struct shtps_rmi_fingerState {
	u8	data[6];
};

/* -------------------------------------------------------------------------- */
#endif/* __SHTPS_FWCTL_S3700_H__ */

