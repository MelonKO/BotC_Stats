#pragma once
#include "QNetworkReply"
#include "QObject"

namespace OpenAPI
{
    class OAIListPlayers_200_response;
    class OAIListRoles_200_response;
}

namespace botc
{
    class DBCache final : public QObject
    {
    public:
        static DBCache* instance()
        {
            static auto _instance = DBCache{};
            return &_instance;
        };

        void updateRoles(const std::optional<QString>& lang) const;
        const QStringList& getRolesCache() const { return m_cachedRoles; }

        void updatePlayers() const;
        const QStringList& getPlayersCache() const { return m_cachedPlayers; }

    signals:
        void onRolesCacheUpdated();

    private:
        DBCache() = default;

        void onRoleListFinished(const OpenAPI::OAIListRoles_200_response& summary,
                                QNetworkReply::NetworkError error_type,
                                const QString& error_str);

        void onPlayerListFinished(const OpenAPI::OAIListPlayers_200_response& summary,
                                  QNetworkReply::NetworkError error_type, const QString& error_str);

    private:
        QStringList m_cachedRoles{};
        QStringList m_cachedPlayers;
    };
}
