/*
 * FileName: ParamMgr.h
 *
 * Author:   Parker Owen   <jpowen898@gmail.com>
 *
 * Description: Manages all params on the system
 */

#ifndef __PARAM_MGR_H__
#define __PARAM_MGR_H__

#include <NvsMgr.h>
#include <IParam.h>
#include <vector>
#include <string>

namespace params
{

class ParamMgr;
class IParam;

class ParamMgr
{
public:
    static ParamMgr& getInstance()
    {
        static ParamMgr instance; // Singleton instance
        return instance;
    }

    void      listAll();
    esp_err_t saveAll();
    esp_err_t readAll();
    esp_err_t eraseAll();
    esp_err_t resetDefaultAll();
    esp_err_t setParam(const std::string& name, const std::string& value);
    IParam*   getParam(const std::string& name);

private:
    ParamMgr();
    ParamMgr(const ParamMgr&)            = delete;
    ParamMgr& operator=(const ParamMgr&) = delete;

    void registerParam(IParam* param);
    friend class IParam;

    std::vector<IParam*> m_params;
};
} // namespace params

#endif // __PARAM_MGR_H__