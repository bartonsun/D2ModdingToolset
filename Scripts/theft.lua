--
-- Called before an item can be stolen from a Merchant or a spell can be
-- stolen from a Mage Tower.
--
-- Return value:
--   true  - allow stealing.
--   false - prevent stealing.
--
-- These callbacks can be used to:
--   * forbid stealing specific items or spells;
--   * restrict stealing by player race or lord;
--   * inspect the Merchant or Mage Tower contents;
--   * implement custom stealing rules.
--
--
-- Returning true allows the current item or spell to be stolen.
-- Return false to block stealing.
--

--
-- Example 1:
-- Prevent stealing a specific item.
--
-- if item.id == "g000ig0005" then
--     return false
-- end

--
-- Example 2:
-- Allow stealing only low-level spells.
--
-- if spell.level > 3 then
--     return false
-- end

function theftFilterItemsMerchant(stealItemContext)

    log("")
    log("========================================")
    log("===== THEFT FILTER ITEMS MERCHANT ======")
    log("========================================")

    --
    -- Player
    --

    local player = stealItemContext.player

    log("")
    log("PLAYER")
    log("id        : " .. tostring(player.id))
    log("race      : " .. player.race)
    log("lord      : " .. player.lord)
    log("human     : " .. tostring(player.human))
    log("alwaysAi  : " .. tostring(player.alwaysAi))

    log("bank      : " .. tostring(player.bank))
    log("gold      : " .. player.bank.gold)
    log("infernal  : " .. player.bank.infernalMana)
    log("life      : " .. player.bank.lifeMana)
    log("death     : " .. player.bank.deathMana)
    log("runic     : " .. player.bank.runicMana)
    log("grove     : " .. player.bank.groveMana)

    --
    -- Merchant
    --

    local merchant = stealItemContext.merchant

    log("")
    log("MERCHANT")
    log("id        : " .. tostring(merchant.id))
    log("position  : (" ..
        merchant.position.x .. ", " ..
        merchant.position.y .. ")")

    log("mission   : " .. tostring(merchant.mission))

    log("visitors  : " .. #merchant.visitors)

    for i, visitor in ipairs(merchant.visitors) do
        log(string.format(
            "visitor[%d] : %s",
            i,
            tostring(visitor.id)))
    end

    log("items     : " .. #merchant.items)

    for i, merchantItem in ipairs(merchant.items) do

        local base = merchantItem.base

        log(string.format(
            "item[%d]    : %s x%d",
            i,
            tostring(base.id),
            merchantItem.amount))
    end

    --
    -- Item
    --

    local item = stealItemContext.item

    log("")
    log("ITEM")
    log("id        : " .. tostring(item.id))
    log("type      : " .. item.type)

    log("value     : " .. tostring(item.value))
    log("gold      : " .. item.value.gold)
    log("infernal  : " .. item.value.infernalMana)
    log("life      : " .. item.value.lifeMana)
    log("death     : " .. item.value.deathMana)
    log("runic     : " .. item.value.runicMana)
    log("grove     : " .. item.value.groveMana)

    if item.unitImpl then
        log("unitImpl  : " .. tostring(item.unitImpl.id))
    else
        log("unitImpl  : nil")
    end

    if item.attack then
        log("attack    : " .. tostring(item.attack.id))
    else
        log("attack    : nil")
    end

    log("")
    log("========================================")

    return true
end


function theftFilterMageTower(stealSpellContext)

    log("")
    log("========================================")
    log("======= THEFT FILTER MAGE TOWER ========")
    log("========================================")

    --
    -- Player
    --

    local player = stealSpellContext.player

    log("")
    log("PLAYER")

    log("id        : " .. tostring(player.id))
    log("race      : " .. player.race)
    log("lord      : " .. player.lord)
    log("human     : " .. tostring(player.human))
    log("alwaysAi  : " .. tostring(player.alwaysAi))

    log("bank      : " .. tostring(player.bank))
    log("gold      : " .. player.bank.gold)
    log("infernal  : " .. player.bank.infernalMana)
    log("life      : " .. player.bank.lifeMana)
    log("death     : " .. player.bank.deathMana)
    log("runic     : " .. player.bank.runicMana)
    log("grove     : " .. player.bank.groveMana)

    --
    -- Mage Tower
    --

    local siteMage = stealSpellContext.mage

    log("")
    log("SITE MAGE")

    log("id        : " .. tostring(siteMage.id))
    log("position  : (" ..
        siteMage.position.x .. ", " ..
        siteMage.position.y .. ")")

    log("visitors  : " .. #siteMage.visitors)

    for i, visitor in ipairs(siteMage.visitors) do
        log(string.format(
            "visitor[%d] : %s",
            i,
            tostring(visitor.id)))
    end

    log("spells    : " .. #siteMage.spells)

    for i, spellId in ipairs(siteMage.spells) do
        log(string.format(
            "spell[%d]   : %s",
            i,
            tostring(spellId)))
    end

    --
    -- Spell
    --

    local spell = stealSpellContext.spell

    log("")
    log("SPELL")

    log("id             : " .. tostring(spell.id))
    log("type           : " .. spell.type)
    log("level          : " .. spell.level)

    log("castingCost    : " .. tostring(spell.castingCost))
    log("buyCost        : " .. tostring(spell.buyCost))

    log("cast gold      : " .. spell.castingCost.gold)
    log("cast infernal  : " .. spell.castingCost.infernalMana)
    log("cast life      : " .. spell.castingCost.lifeMana)
    log("cast death     : " .. spell.castingCost.deathMana)
    log("cast runic     : " .. spell.castingCost.runicMana)
    log("cast grove     : " .. spell.castingCost.groveMana)

    log("buy gold       : " .. spell.buyCost.gold)
    log("buy infernal   : " .. spell.buyCost.infernalMana)
    log("buy life       : " .. spell.buyCost.lifeMana)
    log("buy death      : " .. spell.buyCost.deathMana)
    log("buy runic      : " .. spell.buyCost.runicMana)
    log("buy grove      : " .. spell.buyCost.groveMana)

    log("restoreMove    : " .. spell.restoreMove)
    log("area           : " .. spell.area)
    log("damage         : " .. spell.damage)
    log("heal           : " .. spell.heal)
    log("ground         : " .. spell.ground)
    log("changeTerrain  : " .. tostring(spell.changeTerrain))
    log("aiType         : " .. spell.aiType)
    log("damageSource   : " .. spell.damageSource)

    if spell.unit then
        log("unit           : " .. tostring(spell.unit.id))
    else
        log("unit           : nil")
    end

    if spell.modifier then
        log("modifier       : " .. tostring(spell.modifier.id))
    else
        log("modifier       : nil")
    end

    log("wards          : " .. #spell.wards)

    for i, ward in ipairs(spell.wards) do
        log(string.format(
            "ward[%d]       : %s",
            i,
            tostring(ward.id)))
    end

    log("")
    log("========================================")

    return true
end