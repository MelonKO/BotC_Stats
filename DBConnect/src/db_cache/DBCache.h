#pragma once
#include "QNetworkReply"
#include "QObject"

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

        void onRoleListFinished(const OpenAPI::OAIRolesResponse& summary,
                                QNetworkReply::NetworkError error_type,
                                const QString& error_str);

        void onPlayerListFinished(const OpenAPI::OAIPlayersResponse& summary,
                                  QNetworkReply::NetworkError error_type, const QString& error_str);

    private:
        QStringList m_cachedRoles{};
        QStringList m_cachedPlayers;
    };
}
