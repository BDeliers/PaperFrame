#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "driver/spi_master.h"

bool    display_driver_init(uint8_t* framebuffer);
bool    display_configure(void);
bool    display_transfer(void);
bool    display_refresh(void);
bool    display_low_power_mode(void);

#ifdef __cplusplus
}
#endif
