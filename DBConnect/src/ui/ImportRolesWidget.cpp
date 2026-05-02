#include "ImportRolesWidget.h"

#include <QFileDialog>

#include "ui_ImportRolesWidget.h"

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

        // Парсим и сразу показываем предпросмотр
        utils::RolesCsvParser parser;
        m_lastResult = parser.parse(path);

        showPreview(m_lastResult);
    }

    void ImportRolesWidget::onImportClicked()
    {
        if (m_lastResult.records.isEmpty()) return;

        emit rolesImported(m_lastResult.records);

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

    void ImportRolesWidget::showErrors(const QStringList& errors)
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

    void ImportRolesWidget::setImportEnabled(bool enabled)
    {
        ui->importButton->setEnabled(enabled);
    }

    QString ImportRolesWidget::statusStyle(bool ok)
    {
        return ok
                   ? "color: green; font-weight: bold;"
                   : "color: orange; font-weight: bold;";
    }
} // botc::ui
