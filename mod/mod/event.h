#pragma once

#include "../utils/lua.h"

namespace xyu
{
    /* 动画 */
    void event_quick_move(LuaState* L, bool on);
    void event_remove_bbanimation(LuaState* L, bool on);

    /* 资源 */
    void event_uncensor(LuaState* L, bool on);
    void event_unlock_attire(LuaState* L, bool on);
    void event_unlock_ship_skin(LuaState* L, bool on);
    void event_unlock_cryptolalia(LuaState* L, bool on);

    /* 破限 */
    void event_remove_mood_warning(LuaState* L, bool on);
    void event_remove_mood_full(LuaState* L, bool on);
    void event_remove_hardmode_fleet_limit(LuaState* L, bool on);
    void event_set_autotimes_limit(LuaState* L, int newlimit);
    void event_expand_battle_bounds(LuaState* L, bool on);

    /* 战斗逻辑 */
    void event_summon_all_waves(LuaState* L, bool on);
    void event_remove_continuous_limit(LuaState* L, bool on);
    void event_uncontinuous_manual(LuaState* L, bool on);
    void event_autobot_all_clear(LuaState* L, bool on);
    void event_autobot_high_priority(LuaState* L, bool on);

    /* 大世界 */
    void event_remove_world_fog(LuaState* L, bool on);
    void event_remove_world_auto_limit(LuaState* L, bool on);
    void event_autobuy_energy_box(LuaState* L, bool on);
    void event_autouse_energy_box(LuaState* L, bool on);
    void event_autouse_siren_facilities(LuaState* L, bool on);

    /* 战斗数值 */
    void event_exercise_no_scale(LuaState* L, bool on);
    void event_set_attack_scale(LuaState* L, double newack);
    void event_set_attack_limit(LuaState* L, bool on);
    void event_set_defence_scale(LuaState* L, double newdef);
    void event_set_coldown_rate(LuaState* L, double rate);
    void event_remove_global_cooldown(LuaState* L, bool on);
    void event_hive_cooldown_limit(LuaState* L, bool on);
    void event_set_range_scale(LuaState* L, double range);

    /* 小游戏 */
    void event_gameroom_shooting_game(LuaState* L, bool on);

    /* 其他 */
    void load_player_info(LuaState* L);
    void execute_self_command(LuaState* L, const std::string& command);
    void event_replace_chat_box(LuaState* L, bool on);

}
