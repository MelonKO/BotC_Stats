#include "ImportRolesWidget.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>

#include "OAIImportRoles_200_response.h"
#include "OAIImportRoles_request.h"
#include "ui_ImportRolesWidget.h"
#include "../api/BotCApiClient.h"
#include "../config/ConfigManager.h"

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
        connect(ui->languageCombo, &QComboBox::currentTextChanged,
                this, &ImportRolesWidget::onLanguageChanged);

        setImportEnabled(false);
        ui->statusLabel->clear();
    }

    ImportRolesWidget::~ImportRolesWidget()
    {
        delete ui;
    }

    void ImportRolesWidget::onBrowseClicked()
    {
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Выберите CSV файл",
            QDir::homePath(),
            "CSV файлы (*.csv);;Все файлы (*)"
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
        m_importRolesProgressDial = new QProgressDialog("Импортирование ролей",
                                                        "Отмена",
                                                        0,
                                                        static_cast<int>(m_lastResult.records.size()) + 1, this);
        m_importRolesProgressDial->setWindowModality(Qt::WindowModal);
        m_importRolesProgressDial->show();

        QVector<OpenAPI::OAIImportRoles_request_roles_inner> roles;
        for (const auto& record : m_lastResult.records)
        {
            m_importRolesProgressDial->setValue(m_importRolesProgressDial->value() + 1);
            if (m_importRolesProgressDial->wasCanceled())
            {
                QMessageBox::information(this,
                                         "Импортирование ролей",
                                         "Импортирование ролей было отменено");
                return;
            }

            QMap<QString, OpenAPI::OAIImportRoles_request_roles_inner_translations_value> translations;

            for (auto it = record.translations.constBegin(); it != record.translations.constEnd(); ++it)
            {
                OpenAPI::OAIImportRoles_request_roles_inner_translations_value translation;
                translation.setName(it.value().first);
                translation.setDescription(it.value().second);
                translations.insert(it.key(), std::move(translation));
            }
            OpenAPI::OAIImportRoles_request_roles_inner role;
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
        OpenAPI::OAIImportRoles_request rolesImportRequest{};
        rolesImportRequest.setRoles(roles);
        apiClient->importRoles(std::move(rolesImportRequest));

        setImportEnabled(false);
    }

    void ImportRolesWidget::onLanguageChanged(const QString& lang)
    {
        m_currentLanguage = lang;
        if (!m_lastResult.records.isEmpty())
            populateTable(lang);
    }

    void ImportRolesWidget::onClearClicked()
    {
        ui->filePathEdit->clear();
        clearAll();
    }

    void ImportRolesWidget::onRolesImportFinished(const OpenAPI::OAIImportRoles_200_response& summary,
                                                  QNetworkReply::NetworkError error_type,
                                                  const QString& error_str)
    {
        m_importRolesProgressDial->setValue(m_importRolesProgressDial->maximum());
        if (error_type == QNetworkReply::NoError)
        {
            QString message = "Импорт ролей прошёл успешно.";

            message += "\nStatus: " + summary.getStatus();
            message += "\n Roles created: " + std::to_string(summary.getRolesCreated());
            message += "\n Roles updated: " + std::to_string(summary.getRolesUpdated());

            QMessageBox::information(this, "Импорт ролей", message);
            ui->statusLabel->setText(
                QString("✔ Импортировано записей: %1; Обновлено записей %2")
                .arg(summary.getRolesCreated())
                .arg(summary.getRolesUpdated())
            );
            ui->statusLabel->setStyleSheet("color: green; font-weight: bold;");
        }
        else
        {
            QString message = "Импорт ролей произошёл с ошибкой";
            message         += "\nStatus: " + error_str;
            QMessageBox::warning(this,
                                 "Импорт ролей",
                                 message);

            ui->statusLabel->setText(
                QString("Импорт завершён с ошибкой: %1").arg(m_lastResult.records.size())
            );
            ui->statusLabel->setStyleSheet("color: red; font-weight: bold;");
        }
        emit onRolesImported();
    }

    void ImportRolesWidget::showPreview(const utils::ParseResult& result)
    {
        // Заполняем комбобокс языков
        ui->languageCombo->clear();
        ui->languageCombo->addItem("(оригинал)");
        for (const QString& lang : result.languages)
            ui->languageCombo->addItem(lang);

        if (!result.errors.isEmpty())
            showErrors(result.errors);

        if (result.records.isEmpty())
        {
            ui->statusLabel->setText("✘ Записи не загружены.");
            ui->statusLabel->setStyleSheet("color: red;");
            setImportEnabled(false);
            return;
        }

        populateTable("(оригинал)");

        const bool hasErrors = !result.errors.isEmpty();
        ui->statusLabel->setText(
            hasErrors
                ? QString("⚠ Загружено %1 записей, обнаружены ошибки валидации")
                .arg(result.records.size())
                : QString("✔ Загружено %1 записей, ошибок нет")
                .arg(result.records.size())
        );
        ui->statusLabel->setStyleSheet(statusStyle(!hasErrors));

        // Импорт разрешаем даже при ошибках — пусть пользователь решает
        setImportEnabled(true);
    }

    void ImportRolesWidget::populateTable(const QString& language)
    {
        const bool isOriginal = (language == "(оригинал)");
        const auto& records   = m_lastResult.records;

        // Заголовки таблицы
        const QStringList headers = isOriginal
                                        ? QStringList{"name", "alignment", "role_type", "description"}
                                        : QStringList{
                                            "name", "alignment", "role_type", "description",
                                            language + "_name", language + "_description"
                                        };

        ui->previewTable->clear();
        ui->previewTable->setColumnCount(headers.size());
        ui->previewTable->setRowCount(records.size());
        ui->previewTable->setHorizontalHeaderLabels(headers);

        // Ошибочные строки — для подсветки
        QSet<int> errorRows;
        for (const QString& err : m_lastResult.errors)
        {
            // Извлекаем номер строки из "Строка N: ..."
            static const QRegularExpression rx(R"(Строка (\d+):)");
            auto m = rx.match(err);
            if (m.hasMatch())
                errorRows.insert(m.captured(1).toInt() - 2); // -2: заголовок + 0-based
        }

        for (int row = 0; row < records.size(); ++row)
        {
            const utils::RoleRecord& r = records[row];
            const bool hasError        = errorRows.contains(row);

            QStringList cells = {r.name, r.alignment, r.roleType, r.description};
            if (!isOriginal)
            {
                cells << r.translations.value(language).first;
                cells << r.translations.value(language).second;
            }

            for (int col = 0; col < cells.size(); ++col)
            {
                auto* item = new QTableWidgetItem(cells[col]);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable); // только чтение

                // Подсвечиваем ошибочные строки
                if (hasError)
                    item->setBackground(QColor(255, 220, 220));

                ui->previewTable->setItem(row, col, item);
            }
        }

        ui->previewTable->horizontalHeader()->setSectionResizeMode(
            3, QHeaderView::Stretch // description растягивается
        );
        ui->previewTable->resizeColumnsToContents();
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
        ui->previewTable->clear();
        ui->previewTable->setRowCount(0);
        ui->previewTable->setColumnCount(0);
        ui->errorsWidget->clear();
        ui->errorsGroup->setVisible(false);
        ui->statusLabel->clear();
        ui->languageCombo->clear();
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
} // botc::ui
