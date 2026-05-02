#include "BotCApiClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>

#include "models/GameImportRequest.h"
#include "models/GameImportResponse.h"
#include "models/HealthResponse.h"
#include "models/RolesImportRequest.h"
#include "models/RolesImportResponse.h"

namespace
{
    const QString ENDPOINT_HEALTH       = "/health";
    const QString ENDPOINT_GAME_IMPORT  = "/api/games/import";
    const QString ENDPOINT_ROLES_LIST   = "/api/roles";
    const QString ENDPOINT_ROLES_IMPORT = "/api/roles/import";
    const QRegularExpression ALIGNMENT_WIN_PATTERN("^(добро|зло)$");
    const QRegularExpression ALIGNMENT_END_PATTERN("^(добро|зло|нейтральный)$");
    const QRegularExpression VALID_ROLE_ALIGNMENT("^(good|evil|neutral)$");
    const QRegularExpression VALID_ROLE_TYPE("^(Townsfolk|Outsider|Minion|Demon|Traveller)$");
}

namespace botc::api
{
    BotCApiClient::BotCApiClient(const QString& baseUrl, const QString& apiKey, bool bSslVerify, QObject* parent)
        : QObject(parent),
          m_baseUrl(baseUrl),
          m_apiKey(apiKey),
          m_bSslVerify(bSslVerify),
          m_networkManager(new QNetworkAccessManager(this))
    {
        if (!m_bSslVerify)
        {
            QSslConfiguration config = QSslConfiguration::defaultConfiguration();
            config.setPeerVerifyMode(QSslSocket::VerifyNone);
            QSslConfiguration::setDefaultConfiguration(config);
        }
    }

    void BotCApiClient::setBaseUrl(const QString& baseUrl)
    {
        m_baseUrl = baseUrl;
    }

    void BotCApiClient::setApiKey(const QString& apiKey)
    {
        m_apiKey = apiKey;
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

    void BotCApiClient::healthCheck()
    {
        const QNetworkRequest request = initRequest(ENDPOINT_HEALTH);
        QNetworkReply* reply          = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]()
        {
            handleHealthCheckReply(reply);
        });
    }

    void BotCApiClient::importGame(const models::games::GameImportRequest& request)
    {
        const QNetworkRequest netRequest = initRequest(ENDPOINT_GAME_IMPORT);
        const QJsonDocument jsonDoc      = serializeGameRequest(request);
        QNetworkReply* reply             = m_networkManager->post(netRequest, jsonDoc.toJson());
        connect(reply, &QNetworkReply::finished, this, [this, reply]()
        {
            handleGameImportReply(reply);
        });
    }

    void BotCApiClient::getRoles()
    {
        const QNetworkRequest request = initRequest(ENDPOINT_ROLES_LIST);
        QNetworkReply* reply          = m_networkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]()
        {
            handleRolesListReply(reply);
        });
    }

    void BotCApiClient::importRoles(const models::roles::RolesImportRequest& request)
    {
        QNetworkRequest netRequest  = initRequest(ENDPOINT_ROLES_IMPORT);
        const QJsonDocument jsonDoc = serializeRolesRequest(request);
        QNetworkReply* reply        = m_networkManager->post(netRequest, jsonDoc.toJson());
        connect(reply, &QNetworkReply::finished, this, [this, reply]()
        {
            handleRolesImportReply(reply);
        });
    }

    QNetworkRequest BotCApiClient::initRequest(const QString& endpoint) const
    {
        QUrl url(m_baseUrl);
        url.setPath(endpoint);
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        request.setRawHeader("X-API-Key", m_apiKey.toUtf8());
        if (!m_bSslVerify)
        {
            QSslConfiguration sslConfig = request.sslConfiguration();
            sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
            request.setSslConfiguration(sslConfig);
        }
        return request;
    }

    QJsonDocument BotCApiClient::serializeGameRequest(const models::games::GameImportRequest& gameImportRequest)
    {
        QJsonObject gameJson;
        gameJson["game_date"]        = gameImportRequest.gameDate.toString("yyyy-MM-dd");
        gameJson["scenario_name"]    = gameImportRequest.scenarioName;
        gameJson["storyteller_name"] = gameImportRequest.storytellerName;
        gameJson["alignment_win"]    = gameImportRequest.alignmentWin;
        gameJson["location"]         = gameImportRequest.location;
        gameJson["game_number"]      = gameImportRequest.gameNumber;

        if (gameImportRequest.duration.has_value())
        {
            gameJson["duration"] = gameImportRequest.duration.value();
        }
        if (gameImportRequest.notes.has_value())
        {
            gameJson["notes"] = gameImportRequest.notes.value();
        }

        QJsonArray playersArray;
        // ReSharper disable once CppUseStructuredBinding
        for (const models::games::PlayerImportRequest& player : gameImportRequest.players)
        {
            QJsonObject playerJson;
            playerJson["name"]          = player.name;
            playerJson["role_start"]    = player.roleStart;
            playerJson["role_end"]      = player.roleEnd;
            playerJson["alignment_end"] = player.alignmentEnd;
            playerJson["is_alive"]      = player.bIsAlive;
            if (player.seatNumber.has_value())
            {
                playerJson["seat_number"] = player.seatNumber.value();
            }

            playersArray.push_back(std::move(playerJson));
        }

        gameJson["players"] = std::move(playersArray);
        return QJsonDocument(gameJson);
    }

    QJsonDocument BotCApiClient::serializeRolesRequest(const models::roles::RolesImportRequest& rolesImportRequest)
    {
        QJsonArray rolesArray;
        // ReSharper disable once CppUseStructuredBinding
        for (const models::roles::RoleImportItem& role : rolesImportRequest.roles)
        {
            QJsonObject roleJson;
            roleJson["name"]        = role.name;
            roleJson["alignment"]   = role.alignment;
            roleJson["role_type"]   = role.roleType;
            roleJson["description"] = role.description.has_value() ? role.description.value() : QString();
            QJsonObject roleTranslations;

            for (auto it = role.translations.begin(); it != role.translations.end(); ++it)
            {
                QJsonObject translationJson;
                translationJson["name"]        = it.value().name;
                translationJson["description"] = it.value().description.has_value()
                                                     ? it.value().description.value()
                                                     : QString();
                roleTranslations[it.key()] = translationJson;
            }
            roleJson["translations"] = std::move(roleTranslations);
            rolesArray.push_back(std::move(roleJson));
        }

        QJsonObject importJson;
        importJson["roles"] = std::move(rolesArray);
        return QJsonDocument(importJson);
    }

    void BotCApiClient::handleHealthCheckReply(QNetworkReply* reply)
    {
        bool bSuccess = false;
        models::HealthResponse healthResponse;
        if (reply->error() == QNetworkReply::NoError)
        {
            const QByteArray response        = reply->readAll();
            const QJsonDocument responseJson = QJsonDocument::fromJson(response);
            const QJsonObject jsonObject     = responseJson.object();

            healthResponse.status      = jsonObject["status"].toString();
            healthResponse.dbConnected = jsonObject["connected"].toBool();
            bSuccess                   = true;
        }
        else
        {
            healthResponse.status      = reply->errorString();
            healthResponse.dbConnected = false;
        }
        emit healthCheckFinished(bSuccess, healthResponse);
        reply->deleteLater();
    }

    void BotCApiClient::handleGameImportReply(QNetworkReply* reply)
    {
        bool bSuccess = false;
        models::games::GameImportResponse gameImportResponse;
        if (reply->error() == QNetworkReply::NoError)
        {
            const QByteArray response        = reply->readAll();
            const QJsonDocument responseJson = QJsonDocument::fromJson(response);
            const QJsonObject jsonObject     = responseJson.object();

            gameImportResponse.status         = jsonObject["status"].toString();
            gameImportResponse.gameId         = jsonObject["gameId"].toString();
            gameImportResponse.playersCreated = jsonObject["playersCreated"].toInt();

            QVector<QString> errors;
            for (const auto& error : jsonObject["errors"].toArray())
            {
                errors.push_back(error.toString());
            }
            gameImportResponse.errors = std::move(errors);
            bSuccess                  = true;
        }
        else
        {
            gameImportResponse.status = reply->errorString();
            bSuccess                  = false;
        }
        emit gameImportFinished(bSuccess, gameImportResponse);
        reply->deleteLater();
    }

    void BotCApiClient::handleRolesListReply(QNetworkReply* reply)
    {
        //name, alignment, role_type
        bool bSuccess = false;
        models::roles::RolesResponse rolesListResponse;
        if (reply->error() == QNetworkReply::NoError)
        {
            const QByteArray response        = reply->readAll();
            const QJsonDocument responseJson = QJsonDocument::fromJson(response);
            const QJsonObject jsonObject     = responseJson.object();

            QVector<models::roles::RoleInfo> infos;
            for (const auto& roleJson : jsonObject["roles"].toArray())
            {
                models::roles::RoleInfo info;

                auto roleObj     = roleJson.toObject();
                info.name        = roleObj["name"].toString();
                info.alignment   = roleObj["alignment"].toString();
                info.roleType    = roleObj["role_type"].toString();
                info.description = roleObj["description"].isNull() ? QString() : roleObj["description"].toString();
                infos.emplace_back(std::move(info));
            }

            rolesListResponse.roles = std::move(infos);
            bSuccess                = true;
        }
        emit rolesListFinished(bSuccess, rolesListResponse);
        reply->deleteLater();
    }

    void BotCApiClient::handleRolesImportReply(QNetworkReply* reply)
    {
        bool bSuccess = false;
        models::roles::RolesImportResponse rolesImportResponse;
        if (reply->error() == QNetworkReply::NoError)
        {
            const QByteArray response        = reply->readAll();
            const QJsonDocument responseJson = QJsonDocument::fromJson(response);
            const QJsonObject jsonObject     = responseJson.object();

            rolesImportResponse.status       = jsonObject["status"].toString();
            rolesImportResponse.rolesCreated = jsonObject["roles_created"].toInt();
            rolesImportResponse.rolesUpdated = jsonObject["roles_updated"].toInt();
            QVector<QString> errors;
            for (const auto& error : jsonObject["errors"].toArray())
            {
                errors.push_back(error.toString());
            }
            rolesImportResponse.errors = std::move(errors);
            bSuccess                   = true;
        }
        else
        {
            rolesImportResponse.status = reply->errorString();
        }

        emit rolesImportFinished(bSuccess, rolesImportResponse);
        reply->deleteLater();
    }
}
