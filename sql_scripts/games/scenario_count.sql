-- games count by scenario
select
    scenario_name,
    count(*) as record_count
from games
group by scenario_name
order by scenario_name