#include <QApplication>
#include <QPushButton>
#include <QVBoxLayout>

#include "api/BotCApiClient.h"
#include "config/ConfigManager.h"

int main(int argc, char* argv[])
{
    // Initialize Qt application and parse command-line arguments
    const QApplication app(argc, argv);

    auto ConfigManager = botc::config::ConfigManager::instance();
    if (const bool bConfigLoaded = ConfigManager->loadConfig())
    {
        auto api_client = new botc::api::BotCApiClient(
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
    }

    // Create main window widget
    QWidget window;
    window.setWindowTitle("Qt vcpkg CMake Demo");
    window.resize(400, 300);

    // Create vertical layout and a push button
    QVBoxLayout* layout = new QVBoxLayout(&window);
    QPushButton* button = new QPushButton("Hello from Qt!", &window);
    layout->addWidget(button);

    // Connect button click signal to application quit slot
    QObject::connect(button, &QPushButton::clicked, &app, &QApplication::quit);

    // Show the window and start the Qt event loop
    window.show();
    return QApplication::exec();
}
