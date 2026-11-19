#include "MainWindow.h"

#include <QEvent>
#include <QTabWidget>

#include "GamesWidget.h"
#include "ImportGamesWidget.h"
#include "ImportRolesWidget.h"
#include "PlayersWidget.h"
#include "RolesWidget.h"
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

        m_tabs = new QTabWidget(this);

        m_rolesPages = new QTabWidget(m_tabs);
        auto* rolesWidget = new RolesWidget(m_rolesPages);
        m_rolesCatalogWidget = rolesWidget;
        m_rolesPages->addTab(rolesWidget, tr("Catalog"));
        auto* importRolesWidget = new ImportRolesWidget(m_rolesPages);
        m_importRolesWidget = importRolesWidget;
        m_rolesPages->addTab(importRolesWidget, tr("Import CSV"));
        m_tabs->addTab(m_rolesPages, tr("Roles"));

        m_gamesPages = new QTabWidget(m_tabs);
        auto* gamesWidget = new GamesWidget(m_gamesPages);
        m_gamesWidget = gamesWidget;
        m_gamesPages->addTab(gamesWidget, tr("Game list"));
        auto* importGamesWidget = new ImportGamesWidget(m_gamesPages);
        m_importGamesWidget = importGamesWidget;
        m_gamesPages->addTab(importGamesWidget, tr("Import CSV"));
        m_tabs->addTab(m_gamesPages, tr("Games"));

        auto* playersWidget = new PlayersWidget(this);
        m_playersWidget = playersWidget;
        m_tabs->addTab(playersWidget, tr("Players"));

        auto* settingsWidget = new SettingsWidget(this);
        m_settingsWidget = settingsWidget;
        m_tabs->addTab(settingsWidget, tr("Settings"));

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
        connect(importGamesWidget, &ImportGamesWidget::onGamesImported,
                gamesWidget, &GamesWidget::refreshGames);
        connect(config::ConfigManager::instance(), &config::ConfigManager::serverSettingsChanged,
                gamesWidget, &GamesWidget::refreshGames);

        setCentralWidget(m_tabs);
        setWindowTitle("BotC DBConnect");
        resize(800, 600);
    }

    MainWindow::~MainWindow()
    {
        delete ui;
    }

    void MainWindow::changeEvent(QEvent* event)
    {
        if (event->type() == QEvent::LanguageChange)
            retranslate();

        QMainWindow::changeEvent(event);
    }

    void MainWindow::retranslate()
    {
        ui->retranslateUi(this);

        m_tabs->setTabText(m_tabs->indexOf(m_rolesPages), tr("Roles"));
        m_tabs->setTabText(m_tabs->indexOf(m_gamesPages), tr("Games"));
        m_tabs->setTabText(m_tabs->indexOf(m_playersWidget), tr("Players"));
        m_tabs->setTabText(m_tabs->indexOf(m_settingsWidget), tr("Settings"));
        m_rolesPages->setTabText(m_rolesPages->indexOf(m_rolesCatalogWidget), tr("Catalog"));
        m_rolesPages->setTabText(m_rolesPages->indexOf(m_importRolesWidget), tr("Import CSV"));
        m_gamesPages->setTabText(m_gamesPages->indexOf(m_gamesWidget), tr("Game list"));
        m_gamesPages->setTabText(m_gamesPages->indexOf(m_importGamesWidget), tr("Import CSV"));
    }
} // namespace botc::ui
