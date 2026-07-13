#ifndef PACT_SIM

#include "storage.h"
#include "ff.h"

static FATFS fs_sd;    /* big structs: keep out of task stacks */
static FATFS fs_emmc;

storage_status_t storage_mount_all(void)
{
    storage_status_t st = {0};
    st.sd_mounted = (f_mount(&fs_sd, "0:", 1) == FR_OK);
    st.emmc_mounted = (f_mount(&fs_emmc, "1:", 1) == FR_OK);
    return st;
}

#endif /* !PACT_SIM */
