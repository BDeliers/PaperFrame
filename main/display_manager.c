#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "display_manager.h"

#define DISPLAY_HEIGHT          480U
#define DISPLAY_WIDTH           800U
#define BUFFER_SIZE             (DISPLAY_WIDTH*DISPLAY_HEIGHT)/4U

#define STORAGE_NAMESPACE       "storage"

// We'll use 2 bits per pixel, black=0, white=1, red=2
static uint8_t framebuffer[BUFFER_SIZE] = {0};

uint8_t* display_get_framebuffer(void)
{
    return framebuffer;
}

uint32_t display_get_framebuffer_size(void)
{
    return BUFFER_SIZE;
}

bool display_save_framebuffer(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        return false;
    }

    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
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

bool display_restore_framebuffer(void)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // Open
    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        return false;
    }

    // Read the size of memory space required for blob
    size_t required_size = 0;  // value will default to 0, if not set yet in NVS
    err = nvs_get_blob(my_handle, "framebuffer", NULL, &required_size);

    // Fail if corrupted data or not set yet
    if (((err != ESP_OK) && (err != ESP_ERR_NVS_NOT_FOUND)) || (required_size != BUFFER_SIZE)) 
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