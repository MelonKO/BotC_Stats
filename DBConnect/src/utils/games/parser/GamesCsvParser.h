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

        GamesParseResult parse(const QString& filePath);

    private:
        QStringList parseCsvLine(const QString& line);
        QMap<QString, int> mapHeaders(const QStringList& headers);
        QStringList validateParty(const GameRecord& party, int rowIndex);
        QStringList validatePlayer(const PlayerRecord& player,
                                   int rowIndex, int playerIndex);
    };
}
