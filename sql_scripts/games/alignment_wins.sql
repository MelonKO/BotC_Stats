-- Wins by alignment in the selected period
SELECT
    alignment_win,
    COUNT(*) AS wins
FROM games
WHERE game_date BETWEEN {{start_date}} AND {{end_date}}
GROUP BY alignment_win
