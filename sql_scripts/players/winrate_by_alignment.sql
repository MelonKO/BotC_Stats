SELECT
    p.name,
    COUNT(*)                                                        AS games_played,
    COUNT(*) FILTER (WHERE gp.alignment_end = 'good')              AS games_as_good,
    COUNT(*) FILTER (WHERE gp.alignment_end = 'evil')              AS games_as_evil,
    COUNT(*) FILTER (WHERE gp.alignment_end = g.alignment_win)     AS games_won,
    ROUND(
        COUNT(*) FILTER (WHERE gp.alignment_end = 'good' AND g.alignment_win = 'good')::numeric
        / NULLIF(COUNT(*) FILTER (WHERE gp.alignment_end = 'good'), 0) * 100,
        2
    )                                                               AS win_rate_good,
    ROUND(
        COUNT(*) FILTER (WHERE gp.alignment_end = 'evil' AND g.alignment_win = 'evil')::numeric
        / NULLIF(COUNT(*) FILTER (WHERE gp.alignment_end = 'evil'), 0) * 100,
        2
    )                                                               AS win_rate_evil
FROM game_players gp
JOIN games g ON g.id = gp.game_id
JOIN players p ON p.id = gp.player_id
GROUP BY p.id, p.name
HAVING COUNT(*) >= 5
ORDER BY win_rate_good DESC NULLS LAST;