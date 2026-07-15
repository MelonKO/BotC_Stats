#include "DBCache.h"

#include "OAIPlayersResponse.h"
#include "OAIRolesResponse.h"
#include "OAIRoleItem.h"
#include "qeventloop.h"
#include "api/BotCApiClient.h"

namespace botc
{
    void DBCache::updateRoles(const std::optional<QString>& lang) const
    {
        const auto* apiClient = api::BotCApiClient::instance();
        connect(apiClient, &api::BotCApiClient::rolesListFinished,
                this, &DBCache::onRoleListFinished, Qt::SingleShotConnection);
        apiClient->listRoles(lang);
    }

    void DBCache::updatePlayers() const
    {
        const auto* apiClient = api::BotCApiClient::instance();
        connect(apiClient, &api::BotCApiClient::playersListFinished,
                this, &DBCache::onPlayerListFinished, Qt::SingleShotConnection);
        apiClient->listPlayers();
    }

    void DBCache::onRoleListFinished(const OpenAPI::OAIRolesResponse& summary,
                                     const QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        if (error_type != QNetworkReply::NoError) { m_cachedRoles.clear(); }

        std::ranges::transform(
            summary.getRoles(),
            std::inserter(m_cachedRoles, m_cachedRoles.end()),
            [](const OpenAPI::OAIRoleItem& role) -> QString
            {
                QString transName = role.getTranslation().getName();
                return transName.isEmpty() ? role.getName() : transName;
            }
        );
    }

    void DBCache::onPlayerListFinished(const OpenAPI::OAIPlayersResponse& summary,
                                       QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        if (error_type != QNetworkReply::NoError) { m_cachedPlayers.clear(); }

        std::ranges::transform(
            summary.getPlayers(),
            std::inserter(m_cachedPlayers, m_cachedPlayers.end()),
            [](const OpenAPI::OAIPlayerItem& player) -> QString
            {
                return player.getName();
            });
    }
}
