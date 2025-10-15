/*
 * FileName: NvsMgr.h
 *
 * Author:   Parker Owen   <jpowen898@gmail.com>
 *
 * Description: wraps esp32 nvs system
 */

#ifndef __NVS_MGR_H__
#define __NVS_MGR_H__

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"

namespace params {
class NvsMgr {
public:
    static NvsMgr &getInstance()
    {
        static NvsMgr instance;  // Singleton instance
        return instance;
    }

    esp_err_t set(const char *name, const void *value, size_t size);
    esp_err_t get(const char *name, void *value, size_t &size);
    esp_err_t erase(const char *name);
    esp_err_t eraseAll();

private:
    NvsMgr();
    ~NvsMgr();
    NvsMgr(const NvsMgr &)            = delete;
    NvsMgr &operator=(const NvsMgr &) = delete;

    SemaphoreHandle_t m_mutex;  // Mutex to protect NVS access
};
}  // namespace params

#endif  // __NVS_MGR_H__