#include "BotCNetworkAccessManager.h"

#include <QSslConfiguration>
#include <QSslSocket>

namespace botc::api
{
    BotCNetworkAccessManager::BotCNetworkAccessManager(QObject* parent) :
        QNetworkAccessManager(parent)
    {
    }

    void BotCNetworkAccessManager::setPeerVerificationEnabled(const bool bEnabled)
    {
        if (m_bPeerVerificationEnabled == bEnabled)
        {
            return;
        }

        m_bPeerVerificationEnabled = bEnabled;

        // A keep-alive connection opened under the previous policy must not be
        // reused: a socket established with VerifyNone would silently bypass the
        // restored verification, so the next request has to handshake again.
        clearConnectionCache();
    }

    bool BotCNetworkAccessManager::isPeerVerificationEnabled() const noexcept
    {
        return m_bPeerVerificationEnabled;
    }

    QSslConfiguration BotCNetworkAccessManager::applyPolicy(const QSslConfiguration& sslConfiguration,
                                                           const bool bVerifyPeer)
    {
        QSslConfiguration config = sslConfiguration;
        if (config.isNull())
        {
            config = QSslConfiguration::defaultConfiguration();
        }

        config.setPeerVerifyMode(bVerifyPeer ? QSslSocket::AutoVerifyPeer : QSslSocket::VerifyNone);
        return config;
    }

    QNetworkReply* BotCNetworkAccessManager::createRequest(const Operation op,
                                                           const QNetworkRequest& request,
                                                           QIODevice* const outgoingData)
    {
        QNetworkRequest effectiveRequest{request};
        effectiveRequest.setSslConfiguration(
            applyPolicy(request.sslConfiguration(), m_bPeerVerificationEnabled)
        );

        return QNetworkAccessManager::createRequest(op, effectiveRequest, outgoingData);
    }
}
