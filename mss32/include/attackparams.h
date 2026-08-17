#ifndef ATTACKHITPARAMSVIEW_H
#define ATTACKHITPARAMSVIEW_H

#include "attackview.h"
#include "unitview.h"
#include <sol/sol.hpp>
#include <map>
#include <battlemsgdata.h>

namespace sol {
    class state;
}

namespace game {
    struct CMidUnit;
    struct IMidgardObjectMap;
    struct BattleMsgData;
    struct IAttack;
} // namespace game

namespace bindings {

    class UnitView;
    class AttackView;

    class AttackHitParamsView
    {
        public:
            AttackHitParamsView() = default;

            static void bind(sol::state& lua);

            void setDamage(sol::object dmg);
            void setCritDamage(sol::object dmg);
            void setDrain(sol::object dmg);
            void setDrainHealPercent(sol::object dmg);
            void setHeal(sol::object dmg);
            void setAttackLevel(sol::object dmg);
            void setAttackCount(sol::object value);
            void setFastRetreat(sol::object value);
            void setIsRevive(sol::object value);
            void setIsTemp(sol::object value);
            void setMiss(sol::object value);
            void setIsLong(sol::object value);

            sol::table getModifiers(sol::this_state L);
            void addModifier(const std::string& modifierId);
            void removeModifier(const std::string& modifierId);

            game::CMidUnit* attacker = nullptr;
            game::CMidUnit* target = nullptr;
            game::IAttack* attack = nullptr;
            std::string attackClass = "none";

            // Damage
            int damage = 0;
            int critDamage = 0;
            int drain = 0;
            int drainHealPercent = 0;

            // Heal
            int heal = 0;

            // Boost/Lower
            int attackLevel = 0;

            //GiveAttack
            int attackCount = 0;

            // BestowWard
            std::vector<const game::CMidgardID*> modifierIds;

            // Retreat
            bool fastRetreat = false;

            // Potion
            bool isRevive = false;
            bool isTemp = false;
            game::CMidgardID* itemId = nullptr;

            // Other
            int power = -1;
            bool isLong = false;
            bool miss = false;
            bool blockAttack = false;

            UnitView getAttacker() const;
            UnitView getTarget() const;
            AttackView getAttack() const;
            IdView getItemId() const;
            std::string getAttackClass() const;
            void setTarget(sol::object value);
    };

} // namespace bindings

#endif // ATTACKHITPARAMSVIEW_H
