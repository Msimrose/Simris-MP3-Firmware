/*
 * Pact MP-1 - device boot wiring.
 *
 * Creates the FreeRTOS application tasks (firmware-spec section 10).
 * Called from main() in the RTOS_THREADS user section, after
 * osKernelInitialize() and before osKernelStart().
 */
#pragma once

void pact_boot_create_tasks(void);
