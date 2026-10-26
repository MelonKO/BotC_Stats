#include "NetworkErrorMessages.h"

#include <QCoreApplication>

namespace botc::ui
{
    QString describeNetworkError(const QNetworkReply::NetworkError errorType)
    {
        switch (errorType)
        {
        case QNetworkReply::ServiceUnavailableError:
            return QCoreApplication::translate("NetworkErrorMessages",
                                               "The server rejected the request (503). Try again later.");
        case QNetworkReply::TimeoutError:
            return QCoreApplication::translate("NetworkErrorMessages",
                                               "The server response timed out.");
        case QNetworkReply::ConnectionRefusedError:
        case QNetworkReply::HostNotFoundError:
            return QCoreApplication::translate("NetworkErrorMessages",
                                               "No connection to the server. Check the settings.");
        case QNetworkReply::AuthenticationRequiredError:
        case QNetworkReply::ContentAccessDenied:
            return QCoreApplication::translate("NetworkErrorMessages",
                                               "The server rejected the API key.");
        default:
            return QCoreApplication::translate("NetworkErrorMessages",
                                               "Network error (code %1).")
                .arg(static_cast<int>(errorType));
        }
    }
} // namespace botc::ui
