#include "GamesCsvParser.h"

#include <QFile>
#include <QMap>
#include <QTextStream>

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

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            result.errors << QString("Не удалось открыть файл: %1").arg(filePath);
            return result;
        }

        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::Utf8);

        if (stream.atEnd())
        {
            result.errors << "Файл пустой.";
            return result;
        }

        // Заголовок
        const QStringList headers    = parseCsvLine(stream.readLine());
        result.headers               = headers;
        const QMap<QString, int> col = mapHeaders(headers);

        // Проверяем обязательные колонки
        const QStringList required = {
            "game_date", "scenario_name", "location", "game_number",
            "storyteller_name", "alignment_win", "player_name",
            "role_start_name", "role_end_name", "alignment_end",
            "is_alive", "seat_number"
        };
        for (const QString& c : required)
        {
            if (!col.contains(c))
                result.errors << QString("Отсутствует обязательная колонка: '%1'").arg(c);
        }
        if (!result.errors.isEmpty()) return result;

        // CSV может содержать несколько строк на одну партию (по одной на каждого игрока).
        // Группируем строки по ключу: game_date + game_number + storyteller_name
        struct RawRow
        {
            QStringList cells;
            int lineNumber;
        };
        QMap<QString, QList<RawRow>> groups;
        QList<QString> groupOrder; // сохраняем порядок появления партий

        int lineNumber = 2;
        while (!stream.atEnd())
        {
            const QString line = stream.readLine();
            if (line.trimmed().isEmpty())
            {
                ++lineNumber;
                continue;
            }

            QStringList cells = parseCsvLine(line);

            auto cell = [&](const QString& name) -> QString
            {
                int idx = col.value(name, -1);
                if (idx < 0 || idx >= cells.size()) return {};
                return cells[idx].trimmed();
            };

            // Ключ группировки
            const QString key = cell("game_date") + "|"
                + cell("game_number") + "|"
                + cell("storyteller_name");

            if (!groups.contains(key)) groupOrder << key;
            groups[key].append({cells, lineNumber});
            ++lineNumber;
        }

        // Собираем PartyRecord из каждой группы
        for (const QString& key : groupOrder)
        {
            const QList<RawRow>& rows     = groups[key];
            const QStringList& firstCells = rows.first().cells;
            const int firstLine           = rows.first().lineNumber;

            auto cell = [&](const QString& name) -> QString
            {
                int idx = col.value(name, -1);
                if (idx < 0 || idx >= firstCells.size()) return {};
                return firstCells[idx].trimmed();
            };

            GameRecord game_record;
            game_record.gameDate        = QDate::fromString(cell("game_date"), Qt::ISODate);
            game_record.scenarioName    = cell("scenario_name");
            game_record.location        = cell("location");
            game_record.gameNumber      = cell("game_number").toInt();
            game_record.storytellerName = cell("storyteller_name");
            game_record.alignmentWin    = cell("alignment_win").toLower();
            game_record.duration        = QTime::fromString(cell("duration"), "hh:mm:ss");
            game_record.notes           = cell("notes");

            // Валидация полей партии
            result.errors << validateParty(game_record, firstLine);

            // Игроки — по одному из каждой строки группы
            int playerIdx = 1;
            for (const RawRow& row : rows)
            {
                const QStringList& c = row.cells;
                auto rcell           = [&](const QString& name) -> QString
                {
                    int idx = col.value(name, -1);
                    if (idx < 0 || idx >= c.size()) return {};
                    return c[idx].trimmed();
                };

                PlayerRecord player;
                player.playerName    = rcell("player_name");
                player.roleStartName = rcell("role_start_name");
                player.roleEndName   = rcell("role_end_name");
                player.alignmentEnd  = rcell("alignment_end").toLower();
                player.isAlive       = (rcell("is_alive").toLower() == "true"
                    || rcell("is_alive") == "1");
                player.seatNumber = rcell("seat_number").toInt();

                result.errors << validatePlayer(player, row.lineNumber, playerIdx);
                game_record.players << player;
                ++playerIdx;
            }

            result.records << game_record;
        }

        result.success = result.errors.isEmpty();
        return result;
    }

    // ─── Вспомогательные ─────────────────────────────────────────

    QStringList GamesCsvParser::parseCsvLine(const QString& line)
    {
        QStringList fields;
        QString current;
        bool inQuotes = false;

        for (int i = 0; i < line.size(); ++i)
        {
            QChar c = line[i];
            if (inQuotes)
            {
                if (c == '"')
                {
                    if (i + 1 < line.size() && line[i + 1] == '"')
                    {
                        current += '"';
                        ++i;
                    }
                    else
                    {
                        inQuotes = false;
                    }
                }
                else
                {
                    current += c;
                }
            }
            else
            {
                if (c == '"') inQuotes = true;
                else if (c == ',')
                {
                    fields << current;
                    current.clear();
                }
                else current += c;
            }
        }
        fields << current;
        return fields;
    }

    QMap<QString, int> GamesCsvParser::mapHeaders(const QStringList& headers)
    {
        QMap<QString, int> index;
        for (int i = 0; i < headers.size(); ++i)
            index[headers[i].trimmed().toLower()] = i;
        return index;
    }

    QStringList GamesCsvParser::validateParty(const GameRecord& p, int row)
    {
        QStringList errors;
        auto err = [&](const QString& msg)
        {
            errors << QString("Строка %1: %2").arg(row).arg(msg);
        };

        if (!p.gameDate.isValid())
            err("некорректный формат game_date (ожидается YYYY-MM-DD).");
        if (p.scenarioName.isEmpty())
            err("поле 'scenario_name' не может быть пустым.");
        if (p.location.isEmpty())
            err("поле 'location' не может быть пустым.");
        if (p.gameNumber <= 0)
            err("поле 'game_number' должно быть положительным числом.");
        if (p.storytellerName.isEmpty())
            err("поле 'storyteller_name' не может быть пустым.");
        if (!VALID_ALIGNMENT_WINS.contains(p.alignmentWin))
            err(QString("недопустимое значение alignment_win='%1'. Допустимые: %2")
                .arg(p.alignmentWin, VALID_ALIGNMENT_WINS.join(", ")));

        return errors;
    }

    QStringList GamesCsvParser::validatePlayer(const PlayerRecord& p,
                                               int row, int playerIdx)
    {
        QStringList errors;
        auto err = [&](const QString& msg)
        {
            errors << QString("Строка %1 (игрок %2): %3").arg(row).arg(playerIdx).arg(msg);
        };

        if (p.playerName.isEmpty())
            err("поле 'player_name' не может быть пустым.");
        if (p.roleStartName.isEmpty())
            err("поле 'role_start_name' не может быть пустым.");
        if (p.roleEndName.isEmpty())
            err("поле 'role_end_name' не может быть пустым.");
        if (!VALID_ALIGNMENTS.contains(p.alignmentEnd))
            err(QString("недопустимое значение alignment_end='%1'. Допустимые: %2")
                .arg(p.alignmentEnd, VALID_ALIGNMENTS.join(", ")));
        if (p.seatNumber <= 0)
            err("поле 'seat_number' должно быть положительным числом.");

        return errors;
    }
}
