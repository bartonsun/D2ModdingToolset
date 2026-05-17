--[[
	Call once on turn 0 on host-side
	Allow prepare map before game start
--]]
function processTurnZero()

end


--[[
    Called at the start of each player turn, including neutrals
    Executed on host-side
    Allows running custom per-turn logic
--]]
function processTurnStart(player)
 -- player id log(tostring(player.id))
 -- race category id log(tostring(player.race))
 -- any custom logic log("Custom turn logic executed")

 end