function getLevel(summoner, summonImpl, item, battle)
	return math.max(summoner.impl.level, summonImpl.level);
end

--[[
  This script is called when a summon attack is used. It receives a list of
  possible unit IDs that can be summoned (based on the attack and whether a big
  unit can be summoned). The script can filter, reorder, or replace the list.
  The final summoned unit is randomly chosen from the returned list.

  Parameters:
    battle      		: BattleView object (contains battle state, round, etc.)
    summoner    		: UnitView of the summoner (the unit performing the summon)
    possibleIdList    	: table of Id objects – the default list of summonable units
    slot        		: UnitSlotView of the target slot (contains unit, position, groupId) If the target slot is empty, slotView.unit may be nil.
    canBig    			: boolean – true if the attack can summon a big unit
    item    			: ItemView - item that triggered the transformation or nil

  Returns:
    A table of Id objects representing the modified list of summonable units.
    If the returned table is empty or nil, the original default unit will be used.
--]]
function getSummonList(battle, summoner, possibleIdList, slot, canBig, item)
	return possibleIdList
end