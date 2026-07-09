/* Tiny threading port: pthreads on the host, FreeRTOS on the device. */
#pragma once

#ifdef PACT_SIM
    #include <pthread.h>
    typedef pthread_mutex_t pact_mutex_t;
    #define pact_mutex_init(m)   pthread_mutex_init((m), NULL)
    #define pact_mutex_lock(m)   pthread_mutex_lock(m)
    #define pact_mutex_unlock(m) pthread_mutex_unlock(m)
#else
    #include "FreeRTOS.h"
    #include "semphr.h"
    typedef SemaphoreHandle_t pact_mutex_t;
    static inline void pact_mutex_init_impl(pact_mutex_t *m) { *m = xSemaphoreCreateMutex(); }
    #define pact_mutex_init(m)   pact_mutex_init_impl(m)
    #define pact_mutex_lock(m)   xSemaphoreTake(*(m), portMAX_DELAY)
    #define pact_mutex_unlock(m) xSemaphoreGive(*(m))
#endif
