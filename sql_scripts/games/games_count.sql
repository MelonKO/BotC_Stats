-- Total number of games in the selected period
SELECT COUNT(*) AS games_count
FROM games
WHERE game_date BETWEEN {{start_date}} AND {{end_date}}
