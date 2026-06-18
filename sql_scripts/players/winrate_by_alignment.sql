-- Win rate by alignment for players who played more than 1/3 of games in the period
WITH
period_games AS (
    SELECT COUNT(*) AS total
    FROM games
    WHERE game_date BETWEEN {{start_date}} AND {{end_date}}
),
eligible_players AS (
    SELECT gp.player_id
    FROM game_players gp
    JOIN games g ON g.id = gp.game_id
    WHERE g.game_date BETWEEN {{start_date}} AND {{end_date}}
    GROUP BY gp.player_id
    HAVING COUNT(*) > (SELECT total FROM period_games) / 3.0
)
SELECT
    p.name,
    COUNT(*)                                                                    AS games_played,
    COUNT(*) FILTER (WHERE gp.alignment_end = 'good')                          AS games_as_good,
    COUNT(*) FILTER (WHERE gp.alignment_end = 'evil')                          AS games_as_evil,
    COUNT(*) FILTER (WHERE gp.alignment_end = g.alignment_win)                 AS games_won,
    ROUND(
        COUNT(*) FILTER (WHERE gp.alignment_end = 'good' AND g.alignment_win = 'good')::numeric
        / NULLIF(COUNT(*) FILTER (WHERE gp.alignment_end = 'good'), 0) * 100,
        2
    )                                                                           AS win_rate_good,
    ROUND(
        COUNT(*) FILTER (WHERE gp.alignment_end = 'evil' AND g.alignment_win = 'evil')::numeric
        / NULLIF(COUNT(*) FILTER (WHERE gp.alignment_end = 'evil'), 0) * 100,
        2
    )                                                                           AS win_rate_evil
FROM game_players gp
JOIN games g   ON g.id = gp.game_id
JOIN players p ON p.id = gp.player_id
JOIN eligible_players ep ON ep.player_id = p.id
WHERE g.game_date BETWEEN {{start_date}} AND {{end_date}}
GROUP BY p.id, p.name
ORDER BY win_rate_good DESC NULLS LAST
