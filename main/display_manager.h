#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

uint8_t* display_manager_get_framebuffer(void);
uint32_t display_manager_get_framebuffer_size(void);
void     display_manager_clear_framebuffer(void);
bool     display_manager_save_framebuffer(void);
bool     display_manager_restore_framebuffer(void);

bool     display_manager_init(void);
bool     display_manager_transfer_and_sleep(void);

#ifdef __cplusplus
}
#endif
