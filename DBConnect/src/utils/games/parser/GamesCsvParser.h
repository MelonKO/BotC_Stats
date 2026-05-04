#pragma once

#include <QDate>
#include <QList>
#include <QString>

namespace botc::utils::games
{
    struct PlayerRecord
    {
        QString playerName;
        QString roleStartName;
        QString roleEndName;
        QString alignmentEnd; // "good" | "evil"
        bool isAlive                      = false;
        std::optional<uint8_t> seatNumber = 0;
    };

    struct GameRecord
    {
        QDate gameDate;
        QString scenarioName;
        QString location;
        int gameNumber = 0;
        QString storytellerName;
        QString alignmentWin; // "good" | "evil"
        QTime duration;
        QString notes;

        QList<PlayerRecord> players;
    };

    struct GamesParseResult
    {
        QList<GameRecord> records;
        QStringList headers;
        QStringList errors;
        bool success = false;
    };

    class GamesCsvParser
    {
    public:
        static const QStringList VALID_ALIGNMENTS;
        static const QStringList VALID_ALIGNMENT_WINS;

        static GamesParseResult parse(const QString& filePath);

    private:
        static QMap<QString, int> mapHeaders(const QStringList& headers);
        static QStringList validateGame(const GameRecord& in_gameRecord, int in_rowIndex);
        static QStringList validatePlayer(const PlayerRecord& in_player,
                                          int in_rowIndex, int in_playerIndex);
    };
}
