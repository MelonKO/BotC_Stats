#pragma once
#include "QNetworkReply"
#include "QObject"

#include <optional>

#include <QString>
#include <QVector>

namespace OpenAPI
{
    class OAIPlayersResponse;
    class OAIRolesResponse;
}

namespace botc
{
    /**
     * A player record as returned by the players list endpoint.
     *
     * The id is the stable database key: a name may be changed later, the id never is.
     */
    struct PlayerEntry
    {
        QString id;
        QString name;
    };

    /**
     * A role record with its reference values and optional server translations.
     */
    struct RoleEntry
    {
        QString id;
        QString name;
        QString roleType;
        QString roleTypeTranslation;
        QString alignment;
        QString alignmentTranslation;
        QString description;
        QString translationName;
        QString translationDescription;

        QString displayName() const { return translationName.isEmpty() ? name : translationName; }
    };

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
        /** Full role records used by the roles view. */
        const QVector<RoleEntry>& getRoleEntries() const { return m_roleEntries; }

        /** Error of the last roles list request; empty when that request succeeded. */
        const QString& getRolesError() const { return m_rolesError; }

        /** Network error of the last roles list request, NoError when it succeeded. */
        QNetworkReply::NetworkError getRolesErrorType() const { return m_rolesErrorType; }

        /** True while a roles list request is in flight. */
        bool isRolesRequestInFlight() const { return m_rolesRequestInFlight; }

        void updatePlayers();
        const QStringList& getPlayersCache() const { return m_cachedPlayers; }

        /**
         * Apply a confirmed rename to the cached records.
         *
         * The server has already accepted the new name, so the cache is patched
         * in place: a refresh requested afterwards confirms it instead of briefly
         * showing the old name again.
         */
        void applyPlayerRename(const QString& playerId, const QString& name);

        /** Full player records (id and name), used by the players view. */
        const QVector<PlayerEntry>& getPlayerEntries() const { return m_playerEntries; }

        /** Error of the last players list request; empty when that request succeeded. */
        const QString& getPlayersError() const { return m_playersError; }

        /** Network error of the last players list request, NoError when it succeeded. */
        QNetworkReply::NetworkError getPlayersErrorType() const { return m_playersErrorType; }

        /** True while a players list request is in flight. */
        bool isPlayersRequestInFlight() const { return m_playersRequestInFlight; }

    signals:
        /** Emitted when a roles request is sent, not when one is only queued. */
        void rolesRequestStarted();
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
        QVector<RoleEntry> m_roleEntries{};
        QString m_rolesError{};
        QNetworkReply::NetworkError m_rolesErrorType = QNetworkReply::NoError;
        QStringList m_cachedPlayers;
        // Same rows as m_cachedPlayers, but with the player id kept alongside the
        // name: the import widget only needs names, the players view needs ids.
        QVector<PlayerEntry> m_playerEntries{};
        QString m_playersError{};
        QNetworkReply::NetworkError m_playersErrorType = QNetworkReply::NoError;
        std::optional<QString> m_rolesLang{QString("ru")};
        bool m_rolesRequestInFlight = false;
        bool m_rolesRefreshPending = false;
        bool m_playersRequestInFlight = false;
        bool m_playersRefreshPending = false;
    };
}
