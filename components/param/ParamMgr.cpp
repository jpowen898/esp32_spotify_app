/*
 * FileName: ParamMgr.cpp
 *
 * Author:   Parker Owen   <jpowen898@gmail.com>
 *
 * Description: Manages all params on the system
 */

#include <ParamMgr.h>
#include <NvsMgr.h>
#include <iostream>
#include <string.h>
#include "esp_console.h"
#include "argtable3/argtable3.h"

using namespace params;

void param_cmd_init();

/****************************************************************************/
ParamMgr::ParamMgr()
{
    param_cmd_init();
}
/****************************************************************************/
void ParamMgr::registerParam(IParam* param)
{
    m_params.push_back(param);
}
/****************************************************************************/
void ParamMgr::listAll()
{
    std::cout << "**************************** Params ***************************" << std::endl;

    // ANSI escape codes for colors
    const std::string green  = "\033[32m"; // Green text
    const std::string yellow = "\033[33m"; // Yellow text
    const std::string reset  = "\033[0m";  // Reset to default color

    for (const auto& param : m_params)
    {
        if (param->storedInFlash())
        {
            std::cout << green << param->getParamName() << " = " << param->toString() << reset
                      << std::endl;
        }
        else
        {
            std::cout << yellow << param->getParamName() << " = " << param->toString() << reset
                      << std::endl;
        }
    }
    std::cout << "***************************************************************" << std::endl;
}

/****************************************************************************/
esp_err_t ParamMgr::saveAll()
{
    esp_err_t err = ESP_OK;
    for (const auto& param : m_params)
    {
        err = param->save();
        if (err)
        {
            return err;
        }
    }
    return err;
}
/****************************************************************************/
esp_err_t ParamMgr::readAll()
{
    esp_err_t err = ESP_OK;
    for (const auto& param : m_params)
    {
        err = param->getFromNvs();
        if (err)
        {
            return err;
        }
    }
    return err;
}
/****************************************************************************/
esp_err_t ParamMgr::eraseAll()
{
    return NvsMgr::getInstance().eraseAll();
}
/****************************************************************************/
esp_err_t ParamMgr::resetDefaultAll()
{
    esp_err_t err = ESP_OK;
    for (const auto& param : m_params)
    {
        err = param->setToDefault();
        if (err)
        {
            return err;
        }
    }
    return err;
}
/****************************************************************************/
esp_err_t ParamMgr::setParam(const std::string& name, const std::string& value)
{
    for (const auto& param : m_params)
    {
        if (param->getParamName() == name)
        {
            return param->setFromString(value);
        }
    }
    return ESP_ERR_NOT_FOUND; // Parameter not found
}
/****************************************************************************/
IParam* ParamMgr::getParam(const std::string& name)
{
    for (const auto& param : m_params)
    {
        if (param->getParamName() == name)
        {
            return param;
        }
    }
    return nullptr; // Parameter not found
}
/****************************************************************************/

/****************************************************************************/
/**************************** NVS Console Command ***************************/
/****************************************************************************/
int param_cmd(int argc, char** argv);

static struct
{
    struct arg_str* action;
    struct arg_str* name;
    struct arg_str* value;
    struct arg_end* end;
} s_param_cmd_args;

static esp_console_cmd_t s_param_cmd_struct{
    .command = "param",
    .help    = "param command: 'list', 'read', 'save', 'erase', 'default', or 'set <name> <value>'",
    .hint    = NULL,
    .func    = &param_cmd,
    .argtable = &s_param_cmd_args,
    .context  = NULL,
};

/****************************************************************************/
void param_cmd_init()
{
    s_param_cmd_args.action =
        arg_str1(NULL, NULL, "<action>", "'list', 'read', 'save', 'erase', 'default', or 'set'");
    s_param_cmd_args.name  = arg_str0(NULL, NULL, "<name>", "Parameter name (used with 'set')");
    s_param_cmd_args.value = arg_str0(NULL, NULL, "<value>", "Parameter value (used with 'set')");
    s_param_cmd_args.end   = arg_end(3); // up to 3 arguments
    ESP_ERROR_CHECK(esp_console_cmd_register(&s_param_cmd_struct));
}

/****************************************************************************/
int param_cmd(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &s_param_cmd_args);
    if (nerrors != 0)
    {
        arg_print_errors(stderr, s_param_cmd_args.end, argv[0]);
        printf("Usage: param set <name> <value> or other valid actions.\n");
        return 1;
    }

    const char* action = s_param_cmd_args.action->sval[0];

    if (strcmp(action, "list") == 0)
    {
        ParamMgr::getInstance().listAll();
    }
    else if (strcmp(action, "save") == 0)
    {
        ParamMgr::getInstance().saveAll();
    }
    else if (strcmp(action, "read") == 0)
    {
        ParamMgr::getInstance().readAll();
    }
    else if (strcmp(action, "erase") == 0)
    {
        ParamMgr::getInstance().eraseAll();
    }
    else if (strcmp(action, "default") == 0)
    {
        // check for param name
        if (!s_param_cmd_args.name->sval[0])
        {
            ParamMgr::getInstance().resetDefaultAll();
            return 0;
        }
        else
        {
            std::string name = s_param_cmd_args.name->sval[0];
            if (name.empty())
            {
                ParamMgr::getInstance().resetDefaultAll();
                return 0;
            }

            IParam* param = ParamMgr::getInstance().getParam(name);
            if (param)
            {
                param->setToDefault();
                return 0;
            }
            else
            {
                printf("Error: Parameter '%s' not found.\n", name.c_str());
                return 1;
            }
        }
    }
    else if (strcmp(action, "set") == 0)
    {
        if (!s_param_cmd_args.name->sval[0] || !s_param_cmd_args.value->sval[0])
        {
            printf("Error: 'set' requires both <name> and <value>.\n");
            return 1;
        }
        std::string name  = s_param_cmd_args.name->sval[0];
        std::string value = s_param_cmd_args.value->sval[0];
        ParamMgr::getInstance().setParam(name, value);
    }
    else
    {
        printf("Invalid action. Use 'list', 'read', 'save', 'erase', 'default', or 'set <name> "
               "<value>'.\n");
        return 1;
    }

    return 0;
}
/****************************************************************************/