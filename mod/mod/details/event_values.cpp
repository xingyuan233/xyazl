#include "etools.hpp"
#include "../event.h"
#include "../../utils/hashtable.h"

#include "../../utils/obfstr.h"
#include "../../utils/log.h"

using namespace xyu;

/* 战斗数值 */

static struct DamageHandle
{
private:
    constexpr static double min_scale = 1e-12;
    double ack = 1;
    double def = 1;
    bool exercise_except = false;
    bool attack_limit = false;

public:
    void set_ack(LuaState* L, double newack)
    {
        if (newack < min_scale) newack = min_scale;
        if (newack == ack) return;
        if (def == 1)
        {
            if (ack == 1 && newack != 1) hook_damage(L);
            else if (ack != 1 && newack == 1) unhook_damage(L);
        }
        ack = newack;
    }
    void set_def(LuaState* L, double newdef)
    {
        if (newdef < min_scale) newdef = min_scale;
        if (newdef == def) return;
        if (ack == 1)
        {
            if (def == 1 && newdef != 1) hook_damage(L);
            else if (def != 1 && newdef == 1) unhook_damage(L);
        }
        def = newdef;
    }
    void set_exercise_except(bool on) { exercise_except = on; }
    void set_attck_limit(bool on) { attack_limit = on; }

private:
    static void hook_damage(LuaState* L);
    static void unhook_damage(LuaState* L);
}
damage_handle;

void DamageHandle::hook_damage(LuaState* L)
{
    L->swap_global(xyobfstr("ys.Battle.BattleDataProxy.HandleDamage"), +[](LuaState* L)
    {
        // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: ys.Battle.BattleDataProxy - HandleDamage"));
        int n = L->count();

        // 获取攻击来源
        int from = L->get_field(2, xyobfstr("_IFF")).num;
        int to = L->get_field(3, xyobfstr("_IFF")).num;
        // 获取伤害
        double dmg = L->get_field(2, xyobfstr("_correctedDMG")).num;
        double fix = L->get_field(2, xyobfcstr("_dmgEnhanceRate")).num;

        // 我方攻击
        if (from == 1 && to == -1)
        {
            bool use_scale = true;
            if (damage_handle.exercise_except)
            {
                // 演习不修改攻击伤害
                int type = L->get_field(3, xyobfstr("_type")).num;
                use_scale = type != 1;
            }
            if (use_scale)
            {
                double ori_dmg = dmg;
                dmg = std::max(1., dmg) * damage_handle.ack;
                // 防止伤害溢出太多
                if (damage_handle.attack_limit)
                {
                    double hp = L->get_field(3, xyobfstr("_currentHP")).num;
                    if (dmg > hp) dmg = hp;
                    if (dmg < ori_dmg) dmg = ori_dmg;
                }
                L->store_field(2, xyobfstr("_correctedDMG"), dmg);
            }
        }
        // 我方受击
        else if (to == 1)
        {
            double def = damage_handle.def;
            // 计算公式中基础伤害会取 max(1, _correctedDMG)，直接修改 _correctedDMG 很可能导致部分防御倍率无效
            if (dmg > 1)
            {
                if (dmg < def) { def /= dmg; dmg = 1; }
                else { dmg /= def; def = 1; }
                L->store_field(2, xyobfstr("_correctedDMG"), dmg);
            }
            if (def > 1)
            {
                fix /= def;
                L->store_field(2, xyobfstr("_dmgEnhanceRate"), fix);
            }
        }

        // L->load_field(3, xyobfstr("_tmpData"));
        // L->load_field(-1, xyobfstr("name"));
        // const char* target_name = L->to_cstring(-1);

        L->resize(n);
        L->load_field(1, xyobfcstr("__old_HandleDamage"));
        L->pop_insert(1);
        L->call(n, 2);

        // LOGI("  >> form:%d, to:%d, dmg:%g, rate:%g, target:%s", from, to, dmg, fix, target_name);
        return 2;
    });

    L->swap_global(xyobfstr("ys.Battle.BattleDataProxy.HandleDirectDamage"), +[](LuaState* L)
    {
        int n = L->count();

        // 判断受击对象
        int to = L->get_field(2, xyobfstr("_IFF")).num;
        // 我方受击
        if (to == 1)
        {
            // 获取原始伤害
            double dmg = L->to_number(3);
            dmg /= damage_handle.def;
            L->set(3, dmg);
        }
        // 敌方受击
        else if (to == -1)
        {
            double dmg = L->to_number(3), ori_dmg = dmg;
            dmg = std::max(1., dmg) * damage_handle.ack;
            // 防止伤害溢出太多
            if (damage_handle.attack_limit)
            {
                double hp = L->get_field(2, xyobfstr("_currentHP")).num;
                if (dmg > hp) dmg = hp;
                if (dmg < ori_dmg) dmg = ori_dmg;
            }
            L->set(3, dmg);
        }

        // L->load_field(2, xyobfstr("_tmpData"));
        // L->load_field(-1, xyobfstr("name"));
        // const char* target_name = L->to_cstring(-1);
        // double dmg = L->to_double(3);

        L->resize(n);
        L->push(L->get_global(xyobfstr("ys.Battle.BattleDataProxy.__old_HandleDirectDamage")));
        L->pop_insert(1);
        L->call(n, 0);

        // LOGI("  >> direct input=%d, to=%d, target=%s, dmg=%g", n, to, target_name, dmg);
        return 0;
    });
}
void DamageHandle::unhook_damage(LuaState* L)
{
    L->revert_global(xyobfstr("ys.Battle.BattleDataProxy.HandleDamage"));
    L->revert_global(xyobfstr("ys.Battle.BattleDataProxy.HandleDirectDamage"));
}

void xyu::event_exercise_no_scale(LuaState* L, bool on) { damage_handle.set_exercise_except(on); }
void xyu::event_set_attack_scale(LuaState* L, double newack) { damage_handle.set_ack(L, newack); }
void xyu::event_set_attack_limit(LuaState* L, bool on) { damage_handle.set_attck_limit(on); }
void xyu::event_set_defence_scale(LuaState* L, double newdef) { damage_handle.set_def(L, newdef); }


static struct Weapon
{
private:
    double cooldown_rate = 1;
    double range_scale = 1;
    bool remove_gcd = false;
    bool hive_limit = false;

public:
    void set_cooldown_rate(LuaState* L, double rate)
    {
        if (rate == cooldown_rate) return;
        if (cooldown_rate == 1) add_use(L), hook_bullet(L);
        else if (rate == 1) sub_use(L), unhook_bullet(L);
        cooldown_rate = rate;
    }
    void set_range_scale(LuaState* L, double scale)
    {
        if (scale == range_scale) return;
        if (range_scale == 1) add_use(L);
        else if (scale == 1) sub_use(L);
        range_scale = scale;
    }
    void set_remove_gcd(LuaState* L, bool on)
    {
        if (remove_gcd == on) return;
        on ? add_use(L) : sub_use(L);
        remove_gcd = on;
    }
    void set_hive_limit(LuaState* L, bool on)
    {
        hive_limit = on;
    }

private:
    int use_count = 0;
    void add_use(LuaState* L) { if (++use_count == 1) hook(L); }
    void sub_use(LuaState* L) { if (--use_count == 0) unhook(L); }

private:
    static void hook(LuaState* L);
    static void unhook(LuaState* L);
    static void hook_bullet(LuaState* L);
    static void unhook_bullet(LuaState* L);
}
weapon;

void Weapon::hook(LuaState* L)
{
    static std::vector<std::string> filter{"_environmentList", "_friendlyShipList","_listenerMap_","class",
        "_environmentList", "_cameraUtil", "_waveSummonList", "_dataProxy", "_fleet", "_cldSystem"};

    L->swap_global(xyobfstr("ys.Battle.BattleDataProxy.InitAllFleetUnitsWeaponCD"), +[](LuaState* L)
    {
        // LOGI(">> InitAllFleetUnitsWeaponCD");
        static LuaValue val;
        static HashTable<void*,bool> ht;
        LuaValue self = weapon.cooldown_rate != 1 ? L->get(1) : LuaValue{};
        // L->print_to(gpath.get("BattleDataProxy"), 1, 5, filter);

        // 修改每艘舰船的武器属性
        L->load_field(1, xyobfstr("_friendlyShipList"));
        L->push();
        while (L->next(-2))
        {
            ht.clear();

            L->load_field(-1, xyobfstr("_totalWeapon"));
            L->push();
            while (L->next(-2))
            {
                ht.get(L->to_address(-1)) = true;

                if (weapon.cooldown_rate != 1)
                {
                    if (L->get_field(-1, xyobfstr("_reloadMax"), &val))
                        L->store_field(-1, xyobfstr("_reloadMax"), val.num * weapon.cooldown_rate);
                    if (L->get_field(-1, xyobfstr("_cacheReloadMax"), &val))
                        L->store_field(-1, xyobfstr("_cacheReloadMax"), val.num * weapon.cooldown_rate);
                    if (L->get_field(-1, xyobfstr("_reloadRequire"), &val))
                        L->store_field(-1, xyobfstr("_reloadRequire"), val.num * weapon.cooldown_rate);
                    if (L->get_field(-1, xyobfstr("_cacheReloadTime"), &val))
                        L->store_field(-1, xyobfstr("_cacheReloadTime"), val.num * weapon.cooldown_rate);
                }

                if (weapon.range_scale != 1 && L->get_field(-1, xyobfcstr("_maxRangeSqr"), &val))
                    L->store_field(-1, xyobfstr("_maxRangeSqr"), val.num * weapon.range_scale);

                if (weapon.remove_gcd && L->get_field(-1, xyobfstr("_GCD"), &val))
                    L->store_field(-1, xyobfstr("_GCD"), 0.01);

                L->pop();
            }
            L->pop();

            L->load_field(-1, xyobfstr("_autoWeaponList"));
            L->push();
            while (L->next(-2))
            {
                if (ht.find(L->to_address(-1))) goto next;

                if (weapon.cooldown_rate != 1 && L->get_field(-1, xyobfstr("_reloadMax"), &val))
                    L->store_field(-1, xyobfstr("_reloadMax"), val.num * weapon.cooldown_rate);

                if (weapon.range_scale != 1 && L->get_field(-1, xyobfcstr("_maxRangeSqr"), &val))
                    L->store_field(-1, xyobfstr("_maxRangeSqr"), val.num * weapon.range_scale);

                if (weapon.remove_gcd && L->get_field(-1, xyobfstr("_GCD"), &val))
                    L->store_field(-1, xyobfstr("_GCD"), 0.01);

            next:
                L->pop();
            }
            L->pop();

            L->pop();
        }
        // L->print_to(gpath.get("_friendlyShipList"), -1, 8, filter);

        // 修改舰队属性 (舰载机,防空圈,反潜圈...)
        // 防空圈，反潜圈等由整个舰队共享，已经提前算好放入 fleet 中
        // 每个可携带舰载机的角色会把所有的舰载机打包在一起放到 fleet 的 _airAssistVO 队列中，仅修改 _totalWeapon 无效
        L->load_field(1, xyobfstr("_fleetList"));
        L->push();
        while (L->next(-2))
        {
            // 舰载机
            if (weapon.cooldown_rate != 1)
            {
                L->load_field(-1, xyobfstr("_airAssistVO"));
                L->load_field(-1, xyobfstr("_readyList"));
                L->push();
                while (L->next(-2))
                {
                    auto get_new = [](double num)
                    {
                        num *= weapon.cooldown_rate;
                        if (weapon.hive_limit && num < 300) num = 300;
                        return num;
                    };

                    if (L->get_field(-1, xyobfstr("_reloadMax"), &val))
                        L->store_field(-1, xyobfstr("_reloadMax"), get_new(val.num));
                    if (L->get_field(-1, xyobfstr("_totalReload"), &val))
                        L->store_field(-1, xyobfstr("_totalReload"), get_new(val.num));

                    L->pop();
                }
                L->pop(2);
            }

            // 防空炮
            L->load_field(-1, xyobfstr("_fleetAntiAir"));
            if (weapon.cooldown_rate != 1) L->store_field(-1, xyobfstr("_interval"),
                L->get_field(-1, xyobfstr("_interval")).num * weapon.cooldown_rate);
            if (weapon.range_scale != 1) L->store_field(-1, xyobfstr("_range"),
                L->get_field(-1, xyobfstr("_range")).num * weapon.range_scale);
            L->pop();

            // 声呐探测
            if (weapon.range_scale != 1)
            {
                L->load_field(-1, xyobfstr("_fleetStaticSonar"));
                L->store_field(-1, xyobfstr("_diameter"),
                    L->get_field(-1, xyobfstr("_diameter")).num * weapon.range_scale);
                L->store_field(-1, xyobfstr("_radius"),
                    L->get_field(-1, xyobfstr("_radius")).num * weapon.range_scale);
                L->pop();
            }

            if (weapon.remove_gcd)
            {
                auto remove_one = [L](const char* name)
                {
                    L->load_field(-1, name);
                    L->store_field(-1, xyobfstr("_GCD"), 0.01);
                    L->pop();
                };

                // 鱼雷
                remove_one(xyobfcstr("_torpedoWeaponVO"));
                // 舰炮
                remove_one(xyobfcstr("_chargeWeaponVO"));
                // 舰载机
                remove_one(xyobfcstr("_airAssistVO"));
                // 防空炮
                remove_one(xyobfcstr("_fleetRangeAntiAir"));
            }

            L->pop();
        }
        // L->print_to(gpath.get("_fleetList"), -1, 8, filter);

        // 调用原初始化过程
        L->resize(1);
        L->load_field(1, xyobfstr("__old_InitAllFleetUnitsWeaponCD"));
        L->pop_insert(1);
        L->call(1, 0);
        return 0;
    });
}
void Weapon::unhook(LuaState* L)
{
    L->revert_global(xyobfstr("ys.Battle.BattleDataProxy.InitAllFleetUnitsWeaponCD"));
}

void Weapon::hook_bullet(LuaState* L)
{
    L->swap_global(xyobfstr("ys.Battle.BattleBulletUnit.SetAttr"), +[](LuaState* L)
    {
        int n = L->count();
        LuaValue self = L->get(1);
        L->load_field(1, xyobfstr("__old_SetAttr"));
        L->pop_insert(1);
        L->call(n, 0);

        L->push(self);
        if (L->get_field(1, xyobfstr("_IFF")).num == 1)
        {
            LuaValue v;
            if (L->get_field(1, xyobfstr("_range"), &v))
            {
                L->store_field(1, xyobfstr("_range"), v.num *= weapon.range_scale);
                L->store_field(1, xyobfstr("_sqrRange"), v.num * v.num);
            }
        }
        return 0;
    });
}
void Weapon::unhook_bullet(LuaState* L)
{
    L->revert_global(xyobfstr("ys.Battle.BattleBulletUnit.SetAttr"));
}

void xyu::event_set_coldown_rate(LuaState* L, double rate) { weapon.set_cooldown_rate(L, rate); }
void xyu::event_remove_global_cooldown(LuaState* L, bool on) { weapon.set_remove_gcd(L, on); }
void xyu::event_hive_cooldown_limit(LuaState* L, bool on) { weapon.set_hive_limit(L, on); }
void xyu::event_set_range_scale(LuaState* L, double range) { weapon.set_range_scale(L, range); }
