--[[
  Called from movepathhooks.cpp when showMovementAfterAction is true.

  Context fields:
    stack, maxMovement, currentMovement, spentMovement,
    remainingMovement, afterActionMovement, targetId (optional)

  Return: int — movement points remaining after the action.
]]--

function movementAfterAction(ctx)
	local remaining = ctx.remainingMovement or 0
	local after = ctx.afterActionMovement
	if after == nil then
		after = remaining
	end

	local targetId = ctx.targetId
	if targetId == nil then
		return after
	end

	local scenario = getScenario()
	if scenario == nil then
		return after
	end

	local ruin = scenario:getRuin(targetId)
	if ruin == nil then
		return after
	end

	if ruin.looter ~= nil then
		return remaining
	end

	return after
end
