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

        [[nodiscard]] bool loadConfig();

        const QString& getApiUrl() const { return m_apiUrl; }
        const QString& getApiKey() const { return m_apiKey; }
        bool getSslVerify() const { return m_bSslVerify; }

    private:
        bool ParseConfig();

    private:
        QString m_apiUrl = "https://localhost:443";
        QString m_apiKey;
        bool m_bSslVerify = false;
        QString m_iniPath = "config.ini";
    };
}
