#pragma once
#include <optional>
#include <qstring.h>

namespace botc::api::models::games
{
    struct PlayerImportRequest
    {
        QString name;
        std::optional<uint8_t> seatNumber;
        QString roleStart;
        QString roleEnd;
        QString alignmentEnd;
        bool bIsAlive;
    };
}
