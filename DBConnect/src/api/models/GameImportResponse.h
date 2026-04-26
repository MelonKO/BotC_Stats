#pragma once
#include <qlist.h>
#include <qstring.h>

namespace botc::api::models::games
{
    struct GameImportResponse
    {
        QString status;
        QString gameId;
        int playersCreated;
        QVector<QString> errors;

        [[nodiscard]] bool isSuccess() const;
        [[nodiscard]] bool hasErrors() const;
    };

    inline bool GameImportResponse::isSuccess() const
    {
        return status == "ok";
    }

    inline bool GameImportResponse::hasErrors() const
    {
        return !errors.isEmpty() && !isSuccess();
    }
}
