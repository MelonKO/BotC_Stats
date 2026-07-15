#pragma once

#include <QWidget>

#include "QNetworkReply"
#include "../utils/games/parser/GamesCsvParser.h"

namespace OpenAPI
{
    class OAIGameImportStatusResponse;
    class OAIGamesImportStatusResponse;
    class OAIGamesImportRequest;
}

class QProgressDialog;

namespace botc::api
{
    class BotCApiClient;
}

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
        void onGamesImported();

    private slots:
        void onBrowseClicked();
        void onImportClicked();
        void onClearClicked();
        void onGameSelected(int row);

    private:
        void showPreview(const utils::games::GamesParseResult& result);
        void populateGamesTable();
        void populatePlayersTable(int partyIndex);
        void showErrors(const QStringList& errors) const;
        void showInfo(const QStringList& messages) const;
        void clearAll();
        void setImportEnabled(bool enabled) const;
        static QString statusStyle(bool ok);

        void onGamesImportBatchFinished(const OpenAPI::OAIGamesImportStatusResponse& summary,
                                        QNetworkReply::NetworkError error_type,
                                        const QString& error_str);

    private:
        Ui::ImportGamesWidget* ui;
        utils::games::GamesParseResult m_lastResult;
        QProgressDialog* m_importRolesProgressDial = nullptr;
    };
} // botc::ui
