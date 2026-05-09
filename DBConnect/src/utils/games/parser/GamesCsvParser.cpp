#include "GamesCsvParser.h"

#include <QMap>

#include "../../csv/CsvParser.h"

namespace botc::utils::games
{
    const QStringList GamesCsvParser::VALID_ALIGNMENTS = {
        "добро", "зло"
    };

    const QStringList GamesCsvParser::VALID_ALIGNMENT_WINS = {
        "добро", "зло"
    };

    // ─── parse ───────────────────────────────────────────────────

    GamesParseResult GamesCsvParser::parse(const QString& filePath)
    {
        GamesParseResult result;

        CsvParser parser(filePath);
        if (auto [bSuccess, errors] = parser.openFile(); !bSuccess)
        {
            result.errors = errors;
            return result;
        }

        // Header
        const QStringList headers                 = parser.parseNextLine();
        result.headers                            = headers;
        const QMap<QString, int> headerToIndexMap = mapHeaders(headers);

        auto createCellReader = [&headerToIndexMap](QStringList in_row) -> auto
        {
            return [&headerToIndexMap, &in_row](const QString& col) -> QString
            {
                const int idx = headerToIndexMap.value(col, -1);
                if (idx < 0 || idx >= in_row.size()) return {};
                return in_row[idx].trimmed();
            };
        };

        // Check required columns
        const QStringList required = {
            "game_date", "scenario_name", "location", "game_number",
            "storyteller_name", "alignment_win", "player_name",
            "role_start_name", "role_end_name", "alignment_end",
            "is_alive"
        };
        for (const QString& c : required)
        {
            if (!headerToIndexMap.contains(c))
                result.errors << QString("Отсутствует обязательная колонка: '%1'").arg(c);
        }
        if (!result.errors.isEmpty()) return result;

        // CSV can contain multiple rows per game (one for each player).
        // Group the rows by key:
        // game_date + scenario_name + storyteller_name + alignment_win + location + game_number.
        struct RawRow
        {
            QStringList cells;
            int lineNumber;
        };
        QMap<QString, QList<RawRow>> groups;
        QList<QString> groupOrder; // Keep the order in which the games appear.

        int lineNumber = 2;
        while (parser.isCanReadNext())
        {
            QStringList row = parser.parseNextLine();
            if (row.isEmpty())
            {
                ++lineNumber;
                continue;
            }

            auto readRow = createCellReader(row);

            // The grouping key
            // game_date + scenario_name + storyteller_name + alignment_win + location + game_number
            const QString key = readRow("game_date") + "|"
                + readRow("scenario_name") + "|"
                + readRow("storyteller_name") + "|"
                + readRow("alignment_win") + "|"
                + readRow("location") + "|"
                + readRow("game_number");

            if (!groups.contains(key)) groupOrder << key;
            groups[key].append({row, lineNumber});
            ++lineNumber;
        }

        // Collect a GameRecord from each group
        for (const QString& key : groupOrder)
        {
            const QList<RawRow>& rows    = groups[key];
            const QStringList& firstRow  = rows.first().cells;
            const int firstRowLineNumber = rows.first().lineNumber;

            auto readFirstRow = createCellReader(firstRow);

            GameRecord game_record;
            game_record.gameDate        = parseDate(readFirstRow("game_date"));
            game_record.scenarioName    = readFirstRow("scenario_name");
            game_record.location        = readFirstRow("location");
            game_record.gameNumber      = readFirstRow("game_number").toInt();
            game_record.storytellerName = readFirstRow("storyteller_name");
            game_record.alignmentWin    = readFirstRow("alignment_win").toLower();
            game_record.duration        = QTime::fromString(readFirstRow("duration"), "hh:mm:ss");
            game_record.notes           = readFirstRow("notes");

            // Validation of game fields
            result.errors << validateGame(game_record, firstRowLineNumber);

            // Players — one from each row of the group
            int playerIdx = 1;
            for (const RawRow& row : rows)
            {
                auto readPlayerRow = createCellReader(row.cells);

                PlayerRecord player;
                player.playerName    = readPlayerRow("player_name");
                player.roleStartName = readPlayerRow("role_start_name");
                player.roleEndName   = readPlayerRow("role_end_name");
                player.alignmentEnd  = readPlayerRow("alignment_end").toLower();
                if (std::expected<bool, QString> parsedIsAlive = parseAliveField(readPlayerRow("is_alive"));
                    parsedIsAlive.has_value())
                {
                    player.isAlive = parsedIsAlive.value();
                }
                else
                {
                    result.errors <<
                        QString("Line %1 (player %2): %3")
                        .arg(row.lineNumber)
                        .arg(playerIdx).
                        arg(parsedIsAlive.error());
                }
                player.seatNumber = readPlayerRow("seat_number").toInt();

                result.errors << validatePlayer(player, row.lineNumber, playerIdx);
                game_record.players << player;
                ++playerIdx;
            }

            result.records << game_record;
        }

        result.success = result.errors.isEmpty();
        return result;
    }

    // ─── Helpers ─────────────────────────────────────────

    QMap<QString, int> GamesCsvParser::mapHeaders(const QStringList& headers)
    {
        QMap<QString, int> index;
        for (int i = 0; i < headers.size(); ++i)
            index[headers[i].trimmed().toLower()] = i;
        return index;
    }

    QStringList GamesCsvParser::validateGame(const GameRecord& in_gameRecord, const int in_rowIndex)
    {
        QStringList errors;
        auto err = [&](const QString& msg)
        {
            errors << QString("Line %1: %2").arg(in_rowIndex).arg(msg);
        };

        if (!in_gameRecord.gameDate.isValid())
            err("Incorrect format of game_date (expected YYYY-MM-DD).");
        if (in_gameRecord.scenarioName.isEmpty())
            err("The \"scenario_name\" field cannot be empty.");
        if (in_gameRecord.location.isEmpty())
            err("The \"location\" field cannot be empty.");
        if (in_gameRecord.gameNumber <= 0)
            err("The 'game_number' field must be a positive number.");
        if (in_gameRecord.storytellerName.isEmpty())
            err("The 'storyteller_name' field cannot be empty.");
        if (!VALID_ALIGNMENT_WINS.contains(in_gameRecord.alignmentWin))
            err(QString("Invalid value for the \"alignment_win\" field='%1'. Valid values: %2")
                .arg(in_gameRecord.alignmentWin, VALID_ALIGNMENT_WINS.join(", ")));

        return errors;
    }

    QStringList GamesCsvParser::validatePlayer(const PlayerRecord& in_player,
                                               const int in_rowIndex, const int in_playerIndex)
    {
        QStringList errors;
        auto err = [&](const QString& msg)
        {
            errors << QString("Line %1 (player %2): %3").arg(in_rowIndex).arg(in_playerIndex).arg(msg);
        };

        if (in_player.playerName.isEmpty())
            err("The \"player_name\" field cannot be empty.");
        if (in_player.roleStartName.isEmpty())
            err("The \"role_start_name\" field cannot be empty.");
        if (in_player.roleEndName.isEmpty())
            err("The \"role_end_name\" field cannot be empty.");
        if (!VALID_ALIGNMENTS.contains(in_player.alignmentEnd))
            err(QString("Invalid value for the \"alignment_end\" field='%1'. Valid values: %2")
                .arg(in_player.alignmentEnd, VALID_ALIGNMENTS.join(", ")));

        return errors;
    }

    QDate GamesCsvParser::parseDate(const QString& in_date)
    {
        QDate date = QDate::fromString(in_date, Qt::ISODate);
        if (date.isValid()) { return date; }
        date = QDate::fromString(in_date, "dd.MM.yyyy");
        if (date.isValid()) { return date; }
        return {};
    }

    std::expected<bool, QString> GamesCsvParser::parseAliveField(const QString& in_field)
    {
        // TODO:: remove russian hardcode
        const QString val = in_field.trimmed().toLower();

        static const QSet<QString> trueValues = {
            // russian
            "жив", "да",
            // english
            "alive", "yes", "true",
            // numbers
            "1"
        };

        static const QSet<QString> falseValues = {
            // russian
            "мертв", "мёртв", "нет",
            // english
            "dead", "no", "false",
            // numbers
            "0"
        };

        if (trueValues.contains(val)) return true;
        if (falseValues.contains(val)) return false;

        return std::unexpected(QString("unexpected %1 value of \"is_alive\" field").arg(in_field));
    }
}
