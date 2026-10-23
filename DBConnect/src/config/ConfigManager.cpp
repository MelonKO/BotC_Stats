#include "ConfigManager.h"

// TODO:: use iniParser
namespace botc::config
{
    ServerSettings ConfigManager::getServerSettings() const
    {
        ServerSettings settings;
        settings.apiUrl = m_settings.value("connection/api_url", "https://localhost:443").toString();
        settings.apiKey = m_settings.value("connection/api_key", "").toString();
        settings.bSslVerify = m_settings.value("connection/ssl_verify", false).toBool();
        return settings;
    }

    void ConfigManager::setServerSettings(const ServerSettings& settings)
    {
        // Saving the tab without edits must not trigger server requests.
        if (settings == getServerSettings())
        {
            return;
        }

        m_settings.setValue("connection/api_url", settings.apiUrl);
        m_settings.setValue("connection/api_key", settings.apiKey);
        m_settings.setValue("connection/ssl_verify", settings.bSslVerify);

        // A single signal for the whole change: receivers apply the new values
        // and reload their caches without any debouncing.
        emit serverSettingsChanged(settings);
    }
}
