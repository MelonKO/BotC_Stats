#pragma once
#include <qmap.h>
#include <qstring.h>

namespace botc::api::models::roles
{
    struct RoleTranslation
    {
        QString name;
        std::optional<QString> description;
    };

    struct RoleImportItem
    {
        QString name;
        QString alignment;
        QString roleType;
        std::optional<QString> description;
        QMap<QString, RoleTranslation> translations;
    };

    struct RolesImportRequest
    {
        QVector<RoleImportItem> roles;
    };
}
