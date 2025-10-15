/*
 * FileName: IParam.h
 *
 * Author:   Parker Owen   <jpowen898@gmail.com>
 *
 * Description: Interface for a nvs param
 */

#ifndef __I_NVS_PARAM_H__
#define __I_NVS_PARAM_H__

#include <string>
#include <esp_err.h>

namespace params {

class IParam {
public:
    virtual ~IParam() {}

    virtual esp_err_t save()             = 0;
    virtual esp_err_t clean()            = 0;
    virtual esp_err_t getFromNvs()       = 0;
    virtual esp_err_t setToDefault()     = 0;
    virtual std::string toString() const = 0;
    virtual bool storedInFlash()         = 0;
    std::string getParamName() const;
    virtual esp_err_t setFromString(const std::string &value) = 0;

protected:
    IParam(const char *name);

    std::string m_paramName;  // Name of the parameter
};

}  // namespace params
#endif  // __I_NVS_PARAM_H__