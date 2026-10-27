#include "MainWindow.h"

#include <QEvent>
#include <QTabWidget>

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

        auto* importRolesWidget = new ImportRolesWidget(this);
        m_importRolesWidget = importRolesWidget;
        m_tabs->addTab(importRolesWidget, tr("Roles import"));

        auto* importGamesWidget = new ImportGamesWidget(this);
        m_importGamesWidget = importGamesWidget;
        m_tabs->addTab(importGamesWidget, tr("Games import"));

        auto* playersWidget = new PlayersWidget(this);
        m_playersWidget = playersWidget;
        m_tabs->addTab(playersWidget, tr("Players"));

        auto* rolesWidget = new RolesWidget(this);
        m_rolesWidget = rolesWidget;
        m_tabs->addTab(rolesWidget, tr("Roles"));

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

        m_tabs->setTabText(m_tabs->indexOf(m_importRolesWidget), tr("Roles import"));
        m_tabs->setTabText(m_tabs->indexOf(m_importGamesWidget), tr("Games import"));
        m_tabs->setTabText(m_tabs->indexOf(m_playersWidget), tr("Players"));
        m_tabs->setTabText(m_tabs->indexOf(m_rolesWidget), tr("Roles"));
        m_tabs->setTabText(m_tabs->indexOf(m_settingsWidget), tr("Settings"));
    }
} // namespace botc::ui
