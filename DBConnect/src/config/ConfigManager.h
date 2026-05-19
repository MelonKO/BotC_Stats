#pragma once
#include <QObject>
#include <QSettings>

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

        [[nodiscard]] QString getApiUrl() const
        {
            return m_settings.value("connection/api_url", "https://localhost:443").toString();
        }

        [[nodiscard]] QString getApiKey() const { return m_settings.value("connection/api_key", "").toString(); }
        [[nodiscard]] bool getSslVerify() const { return m_settings.value("connection/ssl_verify", false).toBool(); }

        void setApiUrl(const QString& in_newValue)
        {
            m_settings.setValue("connection/api_url", in_newValue);
            emit apiUrlChanged(in_newValue);
        }

        void setApiKey(const QString& in_newValue)
        {
            m_settings.setValue("connection/api_key", in_newValue);
            emit apiKeyChanged(in_newValue);
        }

        void setSslVerify(const bool b_inNewValue)
        {
            m_settings.setValue("connection/ssl_verify", b_inNewValue);
            emit sslVerifyChanged(b_inNewValue);
        }

    signals:
        void apiUrlChanged(const QString& in_newValue);
        void apiKeyChanged(const QString& in_newValue);
        void sslVerifyChanged(bool b_inNewValue);

    private:
        ConfigManager() : m_settings(m_iniPath, QSettings::IniFormat)
        {
        }

    private:
        QString m_iniPath = "config.ini";
        QSettings m_settings;
    };
}
