#include "MainWindow.h"

#include "ImportGamesWidget.h"
#include "ImportRolesWidget.h"
#include "SettingsWidget.h"
#include "ui_MainWindow.h"
#include "db_cache/DBCache.h"
#include "../config/ConfigManager.h"

#include <QTimer>

namespace botc::ui
{
    MainWindow::MainWindow(QWidget* parent) :
        QMainWindow(parent), ui(new Ui::MainWindow), m_cacheRefreshTimer(new QTimer(this))
    {
        ui->setupUi(this);

        m_cacheRefreshTimer->setSingleShot(true);
        m_cacheRefreshTimer->setInterval(200);
        const auto* configManager = config::ConfigManager::instance();
        const auto scheduleCacheRefresh = [this]
        {
            m_cacheRefreshTimer->start();
        };
        connect(configManager, &config::ConfigManager::apiUrlChanged,
                this, scheduleCacheRefresh);
        connect(configManager, &config::ConfigManager::apiKeyChanged,
                this, scheduleCacheRefresh);
        connect(configManager, &config::ConfigManager::sslVerifyChanged,
                this, scheduleCacheRefresh);
        connect(m_cacheRefreshTimer, &QTimer::timeout,
                this, []
                {
                    DBCache::instance()->refreshAll();
                });

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
