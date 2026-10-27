#include "RolesWidget.h"

#include <QEvent>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSortFilterProxyModel>

#include "NetworkErrorMessages.h"
#include "RoleEditDialog.h"
#include "RolesTableModel.h"
#include "OAIRoleItem.h"
#include "OAIRoleTranslation.h"
#include "OAIRoleUpdateRequest.h"
#include "ui_RolesWidget.h"
#include "../api/BotCApiClient.h"
#include "../db_cache/DBCache.h"

namespace
{
    const QString kErrorStyle = "color: red; font-weight: bold;";
    const QString kOkStyle = "color: green;";
    const QString kMutedStyle = "color: gray;";
}

namespace botc::ui
{
    RolesWidget::RolesWidget(QWidget* parent) :
        QWidget(parent), ui(new Ui::RolesWidget)
    {
        ui->setupUi(this);

        m_model = new RolesTableModel(this);
        m_proxy = new QSortFilterProxyModel(this);
        m_proxy->setSourceModel(m_model);
        m_proxy->setFilterKeyColumn(-1);
        m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);

        ui->rolesTable->setModel(m_proxy);
        ui->rolesTable->setSortingEnabled(true);
        ui->rolesTable->sortByColumn(RolesTableModel::Name, Qt::AscendingOrder);
        ui->rolesTable->verticalHeader()->setVisible(false);
        QHeaderView* header = ui->rolesTable->horizontalHeader();
        header->setSectionResizeMode(QHeaderView::Interactive);
        header->setStretchLastSection(false);
        ui->rolesTable->setColumnWidth(RolesTableModel::Name, 150);
        ui->rolesTable->setColumnWidth(RolesTableModel::Type, 110);
        ui->rolesTable->setColumnWidth(RolesTableModel::Side, 90);
        ui->rolesTable->setColumnWidth(RolesTableModel::Description, 220);
        ui->rolesTable->setColumnWidth(RolesTableModel::NameRu, 150);
        ui->rolesTable->setColumnWidth(RolesTableModel::TypeRu, 120);
        ui->rolesTable->setColumnWidth(RolesTableModel::SideRu, 110);
        ui->rolesTable->setColumnWidth(RolesTableModel::DescriptionRu, 220);

        connect(ui->searchEdit, &QLineEdit::textChanged,
                this, &RolesWidget::onSearchTextChanged);
        connect(ui->refreshButton, &QPushButton::clicked,
                this, &RolesWidget::onRefreshClicked);
        connect(ui->editButton, &QPushButton::clicked,
                this, &RolesWidget::onEditClicked);
        connect(ui->rolesTable->selectionModel(), &QItemSelectionModel::selectionChanged,
                this, &RolesWidget::onSelectionChanged);
        connect(ui->rolesTable, &QTableView::doubleClicked,
                this, [this](const QModelIndex&) { onEditClicked(); });
        connect(api::BotCApiClient::instance(), &api::BotCApiClient::roleUpdateFinished,
                this, &RolesWidget::onRoleUpdateFinished);
        connect(DBCache::instance(), &DBCache::onRolesCacheUpdated,
                this, &RolesWidget::refreshFromCache);
        connect(DBCache::instance(), &DBCache::rolesRequestStarted,
                this, &RolesWidget::onRolesRequestStarted);

        refreshFromCache();
        DBCache::instance()->updateRoles(QString("ru"));
        setRefreshBusy(DBCache::instance()->isRolesRequestInFlight());
        onSelectionChanged();
    }

    RolesWidget::~RolesWidget()
    {
        delete ui;
    }

    void RolesWidget::changeEvent(QEvent* event)
    {
        if (event->type() == QEvent::LanguageChange)
            retranslate();

        QWidget::changeEvent(event);
    }

    void RolesWidget::retranslate()
    {
        ui->retranslateUi(this);
        m_model->retranslate();
        updateCounts();
        applyStatus();
    }

    void RolesWidget::onSearchTextChanged(const QString& text)
    {
        m_proxy->setFilterFixedString(text);
        updateCounts();
        applyStatus();
    }

    void RolesWidget::onRefreshClicked()
    {
        setRefreshBusy(true);
        DBCache::instance()->updateRoles(QString("ru"));
        applyStatus();
    }

    void RolesWidget::refreshFromCache()
    {
        const QString selectedId = selectedRoleId();
        m_model->setRoles(DBCache::instance()->getRoleEntries());
        selectRoleById(selectedId);
        updateCounts();
        applyStatus();
        setRefreshBusy(DBCache::instance()->isRolesRequestInFlight());
        onSelectionChanged();
    }

    void RolesWidget::onRolesRequestStarted()
    {
        setRefreshBusy(true);
        onSelectionChanged();
        applyStatus();
    }

    void RolesWidget::onSelectionChanged()
    {
        const bool hasSelection = !selectedRoleId().isEmpty();
        ui->editButton->setEnabled(hasSelection && !m_isRoleUpdateInFlight
                                   && !DBCache::instance()->isRolesRequestInFlight());
    }

    void RolesWidget::onEditClicked()
    {
        if (m_isRoleUpdateInFlight || DBCache::instance()->isRolesRequestInFlight()) return;

        const QString roleId = selectedRoleId();
        if (roleId.isEmpty()) return;

        RoleEntry selectedRole;
        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            if (m_model->roleIdAt(row) != roleId) continue;
            selectedRole = m_model->roleAt(row);
            break;
        }
        if (selectedRole.id.isEmpty()) return;

        RoleEditDialog dialog(selectedRole, this);
        if (dialog.exec() != QDialog::Accepted) return;

        const RoleEntry editedRole = dialog.editedRole();
        const auto answer = QMessageBox::warning(
            this,
            tr("Confirm role changes"),
            tr("Changing the role type or side affects historical statistics. "
               "Changing the English name affects role imports, and changing the Russian name "
               "affects game CSV imports. Save these changes?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (answer != QMessageBox::Yes) return;

        m_isRoleUpdateInFlight = true;
        setRoleUpdateBusy(true);
        m_actionMessage.clear();
        applyStatus();

        OpenAPI::OAIRoleUpdateRequest request;
        request.setName(editedRole.name);
        request.setRoleType(editedRole.roleType);
        request.setAlignment(editedRole.alignment);
        request.setDescription(editedRole.description);
        QMap<QString, OpenAPI::OAIRoleTranslation> translations;
        QSet<QString> translationsToDelete;
        if (dialog.hasRussianTranslation())
        {
            OpenAPI::OAIRoleTranslation translation;
            translation.setName(editedRole.translationName);
            translation.setDescription(editedRole.translationDescription);
            translations.insert(QStringLiteral("ru"), translation);
        }
        else
        {
            translationsToDelete.insert(QStringLiteral("ru"));
        }
        request.setTranslations(translations);
        request.setTranslationsToDelete(translationsToDelete);
        api::BotCApiClient::instance()->updateRole(roleId, request, QStringLiteral("ru"));
    }

    void RolesWidget::onRoleUpdateFinished(const OpenAPI::OAIRoleItem& role,
                                           const QNetworkReply::NetworkError errorType,
                                           const int httpStatus, const QString& errorStr)
    {
        if (!m_isRoleUpdateInFlight) return;

        m_isRoleUpdateInFlight = false;
        setRoleUpdateBusy(false);

        switch (httpStatus)
        {
        case 200:
        {
            RoleEntry updated{
                role.getId(), role.getName(), role.getRoleType(), role.getRoleTypeTranslation(),
                role.getAlignment(), role.getAlignmentTranslation(), role.getDescription(), {}, {}
            };
            if (role.is_translation_Set())
            {
                const OpenAPI::OAIRoleTranslation translation = role.getTranslation();
                updated.translationName = translation.getName();
                updated.translationDescription = translation.getDescription();
            }
            DBCache::instance()->applyRoleUpdate(updated);
            showMessage(tr("Role \"%1\" updated.").arg(updated.name), kOkStyle);
            DBCache::instance()->updateRoles(QStringLiteral("ru"));
            return;
        }
        case 401:
            showMessage(tr("Missing or invalid API key. The role was not changed."), kErrorStyle);
            return;
        case 403:
            showMessage(tr("API key has been revoked. The role was not changed."), kErrorStyle);
            return;
        case 404:
            showMessage(tr("Role not found. The list has been refreshed."), kErrorStyle);
            DBCache::instance()->updateRoles(QStringLiteral("ru"));
            return;
        case 409:
            showMessage(tr("A role with this English name already exists. The role was not changed."), kErrorStyle);
            return;
        case 422:
            showMessage(tr("Invalid role data. The role was not changed."), kErrorStyle);
            return;
        default:
            showMessage(tr("Failed to update the role. %1").arg(describeNetworkError(errorType)), kErrorStyle);
            ui->statusLabel->setToolTip(errorStr);
            return;
        }
    }

    QString RolesWidget::selectedRoleId() const
    {
        const QModelIndexList selectedRows = ui->rolesTable->selectionModel()->selectedRows();
        if (selectedRows.isEmpty()) return {};

        return m_model->roleIdAt(m_proxy->mapToSource(selectedRows.first()).row());
    }

    void RolesWidget::selectRoleById(const QString& roleId)
    {
        if (roleId.isEmpty()) return;

        for (int row = 0; row < m_model->rowCount(); ++row)
        {
            if (m_model->roleIdAt(row) != roleId) continue;

            const QModelIndex proxyIndex = m_proxy->mapFromSource(m_model->index(row, RolesTableModel::Name));
            if (proxyIndex.isValid()) ui->rolesTable->selectRow(proxyIndex.row());
            return;
        }
    }

    void RolesWidget::updateCounts()
    {
        ui->countLabel->setText(tr("Roles: %1, shown: %2")
            .arg(m_model->rowCount())
            .arg(m_proxy->rowCount()));
    }

    void RolesWidget::applyStatus()
    {
        if (DBCache::instance()->isRolesRequestInFlight())
        {
            ui->statusLabel->setText(tr("Loading roles..."));
            ui->statusLabel->setStyleSheet(kMutedStyle);
            ui->statusLabel->setToolTip(QString());
            return;
        }

        const QNetworkReply::NetworkError errorType = DBCache::instance()->getRolesErrorType();
        if (errorType != QNetworkReply::NoError)
        {
            ui->statusLabel->setText(
                tr("✘ Failed to load roles. %1").arg(describeNetworkError(errorType)));
            ui->statusLabel->setStyleSheet(kErrorStyle);
            ui->statusLabel->setToolTip(DBCache::instance()->getRolesError());
            return;
        }

        ui->statusLabel->setToolTip(QString());
        if (m_isRoleUpdateInFlight)
        {
            ui->statusLabel->setText(tr("Updating role..."));
            ui->statusLabel->setStyleSheet(kMutedStyle);
            return;
        }

        if (!m_actionMessage.isEmpty())
        {
            ui->statusLabel->setText(m_actionMessage);
            ui->statusLabel->setStyleSheet(m_actionStyle);
            return;
        }

        if (m_model->rowCount() == 0)
        {
            ui->statusLabel->setText(tr("No roles in the database."));
            ui->statusLabel->setStyleSheet(kMutedStyle);
            return;
        }

        ui->statusLabel->setText(tr("✔ Roles list loaded."));
        ui->statusLabel->setStyleSheet(kOkStyle);
    }

    void RolesWidget::setRefreshBusy(const bool isBusy)
    {
        ui->refreshButton->setEnabled(!isBusy && !m_isRoleUpdateInFlight);
    }

    void RolesWidget::setRoleUpdateBusy(const bool isBusy)
    {
        if (isBusy)
        {
            ui->editButton->setEnabled(false);
            ui->refreshButton->setEnabled(false);
        }
        else
        {
            setRefreshBusy(DBCache::instance()->isRolesRequestInFlight());
            onSelectionChanged();
        }
    }

    void RolesWidget::showMessage(const QString& message, const QString& style)
    {
        m_actionMessage = message;
        m_actionStyle = style;
        applyStatus();
    }
} // namespace botc::ui
