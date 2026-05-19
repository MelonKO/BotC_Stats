#include "MainWindow.h"

#include "ImportGamesWidget.h"
#include "ImportRolesWidget.h"
#include "SettingsWidget.h"
#include "ui_MainWindow.h"
#include "db_cache/DBCache.h"

namespace botc::ui
{
    MainWindow::MainWindow(QWidget* parent) :
        QMainWindow(parent), ui(new Ui::MainWindow)
    {
        ui->setupUi(this);

        auto* tabs             = new QTabWidget(this);
        auto importRolesWidget = new ImportRolesWidget();
        tabs->addTab(importRolesWidget, tr("Roles import"));
        tabs->addTab(new ImportGamesWidget(this), tr("Games import"));
        tabs->addTab(new SettingsWidget(this), tr("Settings"));

        connect(importRolesWidget, &ImportRolesWidget::onRolesImported,
                DBCache::instance(), []
                {
                    DBCache::instance()->updateRoles("ru");
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
