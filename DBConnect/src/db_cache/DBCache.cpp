#include "DBCache.h"

#include "OAIPlayersResponse.h"
#include "OAIRolesResponse.h"
#include "OAIRoleItem.h"
#include "qeventloop.h"
#include "api/BotCApiClient.h"

namespace botc
{
    DBCache::DBCache()
    {
        // Permanent connections keep the cache in sync with every reply.
        // A per-request single-shot connection would be consumed by the first
        // reply, so overlapping refreshes (an import or a settings change while
        // the previous list request is still in flight) could leave the cache
        // stale: the newer reply would arrive with no handler left.
        const auto* apiClient = api::BotCApiClient::instance();
        connect(apiClient, &api::BotCApiClient::rolesListFinished,
                this, &DBCache::onRoleListFinished);
        connect(apiClient, &api::BotCApiClient::playersListFinished,
                this, &DBCache::onPlayerListFinished);
    }

    void DBCache::refreshAll()
    {
        updateRoles(QString("ru"));
        updatePlayers();
    }

    void DBCache::updateRoles(const std::optional<QString>& lang)
    {
        api::BotCApiClient::instance()->listRoles(lang);
    }

    void DBCache::updatePlayers()
    {
        api::BotCApiClient::instance()->listPlayers();
    }

    void DBCache::onRoleListFinished(const OpenAPI::OAIRolesResponse& summary,
                                     const QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        // Replace the previous cache before filling it so successive refreshes cannot duplicate entries.
        m_cachedRoles.clear();
        if (error_type == QNetworkReply::NoError)
        {
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

        emit onRolesCacheUpdated();
    }

    void DBCache::onPlayerListFinished(const OpenAPI::OAIPlayersResponse& summary,
                                       QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        // Replace the previous cache before filling it so successive refreshes cannot duplicate entries.
        m_cachedPlayers.clear();
        if (error_type == QNetworkReply::NoError)
        {
            std::ranges::transform(
                summary.getPlayers(),
                std::inserter(m_cachedPlayers, m_cachedPlayers.end()),
                [](const OpenAPI::OAIPlayerItem& player) -> QString
                {
                    return player.getName();
                });
        }

        emit onPlayersCacheUpdated();
    }
}
