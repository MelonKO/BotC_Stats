#include "DBCache.h"

#include <algorithm>

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

    void DBCache::applyPlayerRename(const QString& playerId, const QString& name)
    {
        const auto entry = std::ranges::find_if(m_playerEntries, [&playerId](const PlayerEntry& player)
        {
            return player.id == playerId;
        });
        if (entry == m_playerEntries.end()) return;

        entry->name = name;

        // The name list mirrors the entries, so rebuild it instead of keeping a
        // second copy in sync by hand.
        m_cachedPlayers.clear();
        m_cachedPlayers.reserve(m_playerEntries.size());
        for (const PlayerEntry& player : m_playerEntries)
        {
            m_cachedPlayers.append(player.name);
        }

        emit onPlayersCacheUpdated();
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
        m_rolesError.clear();
        m_rolesErrorType = QNetworkReply::NoError;
        emit rolesRequestStarted();
        api::BotCApiClient::instance()->listRoles(m_rolesLang);
    }

    void DBCache::requestPlayers()
    {
        m_playersRequestInFlight = true;
        // Clear the previous error: the view must not report a failure that is
        // already being retried.
        m_playersError.clear();
        m_playersErrorType = QNetworkReply::NoError;
        api::BotCApiClient::instance()->listPlayers();
    }

    void DBCache::onRoleListFinished(const OpenAPI::OAIRolesResponse& summary,
                                     const QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        m_rolesRequestInFlight = false;

        // Replace the previous cache before filling it so successive refreshes cannot duplicate entries.
        m_cachedRoles.clear();
        m_roleEntries.clear();
        if (error_type == QNetworkReply::NoError)
        {
            const QList<OpenAPI::OAIRoleItem> roles = summary.getRoles();
            m_roleEntries.reserve(roles.size());
            m_cachedRoles.reserve(roles.size());
            for (const OpenAPI::OAIRoleItem& role : roles)
            {
                RoleEntry entry{
                    role.getId(),
                    role.getName(),
                    role.getRoleType(),
                    role.getRoleTypeTranslation(),
                    role.getAlignment(),
                    role.getAlignmentTranslation(),
                    role.getDescription(),
                    {},
                    {}
                };
                if (role.is_translation_Set())
                {
                    const OpenAPI::OAIRoleTranslation translation = role.getTranslation();
                    entry.translationName = translation.getName();
                    entry.translationDescription = translation.getDescription();
                }
                m_cachedRoles.append(entry.displayName());
                m_roleEntries.append(entry);
            }
        }
        else
        {
            m_rolesError = error_str;
            m_rolesErrorType = error_type;
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
        m_playerEntries.clear();
        if (error_type == QNetworkReply::NoError)
        {
            const QList<OpenAPI::OAIPlayerItem> players = summary.getPlayers();
            m_cachedPlayers.reserve(players.size());
            m_playerEntries.reserve(players.size());
            for (const OpenAPI::OAIPlayerItem& player : players)
            {
                m_cachedPlayers.append(player.getName());
                m_playerEntries.append(PlayerEntry{player.getId(), player.getName()});
            }
        }
        else
        {
            // Keep the reason: an empty list without it would look like an empty
            // database instead of an unreachable API (KNOWN-ISSUES C9).
            m_playersError = error_str;
            m_playersErrorType = error_type;
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
