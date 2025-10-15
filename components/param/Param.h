/*
 * FileName: Param.h
 *
 * Author:   Parker Owen   <jpowen898@gmail.com>
 *
 * Description: A single param stored in non volatile storage
 */

#ifndef __PARAM_H__
#define __PARAM_H__

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <NvsMgr.h>
#include <IParam.h>

namespace params
{

template <typename T> class Param : public IParam
{
public:
    Param(const char* paramName, T default_value, bool storeInFlash = true)
        : IParam(paramName), DEFAULT_VAL(default_value), STORE_IN_FLASH{storeInFlash},
          m_param(default_value)
    {
        if (STORE_IN_FLASH)
            getFromNvs();
    }
    T& get()
    {
        return m_param;
    }

    esp_err_t set(T val)
    {
        m_param = val;
        return save();
    }
    esp_err_t save() override
    {
        if (STORE_IN_FLASH)
            return NvsMgr::getInstance().set(m_paramName.c_str(), &m_param, sizeof(m_param));
        return ESP_ERR_INVALID_STATE; // Not stored in flash
    }
    esp_err_t getFromNvs() override
    {
        size_t memSize = sizeof(m_param);
        return NvsMgr::getInstance().get(m_paramName.c_str(), &m_param, memSize);
    }
    esp_err_t setToDefault() override
    {
        return set(DEFAULT_VAL);
    }
    esp_err_t clean() override
    {
        return NvsMgr::getInstance().erase(m_paramName.c_str());
    }
    virtual bool storedInFlash() override
    {
        return STORE_IN_FLASH;
    }

    std::string toString() const override
    {
        return std::string("");
    }
    virtual esp_err_t setFromString(const std::string&) override
    {
        return ESP_ERR_NOT_SUPPORTED;
    }

private:
    const T    DEFAULT_VAL;
    const bool STORE_IN_FLASH;
    T          m_param; // The actual parameter value
};

/********************** std::string parameter template functions **********************/
template <> inline esp_err_t params::Param<std::string>::save()
{
    return NvsMgr::getInstance().set(m_paramName.c_str(), (void*) &m_param.at(0), m_param.length());
}
template <> inline esp_err_t params::Param<std::string>::getFromNvs()
{
    char      buf[150];
    size_t    length = 150;
    esp_err_t err    = NvsMgr::getInstance().get(m_paramName.c_str(), buf, length);
    if (err != ESP_OK)
    {
        return err; // Return error if not found or other error
    }
    if (length > 0)
    {
        buf[length] = 0;
        m_param     = std::string((const char*) buf);
    }
    else
    {
        m_param = std::string("");
    }
    return err;
}
template <> inline std::string params::Param<std::string>::toString() const
{
    return m_param;
}
template <> inline esp_err_t params::Param<std::string>::setFromString(const std::string& value)
{
    m_param = value;
    return save();
}

/********************** int parameter template functions **********************/
template <> inline std::string params::Param<int>::toString() const
{
    return std::to_string(m_param);
}
template <> inline esp_err_t params::Param<int>::setFromString(const std::string& value)
{
    int ret = sscanf(value.c_str(), "%d", &m_param);
    if (ret != 1)
    {
        return ESP_ERR_INVALID_ARG; // Error converting to string
    }
    return save();
}

/********************** bool parameter template functions **********************/
template <> inline std::string params::Param<bool>::toString() const
{
    return std::string((m_param) ? "true" : "false");
}
template <> inline esp_err_t params::Param<bool>::setFromString(const std::string& value)
{
    if (value == "true" || value == "1")
    {
        m_param = true;
    }
    else if (value == "false" || value == "0")
    {
        m_param = false;
    }
    else
    {
        int ret = sscanf(value.c_str(), "%d", (int*) &m_param);
        if (ret != 1)
        {
            return ESP_ERR_INVALID_ARG; // Error converting to string
        }
    }
    return save();
}

} // namespace params

#endif // __PARAM_H__