#pragma once
#include <qobject.h>
#include <QNetworkAccessManager>

#include "models/RolesResponse.h"

namespace botc::api
{
    namespace models
    {
        struct HealthResponse;
    }

    namespace models::roles
    {
        struct RolesImportResponse;
        struct RolesImportRequest;
    }

    namespace models::games
    {
        struct GameImportResponse;
        struct GameImportRequest;
    }

    class BotCApiClient final : public QObject
    {
        Q_OBJECT

    public:
        explicit BotCApiClient(const QString& baseUrl, const QString& apiKey, bool bSslVerify = false,
                               QObject* parent                                                = nullptr);

        // config

        void setBaseUrl(const QString& baseUrl);
        void setApiKey(const QString& apiKey);
        void setSslVerify(bool bSslVerify);

        // API

        void healthCheck();
        void importGame(const models::games::GameImportRequest& request);
        void getRoles();
        void importRoles(const models::roles::RolesImportRequest& request);

    signals:
        // callbacks

        void healthCheckFinished(bool bSuccess, const models::HealthResponse& response);
        void gameImportFinished(bool bSuccess, const models::games::GameImportResponse& response);
        void rolesListFinished(bool bSuccess, const models::roles::RolesResponse& response);
        void rolesImportFinished(bool bSuccess, const models::roles::RolesImportResponse& response);

    private:
        QNetworkRequest initRequest(const QString& endpoint) const;
        static QJsonDocument serializeGameRequest(const models::games::GameImportRequest& gameImportRequest);
        static QJsonDocument serializeRolesRequest(const models::roles::RolesImportRequest& rolesImportRequest);

        void handleHealthCheckReply(QNetworkReply* reply);
        void handleGameImportReply(QNetworkReply* reply);
        void handleRolesListReply(QNetworkReply* reply);
        void handleRolesImportReply(QNetworkReply* reply);

    private:
        QString m_baseUrl;
        QString m_apiKey;
        bool m_bSslVerify;
        QNetworkAccessManager* m_networkManager;
    };
}
