/*
 * FileName: ConsoleCommands.cpp
 *
 * Author:   Parker Owen   <jpowen898@gmail.com>
 *
 * Description: Console command functions
 */

#include "ConsoleCommands.h"
#include "esp_heap_caps.h"
#include "esp_heap_trace.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"
#include "string.h"
#include <vector>
#include <algorithm>

#ifdef CONFIG_CONSOLE_COMMANDS_REBOOT
////////////////////////////////////////////////////////////////////////////////////////////////////
// reboot command:
////////////////////////////////////////////////////////////////////////////////////////////////////
esp_console_cmd_t reboot_cmd = {
    .command = "reboot",
    .help    = "Reboots the ESP32",
    .hint    = NULL,
    .func =
        [](int argc, char** argv)
    {
        printf("Rebooting...\n");
        esp_restart();
        return 0;
    },
    .argtable       = NULL,
    .func_w_context = NULL,
    .context        = NULL,
};
#endif // CONFIG_CONSOLE_COMMANDS_REBOOT

#ifdef CONFIG_CONSOLE_COMMANDS_TOP
struct TaskInfo
{
    const char*  name;
    TaskHandle_t handle;
    size_t       priority;
    size_t       stackHighWaterMark;
    size_t       lastRunTime;
    size_t       latestDuration;
    size_t       totalDuration;
    eTaskState   state;
};
static std::vector<TaskInfo> s_tasks;
int                          topCmd(int argc, char** argv)
{
    // Get the number of tasks
    UBaseType_t numTasks = uxTaskGetNumberOfTasks();
    // s_tasks.resize(numTasks);

    // Get the task list
    TaskStatus_t* taskStatusArray = (TaskStatus_t*) pvPortMalloc(numTasks * sizeof(TaskStatus_t));
    if (taskStatusArray == NULL)
    {
        printf("Error: Unable to allocate memory for task status array\n");
        return 1;
    }
    numTasks = uxTaskGetSystemState(taskStatusArray, numTasks, NULL);

    size_t allTasksTotalDuration = 0;
    size_t allTasksTotalTime     = 0;

    // Populate the task info vector
    for (UBaseType_t i = 0; i < numTasks; i++)
    {
        UBaseType_t j         = 0;
        bool        taskFound = false;
        for (; j < s_tasks.size(); j++)
        {
            if (s_tasks[j].handle == taskStatusArray[i].xHandle)
            {
                taskFound = true;
                break;
            }
        }

        if (!taskFound)
        {
            // New task, add to the list
            s_tasks.push_back(TaskInfo{});
            j                      = s_tasks.size() - 1;
            s_tasks[j].lastRunTime = 0;
        }

        s_tasks[j].name               = taskStatusArray[i].pcTaskName;
        s_tasks[j].handle             = taskStatusArray[i].xHandle;
        s_tasks[j].priority           = taskStatusArray[i].uxCurrentPriority;
        s_tasks[j].stackHighWaterMark = taskStatusArray[i].usStackHighWaterMark;
        s_tasks[j].state              = taskStatusArray[i].eCurrentState;

        // get latest run time stats
        s_tasks[j].totalDuration = taskStatusArray[i].ulRunTimeCounter;
        s_tasks[j].latestDuration =
            (taskStatusArray[i].ulRunTimeCounter > s_tasks[j].lastRunTime)
                ? taskStatusArray[i].ulRunTimeCounter - s_tasks[j].lastRunTime
                : 0;
        s_tasks[j].lastRunTime = taskStatusArray[i].ulRunTimeCounter;

        allTasksTotalDuration += s_tasks[j].latestDuration;
        allTasksTotalTime += s_tasks[j].totalDuration;
    }

    // Normalize by number of processors
    allTasksTotalDuration /= portNUM_PROCESSORS;
    allTasksTotalTime /= portNUM_PROCESSORS;

    // Free the task status array
    vPortFree(taskStatusArray);

    // sort s_tasks by the latest durations
    std::sort(s_tasks.begin(), s_tasks.end(), [](const TaskInfo& a, const TaskInfo& b)
              { return a.latestDuration > b.latestDuration; });

    // Print header
    // clang-format off
    printf("+----------------------+---------+----------+-------------+----------+-----------+\n");
    printf("| Task Name            | Util(%%) | Total(%%) | HighWater   | Priority | State     |\n");
    printf("+----------------------+---------+----------+-------------+----------+-----------+\n");
    // clang-format on

    // Print tasks
    for (const auto& task : s_tasks)
    {
        const char* stateStr;
        switch (task.state)
        {
            case eRunning:
                stateStr = "Running";
                break;
            case eReady:
                stateStr = "Ready";
                break;
            case eBlocked:
                stateStr = "Blocked";
                break;
            case eSuspended:
                stateStr = "Suspended";
                break;
            case eDeleted:
                stateStr = "Deleted";
                break;
            default:
                stateStr = "Unknown";
                break;
        }

        float totalUtilization =
            (allTasksTotalTime > 0) ? (task.lastRunTime * 100.0f / allTasksTotalTime) : 0.0f;

        float latestUtilization = (allTasksTotalDuration > 0 && task.latestDuration > 0)
                                      ? (task.latestDuration * 100.0f / allTasksTotalDuration)
                                      : 0.0f;

        printf("| %-20s | %6.1f%% | %7.1f%% | %11u | %8u | %-9s |\n", task.name, latestUtilization,
               totalUtilization, (unsigned int) task.stackHighWaterMark,
               (unsigned int) task.priority, stateStr);
    }

    // clang-format off
    printf("+----------------------+---------+----------+-------------+----------+-----------+\n");
    // clang-format on

    return 0;
}
esp_console_cmd_t top_cmd = {
    .command        = "top",
    .help           = "Show task information",
    .hint           = NULL,
    .func           = topCmd,
    .argtable       = NULL,
    .func_w_context = NULL,
    .context        = NULL,
};
#endif // CONFIG_CONSOLE_COMMANDS_TOP

void ConsoleCommandsInit()
{
    // init console
    esp_console_repl_t*       repl        = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt                    = "spotify>";

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));
#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
#else
#error Unsupported console type
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    // Register commands
#ifdef CONFIG_CONSOLE_COMMANDS_REBOOT
    ESP_ERROR_CHECK(esp_console_cmd_register(&reboot_cmd));
#endif // CONFIG_CONSOLE_COMMANDS_REBOOT

#ifdef CONFIG_CONSOLE_COMMANDS_TOP
    ESP_ERROR_CHECK(esp_console_cmd_register(&top_cmd));
#endif // CONFIG_CONSOLE_COMMANDS_TOP
}
