#pragma once

#include <QNetworkReply>
#include <QString>

namespace botc::ui
{
    /**
     * User-facing reason for a failed request.
     *
     * The raw error string from the network client combines a technical message
     * with the response body, so it is shown as a tooltip while the status line
     * uses this translated text instead.
     */
    [[nodiscard]] QString describeNetworkError(QNetworkReply::NetworkError errorType);
} // namespace botc::ui
