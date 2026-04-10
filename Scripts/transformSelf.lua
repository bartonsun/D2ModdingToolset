function getLevel(unit, transformImpl, item, battle)
	-- transform using current level
	return math.max(unit.impl.level, transformImpl.level);
end

-- Used by freeTransformSelfAttack setting to determine a number of free attacks to give
function getFreeAttackNumber(attacksDone, attacksRemain, hadDoubleAttack, hasDoubleAttack)
	-- Uncomment the following lines to deny free transformation on second attack
	--if attacksDone > 0 then
	--	return 0

	if not hadDoubleAttack and hasDoubleAttack and attacksDone == 0 then
		-- Give 2 extra attacks if transforming from single to double attack.
		-- attacksDone prevents infinite abuse of 2 extra attacks:
		-- do normal attack -> transform to single-attack -> transform back to double-attack
		-- -> do normal attack -> ...
		return 2
	elseif hadDoubleAttack and not hasDoubleAttack and attacksRemain > 0 then
		-- Give nothing if transforming from double to single attack with attacks remain
		return 0
	-- Uncomment the following lines to deny free transformation on second attack if transforming to single
	--elseif hadDoubleAttack and not hasDoubleAttack and attacksDone > 0 then
	--	return 0
	else
		-- Give 1 extra attack to compensate transformation
		return 1
	end
end

--[[
  transformOther.lua – Customize unit transformations caused by “transform other” attacks.

  This script is called when a transform‑other attack is used. It receives a list of
  possible unit IDs that the target can be transformed into (based on the attack and
  target size). The script can filter, reorder, or replace the list. The final
  transformation is randomly chosen from the returned list.

  Parameters:
    battle      		: BattleView object (contains battle state, round, etc.)
    attacker			: UnitView of the attacking unit
    target				: UnitView of the target unit (the one being transformed)
    possibleIdList		: table of Id objects – the default list of transformable units
    slot				: UnitSlotView of the target slot (contains unit, position, groupId)
    item    			: ItemView - item that triggered the transformation or nil

  Returns:
    A table of Id objects representing the modified list of transformable units.
    If the returned table is empty or nil, the original default unit will be used.
--]]
function getTransformSelfList(battle, attacker, target, possibleIdList, slot, item)
	return possibleIdList
end