#pragma once

#include <QWidget>
#include "../utils/games/parser/GamesCsvParser.h"

namespace botc::utils::games
{
    struct GameRecord;
}

namespace botc::ui
{
    QT_BEGIN_NAMESPACE

    namespace Ui
    {
        class ImportGamesWidget;
    }

    QT_END_NAMESPACE

    class ImportGamesWidget : public QWidget
    {
        Q_OBJECT

    public:
        explicit ImportGamesWidget(QWidget* parent = nullptr);
        ~ImportGamesWidget() override;
    signals:
        void partiesImported(const QList<utils::games::GameRecord>& records);

    private slots:
        void onBrowseClicked();
        void onImportClicked();
        void onClearClicked();
        void onGameSelected(int row);

    private:
        void showPreview(const utils::games::GamesParseResult& result);
        void populatePartiesTable();
        void populatePlayersTable(int partyIndex);
        void showErrors(const QStringList& errors);
        void clearAll();
        void setImportEnabled(bool enabled);
        QString statusStyle(bool ok);

        QSet<int> errorRowsFor(const QStringList& errors, const QString& prefix);

    private:
        Ui::ImportGamesWidget* ui;
        utils::games::GamesParseResult m_lastResult;
    };
} // botc::ui
