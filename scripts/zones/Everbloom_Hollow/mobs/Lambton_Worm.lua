-----------------------------------
-- Area: BCNM
--  MOB: Lambton Worm
-----------------------------------

require("scripts/globals/titles");

-----------------------------------
-- onMobSpawn Action
-----------------------------------

function onMobSpawn(mob)
end;

-----------------------------------
-- onMobDeath
-----------------------------------

function onMobDeath(mob, player, isKiller)
    player:addTitle(LAMBTON_WORM_DESEGMENTER);
end;