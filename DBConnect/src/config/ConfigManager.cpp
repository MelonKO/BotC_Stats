#include <QFile>
#include "ConfigManager.h"

#include "../utils/IniParser.h"

// TODO:: use iniParser
namespace botc::config
{
    bool ConfigManager::loadConfig()
    {
        if (!QFile::exists(m_iniPath))
        {
            return false;
        }

        return ParseConfig();
    }

    bool ConfigManager::ParseConfig()
    {
        using namespace botc::utils;
        IniParser parser{m_iniPath};
        if (!parser.IsValid())
        {
            return false;
        }

        m_apiUrl     = parser.GetString(u"connection:api_url", nullptr);
        m_apiKey     = parser.GetString(u"connection:api_key", nullptr);
        m_bSslVerify = parser.GetBool(u"connection:ssl_verify", false);

        return true;
    }
}
