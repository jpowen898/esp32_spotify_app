/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_timer.h"

#include "user_config.h"
#include "user_encoder_bsp.h"
#include "ui.h"
#include "display_init.h"

#include "ParamMgr.h"
#include "ConsoleCommands.h"
#include "wifi.h"

static const char* TAG = "main";

extern "C" void app_main(void)
{
    ConsoleCommandsInit();
    espwifi_Init();
    ESP_LOGI(TAG, "Starting Spotify App\n");

    display_init();
    ui_init();

    params::ParamMgr::getInstance().listAll();

    while (1)
        vTaskSuspend(NULL);
}
