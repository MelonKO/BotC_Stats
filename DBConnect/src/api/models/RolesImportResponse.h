#pragma once
#include <qlist.h>
#include <qstring.h>

namespace botc::api::models::roles
{
    struct RolesImportResponse
    {
        QString status;
        int rolesCreated;
        int rolesUpdated;
        QVector<QString> errors;
    };
}
