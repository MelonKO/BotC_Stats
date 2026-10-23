#pragma once
#include <QObject>
#include <QSettings>

namespace botc::config
{
    // Everything that selects and authenticates against a server. Kept as one
    // value so that saving the settings tab is a single, atomic change.
    struct ServerSettings
    {
        QString apiUrl;
        QString apiKey;
        bool bSslVerify = false;

        bool operator==(const ServerSettings& other) const = default;
    };

    class ConfigManager final : public QObject
    {
        Q_OBJECT

    public:
        static ConfigManager* instance()
        {
            static auto _instance = ConfigManager();
            return &_instance;
        };

        [[nodiscard]] ServerSettings getServerSettings() const;

        // Writes all server settings and emits serverSettingsChanged() once, or
        // does nothing when the values are unchanged.
        void setServerSettings(const ServerSettings& settings);

    signals:
        void serverSettingsChanged(const botc::config::ServerSettings& settings);

    private:
        ConfigManager() : m_settings(m_iniPath, QSettings::IniFormat)
        {
        }

    private:
        QString m_iniPath = "config.ini";
        QSettings m_settings;
    };
}
