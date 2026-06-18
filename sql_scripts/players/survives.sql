select
	name,
	games_played,
	survived,
	ROUND(survived::numeric / NULLIF(games_played, 0) * 100, 2) as survive_rate
from v_player_stats
where games_played > 5
order by survive_rate DESC
