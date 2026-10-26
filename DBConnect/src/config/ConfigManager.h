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

        // Interface language preference: "auto" follows the operating system,
        // anything else is a language code such as "en" or "ru". Kept out of
        // ServerSettings: a language change must not reload server data.
        [[nodiscard]] QString getUiLanguage() const;
        void setUiLanguage(const QString& language);

    signals:
        void serverSettingsChanged(const botc::config::ServerSettings& settings);
        void uiLanguageChanged(const QString& language);

    private:
        ConfigManager() : m_settings(m_iniPath, QSettings::IniFormat)
        {
        }

    private:
        QString m_iniPath = "config.ini";
        QSettings m_settings;
    };
}
