#pragma once

#include <QMainWindow>

#include "../api/BotCApiClient.h"

class QTabWidget;

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

    protected:
        void changeEvent(QEvent* event) override;

    private:
        void retranslate();

    private:
        Ui::MainWindow* ui;
        QTabWidget* m_tabs = nullptr;
        QWidget* m_importRolesWidget = nullptr;
        QWidget* m_importGamesWidget = nullptr;
        QWidget* m_playersWidget = nullptr;
        QWidget* m_rolesWidget = nullptr;
        QWidget* m_settingsWidget = nullptr;
    };
} // namespace botc::ui
