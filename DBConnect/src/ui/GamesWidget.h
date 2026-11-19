#pragma once

#include <QDate>
#include <QNetworkReply>
#include <QString>
#include <QVector>
#include <QWidget>

namespace OpenAPI
{
    class OAIGameDetailsResponse;
    class OAIGamesResponse;
}

namespace botc::ui
{
    QT_BEGIN_NAMESPACE
    namespace Ui
    {
        class GamesWidget;
    }
    QT_END_NAMESPACE

    struct GameSummary
    {
        QString id;
        QDate gameDate;
        int gameNumber = 0;
        QString scenarioName;
        QString location;
        QString storyteller;
        QString winningSide;
        int playerCount = 0;
        int survivorCount = 0;
        QString duration;
    };

    /** Browsable, paginated list of games and the roster of the selected game. */
    class GamesWidget final : public QWidget
    {
        Q_OBJECT

    public:
        static constexpr int kGamesPerPage = 50;

        explicit GamesWidget(QWidget* parent = nullptr);
        ~GamesWidget() override;
        void refreshGames();

    protected:
        void changeEvent(QEvent* event) override;

    private slots:
        void onRefreshClicked();
        void onSearchTextChanged(const QString& text);
        void onGameSelectionChanged();
        void onPreviousPageClicked();
        void onNextPageClicked();
        void onGamesListFinished(const OpenAPI::OAIGamesResponse& summary,
                                 QNetworkReply::NetworkError errorType, const QString& errorString);
        void onGameDetailsFinished(const OpenAPI::OAIGameDetailsResponse& summary,
                                   QNetworkReply::NetworkError errorType, const QString& errorString);

    private:
        void loadPage();
        void populateGames(const QString& preferredGameId);
        void populateDetails(const OpenAPI::OAIGameDetailsResponse& summary);
        void requestSelectedGameDetails();
        QString selectedGameId() const;
        void updateVisibleRows(const QString& text);
        void updatePagination();
        void updateStatus();
        void retranslate();

        Ui::GamesWidget* ui;
        QVector<GameSummary> m_games;
        int m_totalGames = 0;
        int m_pageOffset = 0;
        int m_pageSize = kGamesPerPage;
        bool m_isListRequestInFlight = false;
        bool m_isDetailsRequestInFlight = false;
        bool m_refreshPending = false;
        QString m_requestedDetailsId;
        QNetworkReply::NetworkError m_lastError = QNetworkReply::NoError;
        QString m_lastErrorString;
    };
}
