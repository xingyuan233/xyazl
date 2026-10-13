#include "../switch.h"

#include <array>

#include "etools.hpp"
#include "../event.h"
#include "../local.h"
#include "../../utils/log.h"
#include "../../utils/obfstr.h"
#include "../../utils/hashtable.h"

using namespace xyu;
using Switch = SwitchGroup::Switch;

constexpr int start_id = 233;
SwitchGroup switch_group;
static std::vector<Switch> switches;
static HashTable<std::string, int> switches_hash;

void SwitchGroup::init()
{
    auto add_switch = [](const char* name, auto&& callback, int type)
    {
        int index = switches.size();
        switches.emplace_back(Switch{start_id + index, false, name, [index, callback](LuaState* L, bool on)
        {
            callback(L, on);
            switches[index].on = on;
            write_mod();
        }, type});
        switches_hash.get(name) = index;
    };

    static int value_start;
    static auto flush = [](LuaState* L, int index)
    {
        L->push(L->get_global(xyobfstr("SettingsOtherPanel.UpdateItem")));
        L->push_copy(1);
        L->push(index - value_start + 1);
        L->push(switches[index].data);
        L->call(3, 0);
    };
    auto make_gen = [](int index_start, auto&& values, auto* fn)
    {
        struct Count
        {
        private:
            int* n;
        public:
            Count() : n(new int{0}) { inc(); }
            Count(const Count& c) : n(c.n) { inc(); }
            ~Count() noexcept { dec(); }
            int get() const noexcept { return *n & 0xffff; }
            void add(int v = 1) { *n += v; }
            void sub(int v = 1) { *n -= v; }
        private:
            void inc() { *n += 0x10000; }
            void dec() { if ((*n -= 0x10000) >> 16 == 0) delete n; }
        }
        count;

        return [index_start, count, values, fn]() mutable
        {
            int offset = count.get();
            count.add();
            return [index_start, count, offset, values, fn](LuaState* L, bool on)
            {
                if (on)
                {
                    for (int i = 0; i < count.get(); ++i)
                        if (i != offset && switches[i + index_start].on)
                        {
                            switches[i + index_start].on = false;
                            if (is_mod_loaded()) flush(L, i + index_start);
                        }
                    fn(L, values[offset + 1]);
                }
                else fn(L, values[0]);
            };
        };
    };

    add_switch(xyobfcstr("反和谐"), event_uncensor, 1);
    add_switch(xyobfcstr("解锁装扮"), event_unlock_attire, 1);
    add_switch(xyobfcstr("解锁舰船皮肤"), event_unlock_ship_skin, 1);
    // @todo 解锁宿舍皮肤
    add_switch(xyobfcstr("解锁密语"), event_unlock_cryptolalia, 1);

    add_switch(xyobfcstr("快速移动"), event_quick_move, 2);
    add_switch(xyobfcstr("快速波次生成"), event_summon_all_waves, 2);
    add_switch(xyobfcstr("解除困难模式舰船限制"), event_remove_hardmode_fleet_limit, 2);
    add_switch(xyobfcstr("解除舰船移动范围限制"), event_expand_battle_bounds, 2);
    add_switch(xyobfcstr("移除心情过低警告"), event_remove_mood_warning, 2);
    add_switch(xyobfcstr("心情始终维持饱满"), event_remove_mood_full, 2);
    add_switch(xyobfcstr("移除舰炮自律开火动画"), event_remove_bbanimation, 2);
    add_switch(xyobfcstr("移除大世界迷雾"), event_remove_world_fog, 2);
    // @todo 移除舰炮开火立绘展示
    // @todo 敌方潜艇显形

    add_switch(xyobfcstr("解除周回模式条件限制"), event_remove_continuous_limit, 3);
    add_switch(xyobfcstr("非周回模式下允许自律"), event_uncontinuous_manual, 3);
    add_switch(xyobfcstr("自律寻敌击破所有敌舰"), event_autobot_all_clear, 3);
    add_switch(xyobfcstr("自律寻敌优先高星敌舰"), event_autobot_high_priority, 3);
    add_switch(xyobfcstr("大世界自动购买体力"), event_autobuy_energy_box, 3);
    add_switch(xyobfcstr("大世界自动补充体力"), event_autouse_energy_box, 3);
    add_switch(xyobfcstr("大世界自动使用设施"), event_autouse_siren_facilities, 3);

    value_start = switches.size();
    auto gen_set_autolimit_callback =
        make_gen(switches.size(), std::array{0,10,20,100,9999}, event_set_autotimes_limit);
    add_switch(xyobfcstr("自律次数上限: 10"), gen_set_autolimit_callback(), 4);
    add_switch(xyobfcstr("自律次数上限: 20"), gen_set_autolimit_callback(), 4);
    add_switch(xyobfcstr("自律次数上限: 100"), gen_set_autolimit_callback(), 4);
    add_switch(xyobfcstr("自律次数上限: 9999"), gen_set_autolimit_callback(), 4);

    auto gen_set_attack_scale_callback =
        make_gen(switches.size(), std::array{1,10,100,1000,10000}, event_set_attack_scale);
    add_switch(xyobfcstr("攻击倍率: 10"), gen_set_attack_scale_callback(), 4);
    add_switch(xyobfcstr("攻击倍率: 100"), gen_set_attack_scale_callback(), 4);
    add_switch(xyobfcstr("攻击倍率: 1000"), gen_set_attack_scale_callback(), 4);
    add_switch(xyobfcstr("攻击倍率: 10000"), gen_set_attack_scale_callback(), 4);
    add_switch(xyobfcstr("攻击倍率伤害限制"), event_set_attack_limit, 4);

    auto gen_set_defence_scale_callback =
        make_gen(switches.size(), std::array{1,10,100,1000}, event_set_defence_scale);
    add_switch(xyobfcstr("防御倍率: 10"), gen_set_defence_scale_callback(), 4);
    add_switch(xyobfcstr("防御倍率: 100"), gen_set_defence_scale_callback(), 4);
    add_switch(xyobfcstr("防御倍率: 1000"), gen_set_defence_scale_callback(), 4);

    auto gen_set_coldown_rate_callback =
        make_gen(switches.size(), std::array{1.,0.5,0.33,0.1,0.01}, event_set_coldown_rate);
    add_switch(xyobfcstr("武器冷却时间: 50%"), gen_set_coldown_rate_callback(), 4);
    add_switch(xyobfcstr("武器冷却时间: 33%"), gen_set_coldown_rate_callback(), 4);
    add_switch(xyobfcstr("武器冷却时间: 10%"), gen_set_coldown_rate_callback(), 4);
    add_switch(xyobfcstr("武器冷却时间: 1%"), gen_set_coldown_rate_callback(), 4);
    add_switch(xyobfcstr("解除武器公共冷却"), event_remove_global_cooldown, 4);
    add_switch(xyobfcstr("舰载机冷却限制"), event_hive_cooldown_limit, 4);

    auto gen_set_range_scale_callback =
        make_gen(switches.size(), std::array{1,2,3,5,10}, event_set_range_scale);
    add_switch(xyobfcstr("武器攻击距离: 2"), gen_set_range_scale_callback(), 4);
    add_switch(xyobfcstr("武器攻击距离: 3"), gen_set_range_scale_callback(), 4);
    add_switch(xyobfcstr("武器攻击距离: 5"), gen_set_range_scale_callback(), 4);
    add_switch(xyobfcstr("武器攻击距离: 10"), gen_set_range_scale_callback(), 4);

    add_switch(xyobfstr("替换聊天框指令"), event_replace_chat_box, 5);
    add_switch(xyobfstr("游戏屋：射击游戏速通"), event_gameroom_shooting_game, 5);
    // @todo 其他小游戏
}

std::vector<Switch>& SwitchGroup::get()
{
    return switches;
}

Switch& SwitchGroup::get(int id)
{
    int index = id - start_id;
    if (index <= 0) return switches.front();
    if (index >= switches.size()) return switches.back();
    return switches[index];
}

Switch* SwitchGroup::get(const char* name)
{
    int* ip = switches_hash.find(name);
    if (!ip) return nullptr;
    return &switches[*ip];
}

void SwitchGroup::set(LuaState* L, int id, bool open)
{
    if (id < start_id) return;
    int index = id - start_id;
    if (index >= switches.size()) return;
    if (switches[index].on == open) return;

    // LOGI("[%s] %s 切换为 %s 状态", SS_SET.get().data(), switches[index].name.c_str(), open ? "开启" : "关闭");
    switches[index].callback(L, open);
}
