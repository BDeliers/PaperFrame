#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "display_config.h"
#include "display_manager.h"
#include "display_driver.h"

#define STORAGE_NAMESPACE       "storage"

// First half of the buffer is for white/black info, second half is for red/none
static uint8_t framebuffer[FRAMEBUFFER_SIZE] = {0};

static const char *TAG                  = "display_manager";

uint8_t* display_manager_get_framebuffer(void)
{
    return framebuffer;
}

uint32_t display_manager_get_framebuffer_size(void)
{
    return FRAMEBUFFER_SIZE;
}

void display_manager_clear_framebuffer(void)
{
    // Set framebuffer to full white
    memset(framebuffer, 0xFF, FRAMEBUFFER_SIZE/2);
    memset(framebuffer + FRAMEBUFFER_SIZE/2, 0x0, FRAMEBUFFER_SIZE/2);
}

bool display_manager_save_framebuffer(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open NVS
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        return false;
    }

    // Read the size of memory space required for blob
    size_t required_size = 0;  // Value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "framebuffer", NULL, &required_size);

    if ((err != ESP_OK) && (err != ESP_ERR_NVS_NOT_FOUND)) 
    {
        return false;
    }

    // Write value
    if (nvs_set_blob(my_handle, "framebuffer", framebuffer, required_size) != ESP_OK) 
    {
        return false;
    }

    // Commit
    if (nvs_commit(my_handle) != ESP_OK)
    {
        return false;
    }

    // Close
    nvs_close(my_handle);
    return true;
}

bool display_manager_restore_framebuffer(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open NVS
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        return false;
    }

    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "framebuffer", NULL, &required_size);

    // Fail if corrupted data or not set yet
    if (((err != ESP_OK) && (err != ESP_ERR_NVS_NOT_FOUND)) || (required_size != FRAMEBUFFER_SIZE)) 
    {
        return false;
    }

    // Read value
    if (nvs_get_blob(my_handle, "framebuffer", framebuffer, &required_size) != ESP_OK) 
    {
        return false;
    }

    // Commit
    if (nvs_commit(my_handle) != ESP_OK)
    {
        return false;
    }

    // Close
    nvs_close(my_handle);
    return true;
}

bool display_manager_init(void)
{
    return display_driver_init(framebuffer);
}

bool display_manager_show(void)
{
    uint8_t ret = 0;
    ret += display_configure();
    ret += display_transfer();
    ret += display_refresh();

    return ret == 3;
}

bool display_manager_power_saving(void)
{
    return display_low_power_mode();
}