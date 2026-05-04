#pragma once

#include <QWidget>
#include "../utils/games/parser/GamesCsvParser.h"
#include "../api/models/GameImportResponse.h"
#include "../api/models/GameImportRequest.h"

class QProgressDialog;

namespace botc::api
{
    class BotCApiClient;
}

namespace botc::api::models::games
{
    struct GameImportRequest;
    struct GameImportResponse;
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

    private slots:
        void onBrowseClicked();
        void onImportClicked();
        void onClearClicked();
        void onGameSelected(int row);

    private:
        void initApiClient();
        void showPreview(const utils::games::GamesParseResult& result);
        void populateGamesTable();
        void populatePlayersTable(int partyIndex);
        void showErrors(const QStringList& errors) const;
        void clearAll();
        void setImportEnabled(bool enabled) const;
        static QString statusStyle(bool ok);

        void onGamesImportFinished(bool in_bSuccess, const api::models::games::GameImportResponse& in_response);

    private:
        Ui::ImportGamesWidget* ui;
        utils::games::GamesParseResult m_lastResult;
        api::BotCApiClient* m_apiClient            = nullptr;
        QProgressDialog* m_importRolesProgressDial = nullptr;
        // TODO:: add batch game import and remove
        uint m_importCount = 0;
        // TODO:: add batch game import and remove
        QVector<api::models::games::GameImportResponse> responses;
    };
} // botc::ui
