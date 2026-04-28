#include <QFile>
#include "ConfigManager.h"

#include <qtextstream.h>

// TODO:: use iniParser
namespace botc::config
{
    QVector<QString> ConfigManager::loadConfig()
    {
        QVector<QString> errors;
        if (!QFile::exists(m_envPath))
        {
            saveConfig();
            return errors;
        }

        errors = parseEnvFile();

        return errors;
    }
}
