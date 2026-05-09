#include <QApplication>
#include "api/BotCApiClient.h"
#include "config/ConfigManager.h"
#include "ui/MainWindow.h"

int main(int argc, char* argv[])
{
    // Initialize Qt application and parse command-line arguments
    const QApplication app(argc, argv);

    auto ConfigManager = botc::config::ConfigManager::instance();
    auto api_client    = new botc::api::BotCApiClient(
        ConfigManager->getApiUrl(), ConfigManager->getApiKey(), ConfigManager->getSslVerify());

    QObject::connect(api_client, &botc::api::BotCApiClient::rolesListFinished,
                     [](const bool bSuccess, const botc::api::models::roles::RolesResponse& in_response)
                     {
                         qDebug() << bSuccess;
                         if (bSuccess)
                         {
                             for (const auto& role : in_response.roles)
                             {
                                 qDebug() << role.name << ":" << role.roleType << ":" << role.alignment << ":" <<
                                     role.description;
                             }
                         }
                     });
    api_client->getRoles();

    // Create main window widget
    botc::ui::MainWindow window;
    // window.setWindowTitle("Qt vcpkg CMake Demo");

    // Show the window and start the Qt event loop
    window.show();
    return QApplication::exec();
}
