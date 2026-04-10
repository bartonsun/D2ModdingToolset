function getLevel(attacker, target, transformImpl, item, battle)
    -- transform using target level with a minimum of transform impl level
    return math.max(target.impl.level, transformImpl.level);
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
function getTransformOtherList(battle, attacker, target, possibleIdList, slot, item)
	return possibleIdList
end