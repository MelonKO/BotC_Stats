#include "DBCache.h"

#include "OAIListRoles_200_response.h"
#include "OAIListRoles_200_response_roles_inner.h"
#include "qeventloop.h"
#include "api/BotCApiClient.h"

namespace botc
{
    void DBCache::updateRoles(const std::optional<QString>& lang) const
    {
        const auto* apiClient = api::BotCApiClient::instance();
        connect(apiClient, &api::BotCApiClient::rolesListFinished,
                this, &DBCache::onRoleListFinished, Qt::SingleShotConnection);
        apiClient->listRoles(lang);
    }

    void DBCache::onRoleListFinished(const OpenAPI::OAIListRoles_200_response& summary,
                                     QNetworkReply::NetworkError error_type, const QString& error_str)
    {
        std::ranges::transform(
            summary.getRoles(),
            std::inserter(m_cachedRoles, m_cachedRoles.end()),
            [](const OpenAPI::OAIListRoles_200_response_roles_inner& role) -> QString
            {
                QString transName = role.getTranslation().getName();
                return transName.isEmpty() ? role.getName() : transName;
            }
        );
    }
}
