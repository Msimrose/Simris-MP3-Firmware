/* Pact MP-1 - storage bring-up: mount both volumes.
 *   "0:" microSD (SDMMC1)   "1:" eMMC (SDMMC2)
 */
#pragma once
#include <stdbool.h>

typedef struct {
    bool sd_mounted;
    bool emmc_mounted;
} storage_status_t;

storage_status_t storage_mount_all(void);
