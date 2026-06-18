-- Demon statistics for players who played more than 1/3 of games in the period
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
    COUNT(*)                                                                                    AS games_played,
    COUNT(*) FILTER (WHERE r_start.role_type = 'Demon')                                        AS games_started_as_demon,
    COUNT(*) FILTER (WHERE r_end.role_type = 'Demon')                                          AS games_ended_as_demon,
    COUNT(*) FILTER (WHERE r_end.role_type = 'Demon' AND gp.alignment_end = g.alignment_win)   AS games_won_as_demon,
    ROUND(
        COUNT(*) FILTER (WHERE r_end.role_type = 'Demon' AND gp.alignment_end = g.alignment_win)::numeric
        / NULLIF(COUNT(*) FILTER (WHERE r_end.role_type = 'Demon'), 0) * 100,
        2
    )                                                                                           AS win_rate_as_demon
FROM game_players gp
JOIN games g        ON g.id = gp.game_id
JOIN players p      ON p.id = gp.player_id
JOIN roles r_start  ON r_start.id = gp.role_start_id
JOIN roles r_end    ON r_end.id = gp.role_end_id
JOIN eligible_players ep ON ep.player_id = p.id
WHERE g.game_date BETWEEN {{start_date}} AND {{end_date}}
  AND (r_start.role_type = 'Demon' OR r_end.role_type = 'Demon')
GROUP BY p.id, p.name
ORDER BY games_played DESC
