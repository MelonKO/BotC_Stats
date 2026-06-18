-- Games count by scenario in the selected period
SELECT
    scenario_name,
    COUNT(*) AS record_count
FROM games
WHERE game_date BETWEEN {{start_date}} AND {{end_date}}
GROUP BY scenario_name
ORDER BY scenario_name
