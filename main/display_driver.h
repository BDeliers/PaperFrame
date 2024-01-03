#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "driver/spi_master.h"

// Initialize this module
bool    display_driver_init(uint8_t* framebuffer);

// Configure the display driver
bool    display_configure(void);

// Transfer the framebuffer to the display
bool    display_transfer(void);

// Refresh the display (show transfered buffer)
bool    display_refresh(void);

// Set display to lowest power mode
bool    display_low_power_mode(void);

#ifdef __cplusplus
}
#endif
