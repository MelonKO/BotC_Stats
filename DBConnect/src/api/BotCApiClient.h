#pragma once

#include "QNetworkReply"

#include <optional>

    namespace OpenAPI
    {
        class OAIPlayersResponse;
        class OAIPlayersApi;
        class OAIPlayerItem;
        class OAIPlayerUpdateRequest;
        class OAIRoleItem;
        class OAIRoleUpdateRequest;
        class OAIRolesImportRequest;
    class OAIRolesImportResponse;
    class OAIRolesResponse;
    class OAIGameImportStatusResponse;
    class OAIGameImportRequest;
    class OAIGamesImportRequest;
    class OAIGamesImportStatusResponse;
    class OAIHealthResponse;
    class OAISystemApi;
    class OAIRolesApi;
    class OAIGamesApi;
}

namespace botc::api
{
    class BotCNetworkAccessManager;

    class BotCApiClient final : public QObject
    {
        Q_OBJECT

    public:
        static BotCApiClient* instance()
        {
            static auto _instance = BotCApiClient();
            return &_instance;
        }

        void init(const QString& baseUrl, const QString& apiKey, bool bSslVerify = false);

        // config

        void setBaseUrl(const QString& baseUrl);
        void setApiKey(const QString& apiKey);
        void setSslVerify(bool bSslVerify);

        // API

        void healthCheck() const;
        void importGame(const OpenAPI::OAIGameImportRequest& importGameRequest) const;
        void importGamesBatch(const OpenAPI::OAIGamesImportRequest& request) const;
        void listRoles(const std::optional<QString>& lang) const;
        void importRoles(const OpenAPI::OAIRolesImportRequest& request) const;
        void listPlayers() const;
        void updatePlayer(const QString& playerId, const OpenAPI::OAIPlayerUpdateRequest& request) const;
        void updateRole(const QString& roleId, const OpenAPI::OAIRoleUpdateRequest& request,
                        const std::optional<QString>& lang) const;

    signals:
        // callbacks

        void healthCheckFinished(const OpenAPI::OAIHealthResponse& summary,
                                 QNetworkReply::NetworkError error_type, const QString& error_str);
        void gameImportFinished(const OpenAPI::OAIGameImportStatusResponse& summary,
                                QNetworkReply::NetworkError error_type, const QString& error_str);
        void gamesImportBatchFinished(const OpenAPI::OAIGamesImportStatusResponse& summary,
                                      QNetworkReply::NetworkError error_type, const QString& error_str);
        void rolesListFinished(const OpenAPI::OAIRolesResponse& summary,
                               QNetworkReply::NetworkError error_type, const QString& error_str);
        void rolesImportFinished(const OpenAPI::OAIRolesImportResponse& summary,
                                 QNetworkReply::NetworkError error_type, const QString& error_str);
        void playersListFinished(const OpenAPI::OAIPlayersResponse& summary,
                                 QNetworkReply::NetworkError error_type, const QString& error_str);
        // http_status is the HTTP response code, or 0 when no response arrived
        // (network failure, timeout). The player and roles views use it to map
        // conflict, missing-record, authentication and validation responses.
        void playerUpdateFinished(const OpenAPI::OAIPlayerItem& player,
                                  QNetworkReply::NetworkError error_type,
                                  int http_status, const QString& error_str);
        void roleUpdateFinished(const OpenAPI::OAIRoleItem& role,
                                QNetworkReply::NetworkError error_type,
                                int http_status, const QString& error_str);

    private:
        void initSystemAPI();
        void initGameAPI();
        void initRolesAPI();
        void initPlayersAPI();

        void handleHealthCheckReply(const OpenAPI::OAIHealthResponse& summary,
                                    QNetworkReply::NetworkError error_type, const QString& error_str);
        void handleGameImportReply(const OpenAPI::OAIGameImportStatusResponse& summary,
                                   QNetworkReply::NetworkError error_type, const QString& error_str);
        void handleGamesBatchImportReply(const OpenAPI::OAIGamesImportStatusResponse& summary,
                                         QNetworkReply::NetworkError error_type, const QString& error_str);
        void handleRolesListReply(const OpenAPI::OAIRolesResponse& summary,
                                  QNetworkReply::NetworkError error_type, const QString& error_str);
        void handleRolesImportReply(const OpenAPI::OAIRolesImportResponse& summary,
                                    QNetworkReply::NetworkError error_type, const QString& error_str);

        void handlePlayersListReply(const OpenAPI::OAIPlayersResponse& summary,
                                    QNetworkReply::NetworkError error_type, const QString& error_str);

        void handlePlayerUpdateReply(const OpenAPI::OAIPlayerItem& player,
                                     QNetworkReply::NetworkError error_type,
                                     int http_status, const QString& error_str);
        void handleRoleUpdateReply(const OpenAPI::OAIRoleItem& role,
                                   QNetworkReply::NetworkError error_type,
                                   int http_status, const QString& error_str);

    private:
        BotCApiClient() = default;

    private:
        QString m_baseUrl{};
        QString m_apiKey{};
        // Owns the TLS certificate policy; see BotCNetworkAccessManager.
        BotCNetworkAccessManager* m_networkManager = nullptr;

        OpenAPI::OAIGamesApi* m_gameAPI      = nullptr;
        OpenAPI::OAIRolesApi* m_rolesAPI     = nullptr;
        OpenAPI::OAISystemApi* m_systemAPI   = nullptr;
        OpenAPI::OAIPlayersApi* m_playersAPI = nullptr;
    };
}
