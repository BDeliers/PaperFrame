#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define DISPLAY_HEIGHT          480U
#define DISPLAY_WIDTH           800U
#define FRAMEBUFFER_SIZE        (DISPLAY_WIDTH*DISPLAY_HEIGHT)/4U

#define PIN_SPI_DATA            14U
#define PIN_SPI_CLOCK           13U
#define PIN_DISPLAY_CS          15U
#define PIN_DISPLAY_DC          27U
#define PIN_DISPLAY_RST         26U
#define PIN_DISPLAY_BUSY        25U

#ifdef __cplusplus
}
#endif
