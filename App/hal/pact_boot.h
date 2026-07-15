/*
 * Pact MP-1 - device boot wiring.
 *
 * Creates the FreeRTOS application tasks (firmware-spec section 10).
 * Called from main() in the RTOS_THREADS user section, after
 * osKernelInitialize() and before osKernelStart().
 */
#pragma once
#include <stdbool.h>

void pact_boot_create_tasks(void);

/* True once storage_task has published the scanned library. */
bool pact_boot_library_ready(void);
