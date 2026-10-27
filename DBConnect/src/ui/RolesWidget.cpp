#include "RolesWidget.h"

#include <QEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>

#include "NetworkErrorMessages.h"
#include "RolesTableModel.h"
#include "ui_RolesWidget.h"
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
        ui->rolesTable->horizontalHeader()->setSectionResizeMode(RolesTableModel::Name, QHeaderView::ResizeToContents);
        ui->rolesTable->horizontalHeader()->setSectionResizeMode(RolesTableModel::Type, QHeaderView::ResizeToContents);
        ui->rolesTable->horizontalHeader()->setSectionResizeMode(RolesTableModel::Side, QHeaderView::ResizeToContents);
        ui->rolesTable->horizontalHeader()->setSectionResizeMode(RolesTableModel::Description, QHeaderView::Stretch);
        ui->rolesTable->horizontalHeader()->setSectionResizeMode(RolesTableModel::NameRu, QHeaderView::ResizeToContents);
        ui->rolesTable->horizontalHeader()->setSectionResizeMode(RolesTableModel::TypeRu, QHeaderView::ResizeToContents);
        ui->rolesTable->horizontalHeader()->setSectionResizeMode(RolesTableModel::SideRu, QHeaderView::ResizeToContents);
        ui->rolesTable->horizontalHeader()->setSectionResizeMode(RolesTableModel::DescriptionRu, QHeaderView::Stretch);
        ui->rolesTable->horizontalHeader()->setStretchLastSection(false);

        connect(ui->searchEdit, &QLineEdit::textChanged,
                this, &RolesWidget::onSearchTextChanged);
        connect(ui->refreshButton, &QPushButton::clicked,
                this, &RolesWidget::onRefreshClicked);
        connect(DBCache::instance(), &DBCache::onRolesCacheUpdated,
                this, &RolesWidget::refreshFromCache);
        connect(DBCache::instance(), &DBCache::rolesRequestStarted,
                this, &RolesWidget::onRolesRequestStarted);

        refreshFromCache();
        DBCache::instance()->updateRoles(QString("ru"));
        setRefreshBusy(DBCache::instance()->isRolesRequestInFlight());
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
    }

    void RolesWidget::onRolesRequestStarted()
    {
        setRefreshBusy(true);
        applyStatus();
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
        ui->refreshButton->setEnabled(!isBusy);
    }
} // namespace botc::ui
