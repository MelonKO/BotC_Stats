#pragma once
#include "QNetworkReply"
#include "QObject"

#include <optional>

namespace OpenAPI
{
    class OAIPlayersResponse;
    class OAIRolesResponse;
}

namespace botc
{
    class DBCache final : public QObject
    {
        Q_OBJECT

    public:
        static DBCache* instance()
        {
            static auto _instance = DBCache{};
            return &_instance;
        };

        void refreshAll();

        void updateRoles(const std::optional<QString>& lang);
        const QStringList& getRolesCache() const { return m_cachedRoles; }

        void updatePlayers();
        const QStringList& getPlayersCache() const { return m_cachedPlayers; }

    signals:
        void onRolesCacheUpdated();
        void onPlayersCacheUpdated();

    private:
        DBCache();

        // Only one list request per resource is in flight at a time. A refresh
        // requested meanwhile is remembered and sent after the current reply, so
        // a slower, older reply can never overwrite newer data.
        void requestRoles();
        void requestPlayers();

        void onRoleListFinished(const OpenAPI::OAIRolesResponse& summary,
                                QNetworkReply::NetworkError error_type,
                                const QString& error_str);

        void onPlayerListFinished(const OpenAPI::OAIPlayersResponse& summary,
                                  QNetworkReply::NetworkError error_type, const QString& error_str);

    private:
        QStringList m_cachedRoles{};
        QStringList m_cachedPlayers;
        std::optional<QString> m_rolesLang{QString("ru")};
        bool m_rolesRequestInFlight = false;
        bool m_rolesRefreshPending = false;
        bool m_playersRequestInFlight = false;
        bool m_playersRefreshPending = false;
    };
}
