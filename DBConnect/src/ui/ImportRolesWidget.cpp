#include "ImportRolesWidget.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>

#include "ui_ImportRolesWidget.h"
#include "../api/BotCApiClient.h"
#include "../api/models/RolesImportRequest.h"
#include "../api/models/RolesImportResponse.h"
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

        initApiClient();
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

        // Парсим и сразу показываем предпросмотр
        utils::RolesCsvParser parser;
        m_lastResult = parser.parse(path);

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

        QVector<api::models::roles::RoleImportItem> roles;
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

            QMap<QString, api::models::roles::RoleTranslation> translations;

            for (auto it = record.translations.constBegin(); it != record.translations.constEnd(); ++it)
            {
                translations.insert(it.key(),
                                    api::models::roles::RoleTranslation{
                                        .name        = it.value().first,
                                        .description = it.value().second
                                    });
            }

            roles.push_back(api::models::roles::RoleImportItem{
                .name         = record.name,
                .alignment    = record.alignment,
                .roleType     = record.roleType,
                .description  = record.description,
                .translations = translations
            });
        }

        connect(m_apiClient, &api::BotCApiClient::rolesImportFinished, this, &ImportRolesWidget::onRolesImportFinished);
        m_apiClient->importRoles(api::models::roles::RolesImportRequest{std::move(roles)});

        ui->statusLabel->setText(
            QString("✔ Импортировано записей: %1").arg(m_lastResult.records.size())
        );
        ui->statusLabel->setStyleSheet("color: green; font-weight: bold;");
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

    void ImportRolesWidget::onRolesImportFinished(const bool in_bSuccess,
                                                  const api::models::roles::RolesImportResponse& in_response)
    {
        m_importRolesProgressDial->setValue(m_importRolesProgressDial->maximum());
        disconnect(m_apiClient, &api::BotCApiClient::rolesImportFinished, this,
                   &ImportRolesWidget::onRolesImportFinished);
        if (in_bSuccess)
        {
            QString message = "Импорт ролей прошёл успешно.";

            message += "\nStatus: " + in_response.status;
            message += "\n Roles created: " + std::to_string(in_response.rolesCreated);
            message += "\n Roles updated: " + std::to_string(in_response.rolesUpdated);

            QMessageBox::information(this, "Импорт ролей", message);
        }
        else
        {
            QString message = "Импорт ролей произошёл с ошибкой";
            message         += "\nStatus: " + in_response.status;
            QMessageBox::warning(this,
                                 "Импорт ролей",
                                 message);
        }
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

    void ImportRolesWidget::initApiClient()
    {
        const auto ConfigManager = config::ConfigManager::instance();
        m_apiClient              = new api::BotCApiClient(ConfigManager->getApiUrl(), ConfigManager->getApiKey(),
                                             ConfigManager->getSslVerify(), this);
    }
} // botc::ui
