#ifndef ___MMC_SD_BATTLOG_H___
#define ___MMC_SD_BATTLOG_H___

#if defined(CONFIG_SHARP_MMC_SD_BATTLOG) && defined(CONFIG_SHARP_SHTERM)

#include "misc/shterm_k.h"

#define BATTLOG_EVENT_SD_DETECT_BASE (SHBATTLOG_EVENT_SD_DETECTED)
#define BATTLOG_EVENT_SD_ERROR_BASE  (SHBATTLOG_EVENT_SD_ERROR_UNKNOWN_UNKNOWN)

/* SD Card Detection */
typedef enum {
	SD_DETECTED,
	SD_DETECT_FAILED,
	SD_PHY_REMOVED,
	SD_SOFT_REMOVED,

	SD_DETECT_MAX
} mmc_sd_batlog_detect;

/* note MMC -> SD in battlog */
typedef enum {
	MMC_ERROR_UNKNOWN,
	MMC_ERROR_READ,
	MMC_ERROR_WRITE,
	MMC_ERROR_MISC,

	MMC_ERROR_SDIO, /* ignore them for now */
	MMC_ERROR_CMD_MAX = MMC_ERROR_SDIO
} mmc_sd_batlog_err_cmd;

typedef enum {
	_UNKNOWN,
	_CMD_TIMEOUT,
	_DATA_TIMEOUT,
	_REQ_TIMEOUT,
	_CMD_CRC_ERROR,
	_DATA_CRC_ERROR,
	_OTHER_ERROR,

	_MMC_ERR_TYPE_MAX
} mmc_sd_batlog_err_type;

int mmc_sd_detection_status_check(struct mmc_host *host);
void mmc_sd_post_detection(struct mmc_host *host, mmc_sd_batlog_detect detect);

void mmc_sd_set_err_cmd_type(struct mmc_host *host,
                          u32 cmd, mmc_sd_batlog_err_type type);
void mmc_sd_set_post_err_result(struct mmc_host *host);

void mmc_sd_post_dev_info(struct mmc_host *host);

#endif /* CONFIG_SHARP_MMC_SD_BATTLOG && CONFIG_SHARP_SHTERM */

#endif /* ___MMC_SD_BATTLOG_H___ */
