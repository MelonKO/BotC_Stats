#include "ImportRolesWidget.h"

#include <QColor>
#include <QEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSet>
#include <QSignalBlocker>

#include "OAIRolesImportResponse.h"
#include "OAIRolesImportRequest.h"
#include "ui_ImportRolesWidget.h"
#include "../api/BotCApiClient.h"
#include "NetworkErrorMessages.h"

namespace
{
    const QString kErrorStyle = "color: red;";
    const QString kErrorBoldStyle = "color: red; font-weight: bold;";
}

namespace botc::ui
{
    ImportRolesWidget::ImportRolesWidget(QWidget* parent) :
        QWidget(parent), ui(new Ui::ImportRolesWidget)
    {
        ui->setupUi(this);

        connect(ui->browseButton, &QPushButton::clicked,
                this, &ImportRolesWidget::onBrowseClicked);
        connect(ui->importButton, &QPushButton::clicked,
                this, &ImportRolesWidget::onImportClicked);
        connect(ui->clearButton, &QPushButton::clicked,
                this, &ImportRolesWidget::onClearClicked);
        connect(ui->languageCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                this, &ImportRolesWidget::onLanguageChanged);

        setImportEnabled(false);
        ui->statusLabel->clear();
    }

    ImportRolesWidget::~ImportRolesWidget()
    {
        delete ui;
    }

    void ImportRolesWidget::changeEvent(QEvent* event)
    {
        if (event->type() == QEvent::LanguageChange)
            retranslate();

        QWidget::changeEvent(event);
    }

    void ImportRolesWidget::onBrowseClicked()
    {
        const QString path = QFileDialog::getOpenFileName(
            this,
            tr("Select a CSV file"),
            QDir::homePath(),
            tr("CSV files (*.csv);;All files (*)")
        );
        if (path.isEmpty()) return;

        ui->filePathEdit->setText(path);
        clearAll();

        // Parse and display the preview immediately
        m_lastResult = utils::RolesCsvParser::parse(path);

        showPreview(m_lastResult);
    }

    void ImportRolesWidget::onImportClicked()
    {
        if (m_lastResult.records.isEmpty()) return;
        m_importRolesProgressDial = new QProgressDialog(tr("Importing roles"),
                                                        tr("Cancel"),
                                                        0,
                                                        static_cast<int>(m_lastResult.records.size()) + 1, this);
        m_importRolesProgressDial->setWindowModality(Qt::WindowModal);
        m_importRolesProgressDial->show();

        QVector<OpenAPI::OAIRoleImportItem> roles;
        for (const auto& record : m_lastResult.records)
        {
            m_importRolesProgressDial->setValue(m_importRolesProgressDial->value() + 1);
            if (m_importRolesProgressDial->wasCanceled())
            {
                QMessageBox::information(this,
                                         tr("Importing roles"),
                                         tr("Role import was cancelled."));
                return;
            }

            QMap<QString, OpenAPI::OAIRoleTranslation> translations;

            for (auto it = record.translations.constBegin(); it != record.translations.constEnd(); ++it)
            {
                OpenAPI::OAIRoleTranslation translation;
                translation.setName(it.value().first);
                translation.setDescription(it.value().second);
                translations.insert(it.key(), std::move(translation));
            }
            OpenAPI::OAIRoleImportItem role;
            role.setName(record.name);
            role.setAlignment(record.alignment);
            role.setRoleType(record.roleType);
            role.setDescription(record.description);
            role.setTranslations(std::move(translations));
            roles.push_back(std::move(role));
        }

        auto* apiClient = api::BotCApiClient::instance();
        connect(apiClient, &api::BotCApiClient::rolesImportFinished,
                this, &ImportRolesWidget::onRolesImportFinished, Qt::SingleShotConnection);
        OpenAPI::OAIRolesImportRequest rolesImportRequest{};
        rolesImportRequest.setRoles(roles);
        apiClient->importRoles(std::move(rolesImportRequest));

        setImportEnabled(false);
    }

    void ImportRolesWidget::onLanguageChanged(const int index)
    {
        m_currentLanguage = ui->languageCombo->itemData(index).toString();
        if (!m_lastResult.records.isEmpty())
            populateTable(m_currentLanguage);
    }

    void ImportRolesWidget::onClearClicked()
    {
        ui->filePathEdit->clear();
        clearAll();
    }

    void ImportRolesWidget::onRolesImportFinished(const OpenAPI::OAIRolesImportResponse& summary,
                                                  QNetworkReply::NetworkError error_type,
                                                  const QString& error_str)
    {
        m_importRolesProgressDial->setValue(m_importRolesProgressDial->maximum());

        const QList<QString> apiErrors = summary.getErrors();

        if (error_type == QNetworkReply::NoError && apiErrors.isEmpty())
        {
            const QString message = tr("Roles created: %1\nRoles updated: %2")
                                    .arg(summary.getRolesCreated())
                                    .arg(summary.getRolesUpdated());
            QMessageBox::information(this, tr("Roles import"), message);
            ui->statusLabel->setText(
                tr("✔ Created: %1, updated: %2")
                .arg(summary.getRolesCreated())
                .arg(summary.getRolesUpdated())
            );
            ui->statusLabel->setStyleSheet("color: green; font-weight: bold;");
        }
        else
        {
            QString message;
            if (!apiErrors.isEmpty())
                message = apiErrors.join("\n");
            else
                message = describeNetworkError(error_type);

            QMessageBox::warning(this, tr("Roles import failed"), message);
            ui->statusLabel->setText(tr("✘ Import finished with an error"));
            ui->statusLabel->setStyleSheet(kErrorBoldStyle);
        }

        setImportEnabled(true);
        emit onRolesImported();
    }

    void ImportRolesWidget::showPreview(const utils::ParseResult& result)
    {
        const QString previousLanguage = m_currentLanguage;
        populateLanguageCombo(result.languages);

        if (!result.errors.isEmpty())
            showErrors(result.errors);
        else
            ui->errorsGroup->setVisible(false);

        int languageIndex = ui->languageCombo->findData(previousLanguage);
        if (languageIndex < 0)
            languageIndex = 0;

        {
            const QSignalBlocker blocker(ui->languageCombo);
            ui->languageCombo->setCurrentIndex(languageIndex);
        }
        m_currentLanguage = ui->languageCombo->itemData(languageIndex).toString();

        // Populate even when there are no records: this clears the previous
        // preview and keeps the column headers consistent after a retranslation.
        populateTable(m_currentLanguage);

        updatePreviewStatus();
    }

    void ImportRolesWidget::populateLanguageCombo(const QStringList& languages)
    {
        const QSignalBlocker blocker(ui->languageCombo);

        ui->languageCombo->clear();
        // The visible label is translated, the item data stays an empty string:
        // it is the language key of the original values and never changes.
        ui->languageCombo->addItem(tr("(original)"), QString());
        for (const QString& language : languages)
            ui->languageCombo->addItem(language, language);
    }

    void ImportRolesWidget::populateTable(const QString& languageCode)
    {
        const bool isOriginal = languageCode.isEmpty();
        const auto& records   = m_lastResult.records;

        // Column identifiers mirror the CSV header and stay untranslated: they
        // are data field names, not interface labels.
        const QStringList headers = isOriginal
                                        ? QStringList{"name", "alignment", "role_type", "description"}
                                        : QStringList{
                                            "name", "alignment", "role_type", "description",
                                            languageCode + "_name", languageCode + "_description"
                                        };

        ui->previewTable->clear();
        ui->previewTable->setColumnCount(headers.size());
        ui->previewTable->setRowCount(records.size());
        ui->previewTable->setHorizontalHeaderLabels(headers);

        // Rows with validation errors, taken from the parser instead of the
        // message text: the text is localized and must not be parsed.
        QSet<int> errorRows;
        for (const int lineNumber : m_lastResult.errorLines)
        {
            if (lineNumber >= 2)
                errorRows.insert(lineNumber - 2); // -2: header line + 0-based row
        }

        for (int row = 0; row < records.size(); ++row)
        {
            const utils::RoleRecord& record = records[row];
            const bool hasError             = errorRows.contains(row);

            QStringList cells = {record.name, record.alignment, record.roleType, record.description};
            if (!isOriginal)
            {
                cells << record.translations.value(languageCode).first;
                cells << record.translations.value(languageCode).second;
            }

            for (int col = 0; col < cells.size(); ++col)
            {
                auto* item = new QTableWidgetItem(cells[col]);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable); // read-only preview

                if (hasError)
                    item->setBackground(QColor(255, 220, 220));

                ui->previewTable->setItem(row, col, item);
            }
        }

        ui->previewTable->horizontalHeader()->setSectionResizeMode(
            3, QHeaderView::Stretch // the description column takes the free space
        );
        ui->previewTable->resizeColumnsToContents();
    }

    void ImportRolesWidget::updatePreviewStatus()
    {
        if (m_lastResult.records.isEmpty())
        {
            ui->statusLabel->setText(tr("✘ No records loaded."));
            ui->statusLabel->setStyleSheet(kErrorStyle);
            setImportEnabled(false);
            return;
        }

        const bool hasErrors = !m_lastResult.errors.isEmpty();
        ui->statusLabel->setText(
            hasErrors
                ? tr("⚠ Loaded %1 records, validation errors found").arg(m_lastResult.records.size())
                : tr("✔ Loaded %1 records, no errors").arg(m_lastResult.records.size())
        );
        ui->statusLabel->setStyleSheet(statusStyle(!hasErrors));

        // Import is enabled even with errors: the user decides whether to proceed.
        setImportEnabled(true);
    }

    void ImportRolesWidget::retranslate()
    {
        ui->retranslateUi(this);

        const QString path = ui->filePathEdit->text();
        if (path.isEmpty())
        {
            ui->statusLabel->clear();
            return;
        }

        // Re-parse so that parser errors and the preview status follow the new
        // language as well; the file is small and already on disk.
        m_lastResult = utils::RolesCsvParser::parse(path);
        showPreview(m_lastResult);
    }

    void ImportRolesWidget::showErrors(const QStringList& errors) const
    {
        ui->errorsWidget->clear();
        for (const QString& err : errors)
            ui->errorsWidget->addItem(err);

        ui->errorsGroup->setVisible(true);
    }

    void ImportRolesWidget::clearAll()
    {
        m_lastResult = {};
        m_currentLanguage.clear();
        ui->previewTable->clear();
        ui->previewTable->setRowCount(0);
        ui->previewTable->setColumnCount(0);
        ui->errorsWidget->clear();
        ui->errorsGroup->setVisible(false);
        ui->statusLabel->clear();
        populateLanguageCombo({});
        setImportEnabled(false);
    }

    void ImportRolesWidget::setImportEnabled(const bool enabled) const
    {
        ui->importButton->setEnabled(enabled);
    }

    QString ImportRolesWidget::statusStyle(const bool ok)
    {
        return ok
                   ? "color: green; font-weight: bold;"
                   : "color: orange; font-weight: bold;";
    }
} // namespace botc::ui
