select 
	alignment_win,
	count (*) as wins
from games
group by alignment_win