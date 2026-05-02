#include "MainWindow.h"

#include "SettingsWidget.h"
#include "ui_MainWindow.h"

namespace botc::ui
{
    MainWindow::MainWindow(QWidget* parent) :
        QMainWindow(parent), ui(new Ui::MainWindow)
    {
        ui->setupUi(this);

        QTabWidget* tabs = new QTabWidget(this);
        /*tabs->addTab(new ImportGamesWidget(this), tr("Games import"));
        tabs->addTab(new ImportRolesWidget(), tr("Roles import"));*/
        tabs->addTab(new SettingsWidget(this), tr("Settings"));

        setCentralWidget(tabs);
        setWindowTitle("BotC DBConnect");
        resize(800, 600);
    }

    MainWindow::~MainWindow()
    {
        delete ui;
    }
} // botc::ui
