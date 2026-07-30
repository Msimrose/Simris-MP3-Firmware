#ifndef PACT_SIM

#include "storage.h"
#include "ff.h"
#include "main.h"
#include <stdlib.h>

static FATFS fs_sd;    /* big structs: keep out of task stacks */
static FATFS fs_emmc;

storage_status_t storage_mount_all(void)
{
    storage_status_t st = {0};

    /* eMMC RST_n deasserted before any MMC traffic. Factory parts ignore
     * the pin (JEDEC RST_n_FUNCTION ships disabled), but idle it high. */
    HAL_GPIO_WritePin(EMMC_RST_GPIO_Port, EMMC_RST_Pin, GPIO_PIN_SET);

    st.sd_mounted = (f_mount(&fs_sd, "0:", 1) == FR_OK);

    /* Internal eMMC ONLY: a factory-blank part has no filesystem - format
     * it exFAT once and retry. NEVER auto-format the user's SD card. */
    FRESULT fr = f_mount(&fs_emmc, "1:", 1);
    if (fr == FR_NO_FILESYSTEM) {
        void *work = malloc(64 * 1024);
        if (work) {
            MKFS_PARM parm = { .fmt = FM_EXFAT };
            if (f_mkfs("1:", &parm, work, 64 * 1024) == FR_OK)
                fr = f_mount(&fs_emmc, "1:", 1);
            free(work);
        }
    }
    st.emmc_mounted = (fr == FR_OK);
    return st;
}

void storage_unmount_all(void)
{
    f_unmount("0:");
    f_unmount("1:");
}

#endif /* !PACT_SIM */
