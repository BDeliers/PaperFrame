#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#include "display_config.h"
#include "display_driver.h"

// Dipsplay driver command set
#define GD7965_REG_PSR          0x00U   // Panel Setting
#define GD7965_REG_PWR          0x01U   // Power Setting
#define GD7965_REG_POF          0x02U   // Power OFF
#define GD7965_REG_PFS          0x03U   // Power OFF Sequence Setting
#define GD7965_REG_PON          0x04U   // Power ON
#define GD7965_REG_PMES         0x05U   // Power ON Measure
#define GD7965_REG_BTST         0x06U   // Booster Soft Start
#define GD7965_REG_DSLP         0x07U   // Deep sleep
#define GD7965_REG_DTM1         0x10U   // Display Start Transmission 1 (White/Black Data)
#define GD7965_REG_DSP          0x11U   // Data Stop
#define GD7965_REG_DRF          0x12U   // Display Refresh
#define GD7965_REG_DTM2         0x13U   // Display Start transmission 2 (Red Data)
#define GD7965_REG_DUSPI        0x15U   // Dual SPI
#define GD7965_REG_AUTO         0x17U   // Auto Sequence
#define GD7965_REG_LUTOPT       0x2AU   // LUT option
#define GD7965_REG_KWOPT        0x2BU   // KW LUT option
#define GD7965_REG_PLL          0x30U   // PLL control
#define GD7965_REG_TSC          0x40U   // Temperature Sensor Calibration
#define GD7965_REG_TSE          0x41U   // Temperature Sensor Selection
#define GD7965_REG_TSW          0x42U   // Temperature Sensor Write
#define GD7965_REG_TSR          0x43U   // Temperature Sensor Read
#define GD7965_REG_PBC          0x44U   // Panel Break Check
#define GD7965_REG_CDI          0x50U   // VCOM and data interval setting
#define GD7965_REG_LPD          0x51U   // Lower Power Detection
#define GD7965_REG_EVS          0x52U   // End Voltage Setting
#define GD7965_REG_TCON         0x60U   // TCON setting
#define GD7965_REG_TRES         0x61U   // Resolution setting
#define GD7965_REG_GSST         0x65U   // Gate/Source Start setting
#define GD7965_REG_REV          0x70U   // Revision
#define GD7965_REG_FLG          0x71U   // Get Status
#define GD7965_REG_AMV          0x80U   // Auto Measurement VCOM
#define GD7965_REG_VV           0x81U   // Read VCOM Value
#define GD7965_REG_VDCS         0x82U   // VCOM_DC Setting
#define GD7965_REG_PTL          0x90U   // Partial Window
#define GD7965_REG_PTIN         0x91U   // Partial In
#define GD7965_REG_PTOUT        0x92U   // Partial Out
#define GD7965_REG_PGM          0xA0U   // Program Mode
#define GD7965_REG_APG          0xA1U   // Active Programming
#define GD7965_REG_ROTP         0xA2U   // Read OTP
#define GD7965_REG_CCSET        0xE0U   // Cascade Setting
#define GD7965_REG_PWS          0xE3U   // Power Saving
#define GD7965_REG_LVSEL        0xE4U   // LVD Voltage Select
#define GD7965_REG_TSSET        0xE5U   // Force Temperature
#define GD7965_REG_TSBDRY       0xE7U   // Temperature Boundary Phase-C2

// Display driver register values
#define GD7965_DSLP_CHECK       0xA5U   // Check value for deep-sleep command
#define GD7965_REVISION         0x0CU   // Revision code of GD7965

// Pointer to the framebuffer, given by caller module
static uint8_t*             framebuffer_ptr     = NULL;
static const char*          TAG                 = "display_driver";
static bool                 transaction_started = false;

// SPI device linked to the display
static spi_device_handle_t  spi_dev;

static bool spi_write_command(uint8_t command, bool keep_cs_active);
static bool spi_write_data(uint8_t* data, uint16_t len);
static bool spi_read_data(uint8_t* data, uint16_t len);
static void spi_pre_transfer_callback(spi_transaction_t *t);

static void display_wait_until_ready(void);

// Write a command to the display.
static bool spi_write_command(uint8_t command, bool keep_cs_active)
{
    // Get bus ownership
    spi_device_acquire_bus(spi_dev, portMAX_DELAY);

    spi_transaction_t t = {0};
    t.length    = 8;        // Command is 8 bits
    t.tx_buffer = &command; // The data is the cmd itself
    t.user      = (void*)0; // D/C needs to be set to 0

    if (keep_cs_active) {
        t.flags = SPI_TRANS_CS_KEEP_ACTIVE;   //Keep CS active after data transfer
    }

    // Transmit & wait until done
    transaction_started = true;
    esp_err_t ret = spi_device_polling_transmit(spi_dev, &t);

    // Release bus if the transaction is finished
    if (!keep_cs_active) {
        transaction_started = false;
        spi_device_release_bus(spi_dev);
    }

    return (ret == ESP_OK);
}

// Write data to the display/ Has to be called after spi_write_command
static bool spi_write_data(uint8_t* data, uint16_t len)
{
    if (!transaction_started)
    {
        return false;
    }

    // Send len bytes, D/C set to 1
    spi_transaction_t t = {0};
    t.length    = 8*len;
    t.tx_buffer = data;
    t.user      = (void*)1;

    // Transmit then release bus
    esp_err_t ret = spi_device_polling_transmit(spi_dev, &t);
    spi_device_release_bus(spi_dev);

    transaction_started = false;

    return (ret == ESP_OK);
}

// Read data from the display/ Has to be called after spi_write_command
static bool spi_read_data(uint8_t* data, uint16_t len)
{
    if (!transaction_started)
    {
        return false;
    }

    // Send len bytes, S/C set to 1
    spi_transaction_t t = {0};
    t.rxlength  = 8*len;
    t.user      = (void*)1;
    t.rx_buffer = data;

    // Receive then release bus
    esp_err_t ret = spi_device_polling_transmit(spi_dev, &t);
    spi_device_release_bus(spi_dev);

    transaction_started = false;

    return (ret == ESP_OK);
}

// Will be called before each SPI operation
// 'user' field of SPI transaction structure holds desired value for D/C pin
static void spi_pre_transfer_callback(spi_transaction_t *t)
{
    uint8_t dc = (uint8_t) t->user;

    gpio_set_level(PIN_DISPLAY_DC, dc);
}

// Wait until the BUSY line of the display gets back to idle
static void display_wait_until_ready(void)
{
    // Display is busy if busy pin is low
    do
    {
        vTaskDelay(1);
    }
    while (gpio_get_level(PIN_DISPLAY_BUSY) == 0);
}

// Refresh the display, i.e. show the framebuffer
bool display_refresh(void)
{
    ESP_LOGI(TAG, "display_refresh");

    bool ret = spi_write_command(GD7965_REG_DRF, false);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    display_wait_until_ready();

    return ret;
}

// Send the display to lowest power consumption
bool display_low_power_mode(void)
{
    ESP_LOGI(TAG, "display_low_power_mode");

    // Power-off display
    uint8_t cnt = spi_write_command(GD7965_REG_POF, false);
    display_wait_until_ready();

    // Deep-sleep for the driver
    cnt += spi_write_command(GD7965_REG_DSLP, true);
    uint8_t check_data = GD7965_DSLP_CHECK;
    cnt += spi_write_data(&check_data, 1);

    return (cnt == 3);
}

// Transfer framebuffer to the display
bool display_transfer(void)
{
    ESP_LOGI(TAG, "display_transfer");

    // Black data
    uint8_t cnt = spi_write_command(GD7965_REG_DTM1, true);
    cnt += spi_write_data(framebuffer_ptr, FRAMEBUFFER_SIZE/2);

    // Red data
    cnt += spi_write_command(GD7965_REG_DTM2, true);
    cnt += spi_write_data(framebuffer_ptr + FRAMEBUFFER_SIZE/2, FRAMEBUFFER_SIZE/2);

    return (cnt == 4);
}

// Initialize this module
bool display_driver_init(uint8_t* framebuffer)
{
    if (framebuffer == NULL)
    {
        return false;
    }

    framebuffer_ptr = framebuffer;

    // Initialize I/O
    // CS, RST and D/C are outputs
    gpio_config_t io_conf = {0};
    io_conf.pin_bit_mask = ((1U << PIN_DISPLAY_DC) | (1U << PIN_DISPLAY_RST));
    io_conf.mode = GPIO_MODE_OUTPUT;
    esp_err_t ret = gpio_config(&io_conf);

    // BUSY pin is an input
    if (ret == ESP_OK)
    {
        io_conf.pin_bit_mask = ((1U << PIN_DISPLAY_BUSY));
        io_conf.mode = GPIO_MODE_INPUT;
        ret |= gpio_config(&io_conf);
    }

    // Set idle levels for CS, RST and CS
    if (ret == ESP_OK)
    {
        gpio_set_level(PIN_DISPLAY_CS, 1);
        gpio_set_level(PIN_DISPLAY_RST, 1);
        gpio_set_level(PIN_DISPLAY_DC, 0);
    }

    if (ret == ESP_OK)
    {
        // Configure SPI bus
        spi_bus_config_t buscfg = {
            .miso_io_num = -1,
            .mosi_io_num = PIN_SPI_DATA,
            .sclk_io_num = PIN_SPI_CLOCK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = (FRAMEBUFFER_SIZE+1) * 8,
        };
        spi_device_interface_config_t devcfg = {
            .clock_speed_hz = 1e6,                              // Clock out at 1 MHz
            .mode = 0,                                          // SPI mode 0
            .spics_io_num = PIN_DISPLAY_CS,                     // CS pin
            .queue_size = 1,                                    // Don't queue SPI transactions
            .flags = SPI_DEVICE_3WIRE | SPI_DEVICE_HALFDUPLEX,  // MISO and MOSI on the same line, transmit then receive
            .pre_cb = spi_pre_transfer_callback                 // We'll play around with the D/C signal in this one
        };

        //  Initialize the SPI bus
        ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

        if (ret == ESP_OK)
        {
            //  Attach the display to the SPI bus
            ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_dev);
        }
    }

    return (ret == ESP_OK);
}

// Sed configuration flow to the display
bool display_configure(void)
{
    ESP_LOGI(TAG, "display_configure");

    // Hardware reset
    gpio_set_level(PIN_DISPLAY_RST, 0);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    gpio_set_level(PIN_DISPLAY_RST, 1);
    vTaskDelay(200 / portTICK_PERIOD_MS);

    // Detect driver
    uint8_t buff[7] = {0};
    if (!spi_write_command(GD7965_REG_REV, true) || !spi_read_data(buff, 7))
    {
        return false;
    }
    if (buff[6] != GD7965_REVISION)
    {
        return false;
    }

    // Init sequence
    spi_write_command(GD7965_REG_PWR, true);
    buff[0] = 0x07;    // Border LDO disabled, VD and VG generated from DC/DC
    buff[1] = 0x07;    // OTP power from VPP pin, slow slew rate, VGH=20V, VGL=-20V
    buff[2] = 0x3F;    // VDH=15V
    buff[3] = 0x3F;    // VDL=-15V
    spi_write_data(buff, 4);

    // Power on
    spi_write_command(GD7965_REG_PON, false);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    display_wait_until_ready();

    // Power configuration
    spi_write_command(GD7965_REG_PSR, true);
    buff[0] = 0x0F;    // LUT from OTP, KWR mode, scan up, shift right, booster on
    spi_write_data(buff, 1);

    // Display resolution
    spi_write_command(GD7965_REG_TRES, true);
    buff[0] = DISPLAY_WIDTH >> 8;      // Horizontal resolution MSBs
    buff[1] = DISPLAY_WIDTH & 0xFF;    // Horizontal resolution LSBs
    buff[2] = DISPLAY_HEIGHT >> 8;     // Vertical resolution MSBs
    buff[3] = DISPLAY_HEIGHT & 0xFF;   // Vertical resolution LSBs
    spi_write_data(buff, 4);

    spi_write_command(GD7965_REG_CDI, true);
    buff[0] = 0x11;    // Border output high-Z disabled, border LUT new data to old data copy disabled, LUT
    buff[1] = 0x07;    // VCOM and data interval 10 hsync
    spi_write_data(buff, 2);

    spi_write_command(GD7965_REG_TCON, true);
    buff[0] = 0x22;    // Non-overlap periods 12
    spi_write_data(buff, 1);

    // Gates/sources settings
    spi_write_command(GD7965_REG_GSST, true);
    buff[0] = 0x00;    // HSTART
    buff[1] = 0x00;
    buff[2] = 0x00;    // VSTART
    buff[3] = 0x00;
    spi_write_data(buff, 4);

    // Partial mode disabled (refresh full display)
    spi_write_command(GD7965_REG_PTOUT, false);

    return true;
}