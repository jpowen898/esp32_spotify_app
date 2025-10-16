/*
 * FileName: NvsMgr.h
 *
 * Author:   Parker Owen   <jpowen898@gmail.com>
 *
 * Description: wraps esp32 nvs system
 */

#include <NvsMgr.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define NVS_NAMESPACE "storage"
#define NVS_LOG_TAG "NvsMgr"

using namespace params;

/****************************************************************************/
NvsMgr::NvsMgr()
{
    esp_err_t err = nvs_flash_init(); // Initialize NVS
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase()); // Erase NVS if necessary
        err = nvs_flash_init();             // Reinitialize NVS
    }
    ESP_ERROR_CHECK(err);

    m_mutex = xSemaphoreCreateMutex(); // Create a FreeRTOS mutex
}
/****************************************************************************/
NvsMgr::~NvsMgr()
{
    if (m_mutex != nullptr)
    {
        vSemaphoreDelete(m_mutex); // Delete the mutex when done
    }
}
/****************************************************************************/
esp_err_t NvsMgr::set(const char* name, const void* value, size_t size)
{
    xSemaphoreTake(m_mutex, portMAX_DELAY); // Lock the mutex

    nvs_handle handle;
    esp_err_t  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        err = nvs_set_blob(handle, name, value, size);
        if (err == ESP_OK)
        {
            ESP_LOGD(NVS_LOG_TAG, "NVS set value. name:%s", name);
            nvs_commit(handle);
        }
        else
        {
            ESP_LOGE(NVS_LOG_TAG, "NVS set value failed. err:0x%x name:%s size:%d", err, name,
                     size);
        }
        nvs_close(handle);
    }
    else
    {
        ESP_LOGE(NVS_LOG_TAG, "Failed to open NVS handle");
    }

    xSemaphoreGive(m_mutex); // Unlock the mutex
    return err;              // Return the error code
}
/****************************************************************************/
esp_err_t NvsMgr::get(const char* name, void* value, size_t& size)
{
    xSemaphoreTake(m_mutex, portMAX_DELAY); // Lock the mutex

    nvs_handle handle;
    esp_err_t  err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK)
    {
        err = nvs_get_blob(handle, name, value, &size);
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGW(NVS_LOG_TAG, "NVS get value failed. Value not found. name:%s", name);
            size = 0;
        }
        else if (err != ESP_OK)
        {
            ESP_LOGE(NVS_LOG_TAG, "NVS get value failed to get NVS value. err:0x%x name:%s size:%d",
                     err, name, size);
            size = 0;
        }
        else
        {
            ESP_LOGD(NVS_LOG_TAG, "NVS get value. name:%s size:%d", name, size);
        }
        nvs_close(handle);
    }
    else
    {
        ESP_LOGE(NVS_LOG_TAG, "Failed to open NVS handle");
    }

    xSemaphoreGive(m_mutex); // Unlock the mutex
    return err;              // Return the error code
}
/****************************************************************************/
esp_err_t NvsMgr::erase(const char* name)
{
    xSemaphoreTake(m_mutex, portMAX_DELAY); // Lock the mutex

    nvs_handle handle;
    esp_err_t  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        err = nvs_erase_key(handle, name);
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGW(NVS_LOG_TAG, "NVS erase value failed. Value not found. name:%s", name);
        }
        else if (err != ESP_OK)
        {
            ESP_LOGE(NVS_LOG_TAG, "NVS erase value failed. err:0x%x name:%s", err, name);
        }
        else
        {
            ESP_LOGD(NVS_LOG_TAG, "NVS erase value. name:%s", name);
        }
        nvs_close(handle);
    }
    else
    {
        ESP_LOGE(NVS_LOG_TAG, "Failed to open NVS handle");
    }

    xSemaphoreGive(m_mutex); // Unlock the mutex
    return err;              // Return the error code
}
/****************************************************************************/
esp_err_t NvsMgr::eraseAll()
{
    xSemaphoreTake(m_mutex, portMAX_DELAY); // Lock the mutex

    nvs_handle handle;
    esp_err_t  err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        err = nvs_erase_all(handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(NVS_LOG_TAG, "NVS erase all data failed.");
        }
        else
        {
            ESP_LOGD(NVS_LOG_TAG, "NVS erase all data.");
            nvs_commit(handle);
        }
        nvs_close(handle);
    }
    else
    {
        ESP_LOGE(NVS_LOG_TAG, "Failed to open NVS handle");
    }

    xSemaphoreGive(m_mutex); // Unlock the mutex
    return err;              // Return the error code
}
/****************************************************************************/