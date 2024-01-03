#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

uint8_t* display_get_framebuffer(void);
uint32_t display_get_framebuffer_size(void);
bool     display_save_framebuffer(void);
bool     display_restore_framebuffer(void);

#ifdef __cplusplus
}
#endif
