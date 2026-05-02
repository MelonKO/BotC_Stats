#include "MainWindow.h"

#include <QMessageBox>

#include "ImportRolesWidget.h"
#include "SettingsWidget.h"
#include "ui_MainWindow.h"
#include "../config/ConfigManager.h"
#include "../api/models/RolesImportRequest.h"
#include "../api/models/RolesImportResponse.h"

namespace botc::ui
{
    MainWindow::MainWindow(QWidget* parent) :
        QMainWindow(parent), ui(new Ui::MainWindow)
    {
        ui->setupUi(this);

        QTabWidget* tabs       = new QTabWidget(this);
        auto RolesImportWidget = new ImportRolesWidget();
        tabs->addTab(RolesImportWidget, tr("Roles import"));

        connect(RolesImportWidget, &ImportRolesWidget::rolesImported, this, &MainWindow::onRolesImportClicked);
        /*tabs->addTab(new ImportGamesWidget(this), tr("Games import"));
        */
        tabs->addTab(new SettingsWidget(this), tr("Settings"));

        setCentralWidget(tabs);
        setWindowTitle("BotC DBConnect");
        resize(800, 600);

        initApiClient();
    }

    MainWindow::~MainWindow()
    {
        delete ui;
    }

    void MainWindow::initApiClient()
    {
        const auto ConfigManager = config::ConfigManager::instance();
        m_apiClient              = new api::BotCApiClient(ConfigManager->getApiUrl(), ConfigManager->getApiKey(),
                                             ConfigManager->getSslVerify(), this);
    }

    void MainWindow::onRolesImportClicked(const QList<utils::RoleRecord>& in_records)
    {
        if (!m_apiClient)
        {
            //TODO:: add error
            return;
        }

        QVector<api::models::roles::RoleImportItem> roles;

        for (const auto& record : in_records)
        {
            QMap<QString, api::models::roles::RoleTranslation> translations;

            for (auto it = record.translations.constBegin(); it != record.translations.constEnd(); ++it)
            {
                translations.insert(it.key(),
                                    api::models::roles::RoleTranslation{
                                        .name        = it.value().first,
                                        .description = it.value().second
                                    });
            }

            roles.push_back(api::models::roles::RoleImportItem{
                .name         = record.name,
                .alignment    = record.alignment,
                .roleType     = record.roleType,
                .description  = record.description,
                .translations = translations
            });
        }

        connect(m_apiClient, &api::BotCApiClient::rolesImportFinished, this, &MainWindow::onRolesImportFinished);
        m_apiClient->importRoles(api::models::roles::RolesImportRequest{std::move(roles)});
    }

    void MainWindow::onRolesImportFinished(bool in_bSuccess, const api::models::roles::RolesImportResponse& in_response)
    {
        auto btn = QMessageBox::information(
            this, "Импорт ролей",
            in_bSuccess ? "Импорт ролей прошёл успешно" : "Импорт ролей произошёл с ошибкой"
        );
    }
} // botc::ui
