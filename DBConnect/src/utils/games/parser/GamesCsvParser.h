#pragma once

#include <QDate>
#include <QList>
#include <QString>
#include <expected>

namespace botc::utils::games
{
    struct PlayerRecord
    {
        QString playerName;
        QString roleStartName;
        QString roleEndName;
        QString alignmentEnd; // "добро" | "зло"
        bool isAlive                      = false;
        std::optional<uint8_t> seatNumber = 0;
    };

    struct GameRecord
    {
        QDate gameDate;
        QString scenarioName;
        QString location;
        int gameNumber = 0;
        QStringList storytellerNames;
        QString alignmentWin; // "добро" | "зло" | "ничья"
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
        static QDate parseDate(const QString& in_date);
        static std::expected<bool, QString> parseAliveField(const QString& in_field);
    };
}
