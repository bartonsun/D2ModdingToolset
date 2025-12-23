package.path = ".\\Scripts\\?.lua;.\\Scripts\\battleAI\\?.lua;.\\Scripts\\battleAI\\aIBattleLogic\\?.lua;.\\Scripts\\battleAI\\aIBattleStats\\?.lua;.\\Scripts\\battleAI\\unitRaiting\\?.lua;.\\Scripts\\exp\\?.lua;.\\Scripts\\modifiers\\?.lua;.\\Scripts\\modifiers\\drawing\\?.lua;.\\Scripts\\modifiers\\items\\?.lua;.\\Scripts\\modifiers\\leaderMods\\?.lua;.\\Scripts\\modifiers\\perks\\?.lua;.\\Scripts\\modifiers\\smns\\?.lua;.\\Scripts\\modifiers\\smns\\items\\?.lua;.\\Scripts\\modifiers\\smns\\perks\\?.lua;.\\Scripts\\modifiers\\smns\\spells\\?.lua;.\\Scripts\\modifiers\\smns\\units\\?.lua;.\\Scripts\\modifiers\\spells\\?.lua;.\\Scripts\\modifiers\\units\\?.lua;.\\Scripts\\modifiers\\units\\bloodsorcerer\\?.lua;.\\Scripts\\modifiers\\units\\multiplicative_stats\\?.lua;.\\Scripts\\modifiers\\units\\torhoth\\?.lua;.\\Scripts\\modules\\?.lua;.\\Scripts\\modules\\smns\\?.lua;.\\Scripts\\unitEncyclopediaTemplate\\?.lua;.\\Scripts\\workshop\\?.lua;.\\Scripts\\workshop\\classes\\?.lua;.\\Templates\\custom_modifier_?.lua"

function OnAddModifier( unit, modifier )
	return true
end

function ModifierApplyed( unit, modifier )
end

function OnRemoveModifier(unit, modifier)
	return true
end

