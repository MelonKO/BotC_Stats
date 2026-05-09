#pragma once

#include <QMainWindow>

#include "../api/BotCApiClient.h"

namespace botc::utils
{
    struct RoleRecord;
}

namespace botc::ui
{
    QT_BEGIN_NAMESPACE

    namespace Ui
    {
        class MainWindow;
    }

    QT_END_NAMESPACE

    class MainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit MainWindow(QWidget* parent = nullptr);
        ~MainWindow() override;

    private:
        Ui::MainWindow* ui;
        api::BotCApiClient* m_apiClient = nullptr;
    };
} // botc::ui
