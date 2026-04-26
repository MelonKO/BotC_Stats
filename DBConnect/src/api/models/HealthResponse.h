#pragma once
#include <qstring.h>

namespace botc::api::models
{
    struct HealthResponse
    {
        QString status;
        bool dbConnected;
    };
}
