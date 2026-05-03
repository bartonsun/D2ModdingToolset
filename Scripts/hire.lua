--[[
  Returns:
	player			: playerView
	currentLeader	: unitView - current stack leader in capital

  Returns:
    New leader CMidgardId.
    If the returned value is empty or nil, the original default unit will be used.
]]--
function getStartingLeader(player, currentLeader)
    return currentLeader
end

--[[
  Returns:
	hireList    	: table of Id objects – the default list of leaders for hire
	player			: playerView

  Returns:
    A table of Id objects representing the modified list of leaders for hire.
    If the returned table is empty or nil, the original default list will be used.
]]--
function getLeadersHireList(hireList, player)
    return hireList
end

--[[
  Returns:
	hireList    	: table of Id objects – the default list of units for hire
	player			: playerView

  Returns:
    A table of Id objects representing the modified list of units for hire.
    If the returned table is empty or nil, the original default list will be used.
]]--
function getUnitsHireList(hireList, player)
    return hireList
end