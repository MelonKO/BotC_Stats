#include "BotCApiClient.h"

#include "OAIGamesApi.h"
#include "OAIGamesImportStatusResponse.h"
#include "OAIPlayersApi.h"
#include "OAIRolesApi.h"
#include "OAISystemApi.h"
#include "QRegularExpression"

namespace
{
    const QRegularExpression ALIGNMENT_WIN_PATTERN("^(добро|зло|ничья)$");
    const QRegularExpression ALIGNMENT_END_PATTERN("^(добро|зло|нейтральный)$");
    const QRegularExpression VALID_ROLE_ALIGNMENT("^(good|evil|neutral)$");
    const QRegularExpression VALID_ROLE_TYPE("^(Townsfolk|Outsider|Minion|Demon|Traveller)$");
}

namespace botc::api
{
    void BotCApiClient::init(const QString& baseUrl, const QString& apiKey, const bool bSslVerify)
    {
        m_baseUrl    = baseUrl;
        m_apiKey     = apiKey;
        m_bSslVerify = bSslVerify;

        if (!m_bSslVerify)
        {
            QSslConfiguration config = QSslConfiguration::defaultConfiguration();
            config.setPeerVerifyMode(QSslSocket::VerifyNone);
            QSslConfiguration::setDefaultConfiguration(config);
        }

        initSystemAPI();
        initRolesAPI();
        initGameAPI();
        initPlayersAPI();
    }

    void BotCApiClient::setBaseUrl(const QString& baseUrl)
    {
        m_baseUrl = baseUrl;
        const QUrl newUrl(m_baseUrl);
        m_systemAPI->setNewServerForAllOperations(newUrl);
        m_rolesAPI->setNewServerForAllOperations(newUrl);
        m_gameAPI->setNewServerForAllOperations(newUrl);
        m_playersAPI->setNewServerForAllOperations(newUrl);
    }

    void BotCApiClient::setApiKey(const QString& apiKey)
    {
        m_apiKey = apiKey;
        m_systemAPI->addHeaders("X-API-Key", m_apiKey);
        m_rolesAPI->addHeaders("X-API-Key", m_apiKey);
        m_gameAPI->addHeaders("X-API-Key", m_apiKey);
        m_playersAPI->addHeaders("X-API-Key", m_apiKey);
    }

    void BotCApiClient::setSslVerify(const bool bSslVerify)
    {
        if (m_bSslVerify != bSslVerify)
        {
            m_bSslVerify = bSslVerify;
            // TODO:: switch SSL verify
            if (!m_bSslVerify)
            {
                QSslConfiguration config = QSslConfiguration::defaultConfiguration();
                config.setPeerVerifyMode(QSslSocket::VerifyNone);
                QSslConfiguration::setDefaultConfiguration(config);
            }
        }
    }

    void BotCApiClient::healthCheck() const
    {
        m_systemAPI->health();
    }

    void BotCApiClient::importGame(const OpenAPI::OAIGameImportRequest& importGameRequest) const
    {
        m_gameAPI->importGame(importGameRequest);
    }

    void BotCApiClient::importGamesBatch(const OpenAPI::OAIGamesImportRequest& request) const
    {
        m_gameAPI->importGames(request);
    }

    void BotCApiClient::listRoles(const std::optional<QString>& lang) const
    {
        m_rolesAPI->listRoles(lang.value_or(QStringLiteral("")));
    }

    void BotCApiClient::importRoles(const OpenAPI::OAIRolesImportRequest& request) const
    {
        m_rolesAPI->importRoles(request);
    }

    void BotCApiClient::listPlayers() const
    {
        m_playersAPI->listPlayers();
    }

    void BotCApiClient::initSystemAPI()
    {
        m_systemAPI = new OpenAPI::OAISystemApi{};
        m_systemAPI->addHeaders("X-API-Key", m_apiKey);
        m_systemAPI->setNewServerForAllOperations(QUrl(m_baseUrl));
        connect(m_systemAPI, &OpenAPI::OAISystemApi::healthSignal,
                this, [this](const OpenAPI::OAIHealthResponse& summary)
                {
                    handleHealthCheckReply(summary, QNetworkReply::NetworkError::NoError, "");
                });
        connect(m_systemAPI, &OpenAPI::OAISystemApi::healthSignalError,
                this, &BotCApiClient::handleHealthCheckReply);
    }

    void BotCApiClient::initGameAPI()
    {
        m_gameAPI = new OpenAPI::OAIGamesApi{};
        m_gameAPI->addHeaders("X-API-Key", m_apiKey);
        m_gameAPI->setNewServerForAllOperations(QUrl(m_baseUrl));
        connect(m_gameAPI, &OpenAPI::OAIGamesApi::importGameSignalError,
                this, &BotCApiClient::handleGameImportReply);
        connect(m_gameAPI, &OpenAPI::OAIGamesApi::importGameSignal,
                this, [this](const OpenAPI::OAIGameImportStatusResponse& _t1)
                {
                    handleGameImportReply(_t1, QNetworkReply::NetworkError::NoError, "");
                });
        connect(m_gameAPI, &OpenAPI::OAIGamesApi::importGamesSignalError,
                this, &BotCApiClient::handleGamesBatchImportReply);
        connect(m_gameAPI, &OpenAPI::OAIGamesApi::importGamesSignal,
                this, [this](const OpenAPI::OAIGamesImportStatusResponse& summary)
                {
                    handleGamesBatchImportReply(summary, QNetworkReply::NetworkError::NoError, "");
                });
    }

    void BotCApiClient::initRolesAPI()
    {
        m_rolesAPI = new OpenAPI::OAIRolesApi{};
        m_rolesAPI->addHeaders("X-API-Key", m_apiKey);
        m_rolesAPI->setNewServerForAllOperations(QUrl(m_baseUrl));

        //listRoles
        connect(m_rolesAPI, &OpenAPI::OAIRolesApi::listRolesSignalError,
                this, &BotCApiClient::handleRolesListReply);
        connect(m_rolesAPI, &OpenAPI::OAIRolesApi::listRolesSignal,
                this, [this](const OpenAPI::OAIRolesResponse& summary)
                {
                    handleRolesListReply(summary, QNetworkReply::NetworkError::NoError, "");
                });
        //importRoles
        connect(m_rolesAPI, &OpenAPI::OAIRolesApi::importRolesSignalError,
                this, &BotCApiClient::handleRolesImportReply);
        connect(m_rolesAPI, &OpenAPI::OAIRolesApi::importRolesSignal,
                this, [this](const OpenAPI::OAIRolesImportResponse& summary)
                {
                    handleRolesImportReply(summary, QNetworkReply::NetworkError::NoError, "");
                });
    }

    void BotCApiClient::initPlayersAPI()
    {
        m_playersAPI = new OpenAPI::OAIPlayersApi{};
        m_playersAPI->addHeaders("X-API-Key", m_apiKey);
        m_playersAPI->setNewServerForAllOperations(QUrl(m_baseUrl));

        connect(m_playersAPI, &OpenAPI::OAIPlayersApi::listPlayersSignalError,
                this, &BotCApiClient::handlePlayersListReply);
        connect(m_playersAPI, &OpenAPI::OAIPlayersApi::listPlayersSignal,
                this, [this](const OpenAPI::OAIPlayersResponse& summary)
                {
                    handlePlayersListReply(summary, QNetworkReply::NetworkError::NoError, "");
                });
    }

    void BotCApiClient::handleHealthCheckReply(const OpenAPI::OAIHealthResponse& summary,
                                               const QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        emit healthCheckFinished(summary, error_type, error_str);
    }

    void BotCApiClient::handleGameImportReply(const OpenAPI::OAIGameImportStatusResponse& summary,
                                              const QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        emit gameImportFinished(summary, error_type, error_str);
    }

    void BotCApiClient::handleGamesBatchImportReply(const OpenAPI::OAIGamesImportStatusResponse& summary,
                                                    const QNetworkReply::NetworkError error_type,
                                                    const QString& error_str)
    {
        emit gamesImportBatchFinished(summary, error_type, error_str);
    }

    void BotCApiClient::handleRolesListReply(const OpenAPI::OAIRolesResponse& summary,
                                             const QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        emit rolesListFinished(summary, error_type, error_str);
    }

    void BotCApiClient::handleRolesImportReply(const OpenAPI::OAIRolesImportResponse& summary,
                                               const QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        emit rolesImportFinished(summary, error_type, error_str);
    }

    void BotCApiClient::handlePlayersListReply(const OpenAPI::OAIPlayersResponse& summary,
                                               const QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        emit playersListFinished(summary, error_type, error_str);
    }
}
