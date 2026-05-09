#pragma once

#include <qdatetime.h>

#include "PlayerImportRequest.h"

namespace botc::api::models::games
{
    struct GameImportRequest
    {
        QDate gameDate;
        QString scenarioName;
        QString storytellerName;
        QString alignmentWin;
        QString location;
        uint8_t gameNumber;
        std::optional<QString> duration;
        std::optional<QString> notes;
        QVector<PlayerImportRequest> players;
    };
}
