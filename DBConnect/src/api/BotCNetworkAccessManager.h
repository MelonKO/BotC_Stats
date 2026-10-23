#pragma once

#include <QNetworkAccessManager>
#include <QSslConfiguration>

namespace botc::api
{
    // Applies the certificate verification policy to every request it issues.
    //
    // Changing QSslConfiguration::setDefaultConfiguration() would affect every
    // TLS connection of the process and cannot be reverted reliably, so the
    // policy lives on this manager: existing API clients use one shared instance
    // and a one-off connection test gets its own.
    class BotCNetworkAccessManager final : public QNetworkAccessManager
    {
        Q_OBJECT

    public:
        explicit BotCNetworkAccessManager(QObject* parent = nullptr);

        // false accepts self-signed and otherwise untrusted certificates
        // (local or test servers); true verifies the peer chain and host name.
        void setPeerVerificationEnabled(bool bEnabled);

        [[nodiscard]] bool isPeerVerificationEnabled() const noexcept;

    protected:
        QNetworkReply* createRequest(Operation op,
                                     const QNetworkRequest& request,
                                     QIODevice* outgoingData = nullptr) override;

    private:
        static QSslConfiguration applyPolicy(const QSslConfiguration& sslConfiguration, bool bVerifyPeer);

        bool m_bPeerVerificationEnabled = true;
    };
}
