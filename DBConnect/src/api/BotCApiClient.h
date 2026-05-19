#pragma once

#include "QNetworkReply"

namespace OpenAPI
{
    class OAIListPlayers_200_response;
    class OAIPlayersApi;
    class OAIImportRoles_request;
    class OAIImportRoles_200_response;
    class OAIListRoles_200_response;
    class OAIImportGame_200_response;
    class OAIImportGame_request;
    class OAIHealth_200_response;
    class OAISystemApi;
    class OAIRolesApi;
    class OAIGamesApi;
}

namespace botc::api
{
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
        void importGame(const OpenAPI::OAIImportGame_request& importGameRequest) const;
        void listRoles(const std::optional<QString>& lang) const;
        void importRoles(const OpenAPI::OAIImportRoles_request& request) const;
        void listPlayers() const;

    signals:
        // callbacks

        void healthCheckFinished(const OpenAPI::OAIHealth_200_response& summary,
                                 QNetworkReply::NetworkError error_type, const QString& error_str);
        void gameImportFinished(const OpenAPI::OAIImportGame_200_response& summary,
                                QNetworkReply::NetworkError error_type, const QString& error_str);
        void rolesListFinished(const OpenAPI::OAIListRoles_200_response& summary,
                               QNetworkReply::NetworkError error_type, const QString& error_str);
        void rolesImportFinished(const OpenAPI::OAIImportRoles_200_response& summary,
                                 QNetworkReply::NetworkError error_type, const QString& error_str);
        void playersListFinished(const OpenAPI::OAIListPlayers_200_response& summary,
                                 QNetworkReply::NetworkError error_type, const QString& error_str);

    private:
        void initSystemAPI();
        void initGameAPI();
        void initRolesAPI();
        void initPlayersAPI();

        void handleHealthCheckReply(const OpenAPI::OAIHealth_200_response& summary,
                                    QNetworkReply::NetworkError error_type, const QString& error_str);
        void handleGameImportReply(const OpenAPI::OAIImportGame_200_response& summary,
                                   QNetworkReply::NetworkError error_type, const QString& error_str);
        void handleRolesListReply(const OpenAPI::OAIListRoles_200_response& summary,
                                  QNetworkReply::NetworkError error_type, const QString& error_str);
        void handleRolesImportReply(const OpenAPI::OAIImportRoles_200_response& summary,
                                    QNetworkReply::NetworkError error_type, const QString& error_str);

        void handlePlayersListReply(const OpenAPI::OAIListPlayers_200_response& summary,
                                    QNetworkReply::NetworkError error_type, const QString& error_str);

    private:
        BotCApiClient() = default;

    private:
        QString m_baseUrl{};
        QString m_apiKey{};
        bool m_bSslVerify = false;

        OpenAPI::OAIGamesApi* m_gameAPI      = nullptr;
        OpenAPI::OAIRolesApi* m_rolesAPI     = nullptr;
        OpenAPI::OAISystemApi* m_systemAPI   = nullptr;
        OpenAPI::OAIPlayersApi* m_playersAPI = nullptr;
    };
}
