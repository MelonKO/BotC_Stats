select
	name,
	games_played,
	games_as_good,
	games_as_evil,
	ROUND(games_as_good::numeric / NULLIF(games_played, 0) * 100, 2) as rate_good,
	ROUND(games_as_evil::numeric / NULLIF(games_played, 0) * 100, 2) as rate_evil
from v_player_stats
where games_played > 5
order by rate_evil DESC
