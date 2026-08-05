#ifndef INIT_H
#define INIT_H

#include "nvic.h"
#include "systick.h"

/**
 * @brief One-time application bring-up: NVIC priority grouping + SysTick.
 * @note  Declaration only - defined in system/init.c. A full function body
 *        used to live directly in this header (not static/inline), which
 *        is an ODR violation waiting to happen: every .c file that
 *        #includes this header gets its own copy of system_init(), and the
 *        linker only tolerates that as long as exactly one translation
 *        unit ever includes it. Today that happens to be true (only
 *        main.c, via main.h) but it silently breaks the moment a second
 *        file - e.g. the communications drivers - includes it too.
 */
void system_init(void);

#endif  /* INIT_H */