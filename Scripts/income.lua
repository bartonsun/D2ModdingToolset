--[[
  Args:
	player			: playerView
	prev			: currencyView
	interfaceCall	: bool - true if called from GUI, false if called on turn start or ai turn

  Returns:
    currencyView
    If the returned value is empty or nil, the original default unit will be used.
]]--
function getTurnIncome(player, prev, interfaceCall)
	return prev
end
