#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Get a pointer to the framebuffer.
// First half of framebuffer is for white/black, second half is for red/none
// 1 byte = 8 pixels. MSB = pixel 7n
uint8_t* display_manager_get_framebuffer(void);

// Get the framebuffer size
uint32_t display_manager_get_framebuffer_size(void);

void     display_manager_clear_framebuffer(void);

// Save the framebuffer to NVM
bool     display_manager_save_framebuffer(void);

// Restore the framebuffer from NVM
bool     display_manager_restore_framebuffer(void);

// Initialize this module
bool     display_manager_init(void);

// Transfer the buffer to the displan then send it to sleep mode
bool     display_manager_show(void);

// Put the display to lowest power mode
bool    display_manager_power_saving(void);

#ifdef __cplusplus
}
#endif
