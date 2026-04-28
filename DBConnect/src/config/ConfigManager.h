#pragma once
#include <QObject>

namespace botc::config
{
    class ConfigManager final : public QObject
    {
        Q_OBJECT

    public:
        static ConfigManager* instance()
        {
            static auto _instance = ConfigManager();
            return &_instance;
        };

        [[nodiscard]] QVector<QString> loadConfig();
        [[nodiscard]] bool saveConfig();

    private:
        QString m_apiUrl = "https://localhost:443";
        QString m_apiKey;
        bool m_bSslVerify = false;
        QString m_iniPath = ".env";
    };
}
