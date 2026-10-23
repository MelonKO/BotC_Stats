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
        // Permanent connections keep the cache in sync with every reply. A
        // per-request single-shot connection would be consumed by the first
        // reply, so an overlapping refresh could lose its own reply. Overlapping
        // refreshes are also coalesced (see updateRoles()/updatePlayers()): at
        // most one list request per resource is in flight, so the cache always
        // ends with the newest data even if an older reply arrives last.
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
        // Remember the language so a queued refresh repeats the same request.
        m_rolesLang = lang;
        if (m_rolesRequestInFlight)
        {
            // One queued refresh is enough: it is sent right after the reply in
            // flight, so the cache ends with the newest data and no extra copies
            // of the same request travel over the network.
            m_rolesRefreshPending = true;
            return;
        }

        requestRoles();
    }

    void DBCache::updatePlayers()
    {
        if (m_playersRequestInFlight)
        {
            m_playersRefreshPending = true;
            return;
        }

        requestPlayers();
    }

    void DBCache::requestRoles()
    {
        m_rolesRequestInFlight = true;
        api::BotCApiClient::instance()->listRoles(m_rolesLang);
    }

    void DBCache::requestPlayers()
    {
        m_playersRequestInFlight = true;
        api::BotCApiClient::instance()->listPlayers();
    }

    void DBCache::onRoleListFinished(const OpenAPI::OAIRolesResponse& summary,
                                     const QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        m_rolesRequestInFlight = false;

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

        // Re-request before notifying listeners: a listener that refreshes again
        // from onRolesCacheUpdated() must be coalesced into the pending flag
        // instead of starting yet another request.
        if (m_rolesRefreshPending)
        {
            m_rolesRefreshPending = false;
            requestRoles();
        }

        emit onRolesCacheUpdated();
    }

    void DBCache::onPlayerListFinished(const OpenAPI::OAIPlayersResponse& summary,
                                       QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        m_playersRequestInFlight = false;

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

        // Re-request before notifying listeners, as in onRoleListFinished().
        if (m_playersRefreshPending)
        {
            m_playersRefreshPending = false;
            requestPlayers();
        }

        emit onPlayersCacheUpdated();
    }
}
