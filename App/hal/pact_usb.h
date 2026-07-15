/*
 * Pact MP-1 - USB MSC (song transfer over USB-C, TinyUSB device stack).
 *
 * Device implementation: App/hal/usb_msc.c. The USB task owns the
 * VBUS-attach state machine: on attach it stops playback, unmounts both
 * FatFs volumes and exposes eMMC + microSD as two raw MSC LUNs; on detach
 * it reboots (iPod-style post-sync restart - boot rescans the library and
 * tops up the thumb cache). FatFs and the USB host never own a volume at
 * the same time.
 */
#pragma once
#include <stdbool.h>

void pact_usb_task(void *arg);     /* the usb task body */

/* True while a USB host owns the disks (volumes unmounted). */
bool pact_usb_active(void);
