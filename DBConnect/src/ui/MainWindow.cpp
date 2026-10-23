#include "MainWindow.h"

#include "ImportGamesWidget.h"
#include "ImportRolesWidget.h"
#include "SettingsWidget.h"
#include "ui_MainWindow.h"
#include "db_cache/DBCache.h"
#include "../config/ConfigManager.h"

namespace botc::ui
{
    MainWindow::MainWindow(QWidget* parent) :
        QMainWindow(parent), ui(new Ui::MainWindow)
    {
        ui->setupUi(this);

        // One saved settings tab is one change: the API client picks up the new
        // URL, key and TLS policy first (see main()), then the caches reload.
        connect(config::ConfigManager::instance(), &config::ConfigManager::serverSettingsChanged,
                DBCache::instance(), &DBCache::refreshAll);

        auto* tabs = new QTabWidget(this);

        auto importRolesWidget = new ImportRolesWidget(this);
        tabs->addTab(importRolesWidget, tr("Roles import"));

        auto* importGamesWidget = new ImportGamesWidget(this);
        tabs->addTab(importGamesWidget, tr("Games import"));
        tabs->addTab(new SettingsWidget(this), tr("Settings"));

        connect(importRolesWidget, &ImportRolesWidget::onRolesImported,
                DBCache::instance(), []
                {
                    DBCache::instance()->updateRoles("ru");
                });

        connect(importGamesWidget, &ImportGamesWidget::onGamesImported,
                DBCache::instance(), []()
                {
                    // Games imports create players, not roles, so only refresh the player cache.
                    DBCache::instance()->updatePlayers();
                });

        setCentralWidget(tabs);
        setWindowTitle("BotC DBConnect");
        resize(800, 600);
    }

    MainWindow::~MainWindow()
    {
        delete ui;
    }
} // botc::ui
