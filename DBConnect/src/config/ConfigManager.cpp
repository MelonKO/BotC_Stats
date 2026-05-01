#include <QFile>
#include "ConfigManager.h"

#include <iostream>

extern "C" {
#include "iniparser.h"
}

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
        const dictionary* config = iniparser_load(m_iniPath.toStdString().c_str());
        if (config == nullptr)
        {
            return false;
        }

        m_apiUrl     = iniparser_getstring(config, "connection:api_url", nullptr);
        m_apiKey     = iniparser_getstring(config, "connection:api_key", nullptr);
        m_bSslVerify = iniparser_getboolean(config, "connection:ssl_verify", false);

        return true;
    }
}
