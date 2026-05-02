#pragma once

#include <QWidget>

namespace botc::ui
{
    QT_BEGIN_NAMESPACE

    namespace Ui
    {
        class SettingsWidget;
    }

    QT_END_NAMESPACE

    class SettingsWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit SettingsWidget(QWidget* parent = nullptr);
        ~SettingsWidget() override;

    private slots:
        void onSaveClicked();
        void onResetClicked();
        void onToggleApiKeyVisibility(bool checked);

    private:
        void loadSettings();
        void saveSettings();

    private:
        Ui::SettingsWidget* ui;
    };
} // botc::ui
