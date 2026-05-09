#pragma once
#include <optional>
#include <qlist.h>
#include <qstring.h>

namespace botc::api::models::roles
{
    struct RoleInfo
    {
        QString name;
        QString alignment;
        QString roleType;
        std::optional<QString> description;
    };

    struct RolesResponse
    {
        QVector<RoleInfo> roles;
    };
}
