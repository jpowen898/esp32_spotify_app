/*
 * FileName: IParam.cpp
 *
 * Author:   Parker Owen   <jpowen898@gmail.com>
 *
 * Description: Interface for a nvs param
 */

#include <IParam.h>
#include <ParamMgr.h>

using namespace params;

/****************************************************************************/
IParam::IParam(const char *name) : m_paramName(name)
{
    ParamMgr::getInstance().registerParam(this);
}
/****************************************************************************/
std::string IParam::getParamName() const
{
    return m_paramName;
}
/****************************************************************************/