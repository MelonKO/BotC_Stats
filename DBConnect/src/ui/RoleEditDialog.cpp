#include "RoleEditDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPlainTextEdit>

#include "ui_RoleEditDialog.h"

namespace botc::ui
{
    RoleEditDialog::RoleEditDialog(const RoleEntry& role, QWidget* parent) :
        QDialog(parent), ui(new Ui::RoleEditDialog), m_role(role)
    {
        ui->setupUi(this);

        ui->roleTypeCombo->addItem(tr("Townsfolk"), QStringLiteral("Townsfolk"));
        ui->roleTypeCombo->addItem(tr("Outsider"), QStringLiteral("Outsider"));
        ui->roleTypeCombo->addItem(tr("Minion"), QStringLiteral("Minion"));
        ui->roleTypeCombo->addItem(tr("Demon"), QStringLiteral("Demon"));
        ui->roleTypeCombo->addItem(tr("Traveller"), QStringLiteral("Traveller"));

        ui->alignmentCombo->addItem(tr("good"), QStringLiteral("good"));
        ui->alignmentCombo->addItem(tr("evil"), QStringLiteral("evil"));
        ui->alignmentCombo->addItem(tr("neutral"), QStringLiteral("neutral"));

        ui->nameEdit->setText(role.name);
        ui->roleTypeCombo->setCurrentIndex(ui->roleTypeCombo->findData(role.roleType));
        ui->alignmentCombo->setCurrentIndex(ui->alignmentCombo->findData(role.alignment));
        ui->descriptionEdit->setPlainText(role.description);
        ui->translationNameEdit->setText(role.translationName);
        ui->translationDescriptionEdit->setPlainText(role.translationDescription);
        ui->validationLabel->clear();

        connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &RoleEditDialog::accept);
        connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &RoleEditDialog::reject);
    }

    RoleEditDialog::~RoleEditDialog()
    {
        delete ui;
    }

    RoleEntry RoleEditDialog::editedRole() const
    {
        RoleEntry edited = m_role;
        edited.name = ui->nameEdit->text().trimmed();
        edited.roleType = ui->roleTypeCombo->currentData().toString();
        edited.alignment = ui->alignmentCombo->currentData().toString();
        edited.description = ui->descriptionEdit->toPlainText();
        edited.translationName = ui->translationNameEdit->text().trimmed();
        edited.translationDescription = ui->translationDescriptionEdit->toPlainText();
        return edited;
    }

    bool RoleEditDialog::hasRussianTranslation() const
    {
        return !ui->translationNameEdit->text().trimmed().isEmpty();
    }

    void RoleEditDialog::accept()
    {
        const QString name = ui->nameEdit->text().trimmed();
        const QString description = ui->descriptionEdit->toPlainText();
        const QString translationName = ui->translationNameEdit->text().trimmed();
        const QString translationDescription = ui->translationDescriptionEdit->toPlainText();

        if (name.isEmpty() || name.size() > 200)
        {
            ui->validationLabel->setText(tr("English name must contain 1 to 200 characters."));
            ui->nameEdit->setFocus();
            return;
        }
        if (description.size() > 2000)
        {
            ui->validationLabel->setText(tr("English description must not exceed 2000 characters."));
            ui->descriptionEdit->setFocus();
            return;
        }
        if (translationName.size() > 200)
        {
            ui->validationLabel->setText(tr("Russian name must not exceed 200 characters."));
            ui->translationNameEdit->setFocus();
            return;
        }
        if (translationDescription.size() > 2000)
        {
            ui->validationLabel->setText(tr("Russian description must not exceed 2000 characters."));
            ui->translationDescriptionEdit->setFocus();
            return;
        }
        if (translationName.isEmpty() && !translationDescription.isEmpty())
        {
            ui->validationLabel->setText(tr("A Russian description requires a Russian name."));
            ui->translationNameEdit->setFocus();
            return;
        }

        QDialog::accept();
    }
} // namespace botc::ui
