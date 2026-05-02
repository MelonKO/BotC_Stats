#pragma once

#include <QWidget>

#include "../utils/roles/parser/RolesCsvParser.h"

namespace botc::utils
{
    struct ParseResult;
}

namespace botc::ui
{
    QT_BEGIN_NAMESPACE

    namespace Ui
    {
        class ImportRolesWidget;
    }

    QT_END_NAMESPACE

    class ImportRolesWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit ImportRolesWidget(QWidget* parent = nullptr);
        ~ImportRolesWidget() override;

    signals:
        void rolesImported(const QList<utils::RoleRecord>& records);

    private slots:
        void onBrowseClicked();
        void onImportClicked();
        void onLanguageChanged(const QString& lang);
        void onClearClicked();

        void showPreview(const utils::ParseResult& result);
        void populateTable(const QString& language);
        void showErrors(const QStringList& errors);
        void clearAll();
        void setImportEnabled(bool enabled);
        QString statusStyle(bool ok);

    private:
        Ui::ImportRolesWidget* ui;

        utils::ParseResult m_lastResult;
        QString m_currentLanguage;
    };
} // botc::ui
