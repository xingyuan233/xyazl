#include <sstream>

#include "../event.h"

#include "./etools.hpp"

using namespace xyu;

/* 战斗逻辑 */

void xyu::event_summon_all_waves(LuaState* L, bool on)
{
    if (on) L->swap_global(xyobfstr("ys.Battle.BattleSpawnWave.SetWaveData"), +[](LuaState* L)
    {
        // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: ys.Battle.BattleSpawnWave - SetWaveData"));
        int n = L->count();
        L->load_field(2, xyobfstr("preWaves"));
        if (int count = L->get_length(-1); count > 0)
        {
            L->store_field(-1, 1, 100);
            for (int i=2; i<=count; ++i) L->store_field(-1, i, {});
        }
        L->pop();

        L->load_field(1, xyobfcstr("__old_SetWaveData"));
        L->pop_insert(1);
        L->call(n, 0);
        return 0;
    });
    else L->revert_global(xyobfstr("ys.Battle.BattleSpawnWave.SetWaveData"));
}


static struct Battle
{
private:
    bool unlock_auto = false;
    bool manual_auto = false;
    bool hide_boss = false;
    bool high_priority = false;

public:
    void set_continuous_limit(LuaState* L, bool on)
    {
        if (unlock_auto == on) return;
        if (on)
        {
            if (!manual_auto) hook_continuous(L);
            use_enemie_acquire(L);
        }
        else
        {
            if (!manual_auto) unhook_continuous(L);
            unuse_enemie_acquire(L);
        }
        unlock_auto = on;
    }
    void set_continuous_manual(LuaState* L, bool on)
    {
        if (manual_auto == on) return;
        if (on)
        {
            if (!unlock_auto) hook_continuous(L);
            use_enemie_acquire(L);
        }
        else
        {
            if (!unlock_auto) unhook_continuous(L);
            unuse_enemie_acquire(L);
        }
        manual_auto = on;
    }
    void set_hide_boss(LuaState* L, bool on)
    {
        if (hide_boss == on) return;
        if (on)
        {
            use_enemie_acquire(L);
            hook_bosscell_acquire(L);
        }
        else
        {
            unuse_enemie_acquire(L);
            unhook_bosscell_acquire(L);
        }
        hide_boss = on;
    }
    void set_high_priority(LuaState* L, bool on)
    {
        if (high_priority == on) return;
        if (on) use_enemie_acquire(L);
        else unuse_enemie_acquire(L);
        high_priority = on;
    }

protected:
    int enemie_acquire_count = 0;
    void use_enemie_acquire(LuaState* L) { if (++enemie_acquire_count == 1) hook_enemie_acquire(L); }
    void unuse_enemie_acquire(LuaState* L) { if (--enemie_acquire_count == 0) unhook_enemie_acquire(L); }

private:
    LuaValue chapter;
    int hide_type = 0;
    int fleet_count = 0;
    int last_fleet_count = -1;
    int try_times = 0;
    int per_step_max = 1;
    int fleet_id = 1;
    struct NearInfo
    {
        bool champion = false;  // 是否为精英怪 (指会移动的怪,非Boss)
        int step = 1001;        // 距离 (大于 1000 表示阻断，无法前往)
        LuaValue path;          // 路径
    } near;

private:
    static void hook_continuous(LuaState* L);
    static void unhook_continuous(LuaState* L);
    static void hook_enemie_acquire(LuaState* L);
    static void unhook_enemie_acquire(LuaState* L);
    static void hook_bosscell_acquire(LuaState* L);
    static void unhook_bosscell_acquire(LuaState* L);
private:
    struct Pos { int row, column; };
    static Pos get_cur_pos(LuaState* L, int self);
    static void push_path(LuaState* L, int self, int pos, int enemie);
}
battle;

Battle::Pos Battle::get_cur_pos(LuaState* L, int self)
{
    if (self < 0) self = L->count() + self + 1;
    DeepGuard guard{L};

    // 获取当前舰队 id
    if (!battle.hide_type)
    {
        if (battle.unlock_auto || battle.manual_auto) L->load_field(self, xyobfstr("__old_GetActiveFleet"));
        else L->load_field(self, xyobfstr("GetActiveFleet"));
        L->push_copy(self);
        L->call(1, 1);
        battle.fleet_id = L->get_field(-1, xyobfstr("id")).num;
    }

    // 获取坐标
    L->load_field(self, xyobfstr("fleets"));
    L->load_field(-1, battle.fleet_id);
    L->load_field(-1, xyobfstr("line"));
    int row = L->get_field(-1, xyobfstr("row")).num;
    int column = L->get_field(-1, xyobfstr("column")).num;
    return {row, column};
}
void Battle::push_path(LuaState* L, int self, int pos, int enemie)
{
    if (self < 0) self = L->count() + self + 1;
    if (pos < 0) pos = L->count() + pos + 1;
    if (enemie < 0) enemie = L->count() + enemie + 1;

    // 获取路径
    if (battle.unlock_auto || battle.manual_auto)  L->load_field(self, xyobfstr("__old_findPath"));
    else L->load_field(self, xyobfstr("findPath"));
    L->push_copy(self);
    L->push(1.);
    L->push_copy(pos);
    L->push_copy(enemie);
    L->call(4, 2);
}

void Battle::hook_continuous(LuaState* L)
{
    // 允许本地启用周回模式
    L->swap_global(xyobfstr("Chapter.canActivateLoop"), func_always_true);

    // 周回模式服务器隐藏与本地恢复
    L->swap_global(xyobfstr("Chapter.isLoop"), +[](LuaState* L)
    {
        // 数据未加载完毕直接返回 (无关调用)
        if (!L->load_field(1, xyobfstr("loopFlag"), false))
        {
            L->push(false);
            return 1;
        }
        bool loopFlag = L->to_number(-1);

        // 获取调用栈
        const char* call_stack;
        print_stack(L, nullptr, 0, &call_stack);

        // 判断来源
        static int state = 0;
        constexpr int cover_times = 2;
        if (const char* p = strstr(call_stack, xyobfstr("TrackingCommand")))
        {
            // 3.接收响应: 服务器回调函数
            if (strstr(p, xyobfstr("SendWindow")))
            {
                if (state < 2 + cover_times)
                {
                    state = std::max(3, state + 1);
                    if (battle.hide_type)
                    {
                        loopFlag = true;
                        L->store_field(1, xyobfstr("loopFlag"), 1);
                    }
                    // LOGI(">> %s, loopFlag=%d hide_type=%d", xyobfcstr("isLoop-接收响应"), loopFlag, battle.hide_type);
                }
                // 非周回模式自律手动设置职能
                if (battle.hide_type == 2)
                {
                    // 获取舰队数量
                    if (!battle.fleet_count && L->load_field(1, xyobfstr("fleets"), false))
                    {
                        int new_fleet_count = L->get_length(-1);
                        // 多段加载，稳定后获取
                        if (battle.last_fleet_count != new_fleet_count) battle.last_fleet_count = new_fleet_count;
                        else
                        {
                            for (int i=1; i<=new_fleet_count; i++)
                            {
                                L->load_field(-1, i);
                                int fleet_type = L->get_field(-1, xyobfstr("fleetType")).num;
                                L->pop();
                                if (fleet_type == 1) ++battle.fleet_count;
                                else break;
                            }
                        }
                    }
                    // 第一舰队全部战斗，其余待机
                    if (battle.fleet_count)
                    {
                        L->create_table(battle.fleet_count);
                        L->store_field(-1, 1, 3);
                        for (int i=2; i<=battle.fleet_count; i++) L->store_field(-1, i, 4);
                        L->store_field(1, xyobfstr("duties"));
                    }
                }
            }
            // 2.发送请求：关卡开启（TrackingCommand.execute）
            else if (state != 2)
            {
                state = 2;
                battle.chapter = L->get(1);
                battle.hide_type = 0;
                // 非周回自律
                if (battle.manual_auto && !loopFlag)
                {
                    battle.hide_type = 2;
                    battle.fleet_count = 0;
                    battle.last_fleet_count = -1;
                }
                // 解除限制
                if (battle.unlock_auto)
                {
                    int progress = L->get_field(1, xyobfstr("progress")).num;
                    if (loopFlag && progress != 100)
                    {
                        battle.hide_type = 1;
                        loopFlag = false;
                        L->store_field(1, xyobfstr("loopFlag"), 0);
                    }
                }
                // LOGI(">> %s, loopFlag=%d hide_type=%d", xyobfcstr("isLoop-发送请求-关卡启动"), loopFlag, battle.hide_type);
            }
        }
        // 1.普通情况：直接基于原始 loopFlag
        else state = 0;

        L->push(loopFlag);
        return 1;
    });

    // 防止非周回模式下 自律状态显示异常
    L->swap_global(xyobfstr("Chapter.CanActivateAutoFight"), +[](LuaState* L)
    {
        if (battle.hide_type) L->push(true);
        else
        {
            int n = L->count();
            L->load_field(1, xyobfcstr("__old_CanActivateAutoFight"));
            L->pop_insert(1);
            L->call(n, 1);
        }
        return 1;
    });

    // 本地周回模式下，无移动限制，会直接移动到怪物格子，并且强制优先探测精英怪
    // 服务器非周回模式下，有移动距离检测，隐藏周回模式时需模拟手动操作移动到允许范围内
    // 本地周回大致执行过程:
    // 1.调用GetActiveFleet，确认当前舰队id (若有更换舰队操作发生在此之前)
    // 2.调用GetBossCell [无Boss返回nil，有Boss返回具体位置信息]
    // 3.若有Boss：直接调用 FindBossPath 找出通往 Boss 路径
    // 4.若无Boss：
    //   4.1调用 GetAllEnemies 获取敌人数量与位置
    //   4.2依次对每个敌人调用 findPath
    //   4.3若有精英怪优先共消灭精英怪，否则找路径最短的
    // 5.回到2 (若更换舰队回到1)

    L->swap_global(xyobfstr("Chapter.GetActiveFleet"), +[](LuaState* L)
    {
        int n = L->count();
        L->load_field(1, xyobfcstr("__old_GetActiveFleet"));
        L->pop_insert(1);
        L->call(n, 1);

        if (battle.hide_type)
        {
            L->load_field(-1, xyobfstr("baseSpeed"));
            battle.per_step_max = L->to_number(-1);
            L->pop();
            L->load_field(-1, xyobfstr("id"));
            battle.fleet_id = L->to_number(-1);
            L->pop();
        }
        // LOGI("[%s] %s: speed=%d fleet=%d", SS_EVENT.get().data(), xyobfcstr("函数被调用: GetActiveFleet"), battle.per_step_max, battle.fleet_id);
        return 1;
    });

    static auto move_execute = [](LuaState* L, double row, double column)
    {
        L->create_table(0, 4);
        L->store_field(-1, xyobfstr("type"), 1);
        L->store_field(-1, xyobfstr("id"), battle.fleet_id);
        L->store_field(-1, xyobfstr("arg1"), row);
        L->store_field(-1, xyobfstr("arg2"), column);
        command_execute(L, xyobfstr("ChapterOpCommand"), L->get_global(xyobfstr("GAME.CHAPTER_OP")));
    };

    static auto format_path = [](LuaState* L, int idx)
    {
        idx = idx < 0 ? L->count() + idx + 1 : idx;
        std::stringstream ss;
        bool first = true;
        ss << '[';
        L->push();
        while (L->next(idx))
        {
            if (!first) ss << ',';
            ss << '(' << L->get_field(-1, xyobfstr("row")).num << ',' <<
                L->get_field(-1, xyobfstr("column")).num << ')';
            L->pop();
        }
        ss << ']';
        return ss.str();
    };

    L->swap_global(xyobfstr("Chapter.findPath"), +[](LuaState* L)
    {
        auto check_champion = [](int attachment) { return attachment >= 12 && attachment <= 19; };

        bool is_champion = battle.try_times ? check_champion(L->get_field(4, xyobfstr("attachment")).num) : false;

        // 原始调用
        L->load_field(1, xyobfcstr("__old_findPath"));
        L->pop_insert(1);
        L->call(4, 2);

        // LOGI(">> findPath try_times=%d move=%s", battle.try_times, L->format(-2).c_str());
        // LOGI(">> oripath=%s", format_path(L,-1).c_str());

        if (battle.try_times)
        {
            // 获取距离
            int step = L->to_number(-2);
            // 存在允许距离内的精英怪则停止探测
            if (is_champion && step <= battle.per_step_max) { battle.try_times = 0; goto over; }

            // 更新最近敌人信息
            if (is_champion && !battle.near.champion)
                battle.near = {true, step, L->get(-1)};
            else if (is_champion == battle.near.champion && step < battle.near.step)
                battle.near = {is_champion, step, L->get(-1)};

            // 最后一次判定
            if (--battle.try_times == 0)
            {
                // 若距离允许则直接返回
                if (battle.near.step <= battle.per_step_max) goto over;
                // 尝试逐步移动
                is_champion = battle.near.champion;
                for (;;)
                {
                    // 移动一次
                    L->push(battle.near.path);
                    // LOGI("near_step=%d near_path=%s", battle.near.step, format_path(L,-1).c_str());
                    if (!L->load_field(-1, battle.per_step_max))
                    {
                        L->push(L->get_length(-1));
                        L->pop_replace(-3);
                        goto over;
                    }
                    double row = L->get_field(-1, xyobfstr("row")).num;
                    double column = L->get_field(-1, xyobfstr("column")).num;
                    move_execute(L, row, column);
                    // 检查移动后坐标
                    L->push(battle.chapter);
                    int instance = L->count();
                    auto rel = get_cur_pos(L, instance);
                    bool error = row != rel.row || column != rel.column;
                    L->resize(instance);
                    // 重新获取敌人列表
                    L->load_field(instance, xyobfstr("GetAllEnemies"));
                    L->push_copy(instance);
                    L->call(1, 1);
                    battle.try_times = 0;
                    int enemies = L->count();
                    // 当前位置
                    L->create_table(0, 2);
                    L->store_field(-1, xyobfstr("row"), rel.row);
                    L->store_field(-1, xyobfstr("column"), rel.column);
                    int pos = L->count();
                    // 获取新路径
                    battle.near.step = 1001;
                    int count = L->get_length(enemies);
                    for (int i=1; i<=count; ++i)
                    {
                        L->load_field(enemies, i);
                        // 跳过非精英怪 (若探测精英怪)
                        if (is_champion && !(check_champion(L->get_field(-1, xyobfstr("attachment")).num))) continue;
                        // 获取路径
                        push_path(L, instance, pos, -1);
                        int newstep = L->to_number(-2);
                        if (newstep <= battle.per_step_max || error) goto over;
                        if (newstep < battle.near.step) battle.near = {battle.near.champion, newstep, L->get(-1)};
                        L->pop(3);
                    }
                }
            }
        }
    over:
        if (battle.try_times == 0) battle.near = {false,1001,{}};
        return 2;
    });

    L->swap_global(xyobfstr("Chapter.FindBossPath"), +[](LuaState* L)
    {
        L->load_field(1, xyobfcstr("__old_FindBossPath"));
        L->pop_insert(1);
        L->call(3, 2);

        // LOGI(">> FindBossPath move=%s", L->format(-2).c_str());
        // LOGI("oripath=%s", format_path(L,-1).c_str());

        if (battle.hide_type)
        {
            int step = L->to_number(-2);
            if (step <= 1000 && step > battle.per_step_max)
            {
                double row, column;
                Pos rel;
                int last, p = 0;
                // 逐步移动至允许范围内
                L->push(battle.chapter);
                for (last = step; last > battle.per_step_max; last -= battle.per_step_max)
                {
                    p += battle.per_step_max;
                    if (!L->load_field(2, p)) goto error;
                    row = L->get_field(-1, xyobfstr("row")).num;
                    column = L->get_field(-1, xyobfstr("column")).num;
                    L->resize(3);
                    move_execute(L, row, column);

                    // 检查移动后坐标
                    rel = get_cur_pos(L, 3);
                    if (row != rel.row || column != rel.column) goto error;
                    L->pop(3);
                    continue;

                error:
                    // 回溯
                    for (int start = p - battle.per_step_max;;)
                    {
                        if (--p == start) break;
                        L->load_field(2, p);
                        double last_row = L->get_field(-1, xyobfstr("row")).num;
                        double last_column = L->get_field(-1, xyobfstr("column")).num;
                        if (last_row == rel.row && last_column == rel.column) break;
                    }
                    last = step - p;
                    break;
                }
                L->resize(2);
                // 移动表格剩余位置
                for (int i=1; i<=last; ++i)
                {
                    L->load_field(2, i + p);
                    L->store_field(2, i);
                }
                // 删除多余位置
                for (int i=last+1; i<=step; ++i) L->store_field(2, i, {});
                L->set(1, last);
            }
        }
        return 2;
    });

    // 非周回触发伏击时，需要手动规避导致自律停止，且触发过程发生在服务器回调无法避免
    // 此处模拟手动点击自动规避，规避失败会自动进行迎击不会导致自律中断
    L->swap_global(xyobfstr("LevelAmbushView.SetFuncOnComplete"), +[](LuaState* L)
    {
        int n = L->count();
        LuaValue self = battle.hide_type ? L->get(1) : LuaValue{};
        // 调用原函数
        L->load_field(1, xyobfcstr("__old_SetFuncOnComplete"));
        L->pop_insert(1);
        L->call(n, 0);

        if (battle.hide_type)
        {
            // 尝试规避伏击
            L->push(self);
            gevent.emit(L, 1, xyobfstr("LevelMediator2:ON_OP"), 4, battle.fleet_id, 1);
            // 销毁面板
            L->push(L->get_global(xyobfstr("BaseSubView.Destroy")));
            L->pop_insert(1);
            L->call(1, 0);
        }
        return 0;
    });
}
void Battle::unhook_continuous(LuaState* L)
{
    battle.hide_type = 0;
    L->revert_global(xyobfstr("Chapter.canActivateLoop"));
    L->revert_global(xyobfstr("Chapter.isLoop"));
    L->revert_global(xyobfstr("Chapter.CanActivateAutoFight"));
    L->revert_global(xyobfstr("Chapter.GetActiveFleet"));
    L->revert_global(xyobfstr("Chapter.findPath"));
    L->revert_global(xyobfstr("Chapter.FindBossPath"));
    L->revert_global(xyobfstr("LevelAmbushView.SetFuncOnComplete"));
}

void Battle::hook_enemie_acquire(LuaState* L)
{
    L->swap_global(xyobfstr("Chapter.GetAllEnemies"), +[](LuaState* L)
    {
        int count;
        bool print = false;
        goto start;
    end:
        battle.try_times = battle.hide_type ? count : 0;
        // if (!print) LOGI(">> GetAllEnemies %d ", count);
        L->resize(1);
        return 1;

    start:
        // LOGI(">> GetAllEnemies");

        // 原函数调用
        int n = L->count();
        LuaValue self = L->get(1);
        L->load_field(1, xyobfcstr("__old_GetAllEnemies"));
        L->pop_insert(1);
        L->call(n, 1);
        if ((count = L->get_length(1)) == 1) goto end;
        if (!battle.hide_boss && !battle.high_priority) goto end;

        // 获取过滤列表
        int enemies = 1;
        int instance = 2;
        L->push(self);

        int level = battle.high_priority ? L->get_field(instance, xyobfstr("id")).num : 0;
        int limit = 1;
        bool first = true;
        std::string flag(count + 1, 100);
        bool nofilter = true;
        for (int i=1; i<=count; ++i)
        {
            L->load_field(enemies, i);
            int attachment = L->get_field(-1, xyobfstr("attachment")).num;
            // 过滤 Boss
            if (battle.hide_boss && attachment == 8)
            {
                flag[i] = 0;
                nofilter = false;
                if (!battle.high_priority) break;
            }
            // 获取敌舰星级
            if (battle.high_priority && attachment == 6)
            {
                int id = L->get_field(-1, xyobfstr("attachmentId")).num;
                if (level <= 1'99'99) id /= 10;
                flag[i] = (id % 100 + 2) % 3 + 1;
                if (first) { limit = flag[i]; first = false; }
                else
                {
                    if (flag[i] != limit) nofilter = false;
                    if (flag[i] > limit) limit = flag[i];
                }
            }
            L->pop();
        }
        if (nofilter) goto end;

        // 可达性判断
        L->create_table(0, 2);
        auto ipos = get_cur_pos(L, instance);
        int cur_pos = 3;
        L->store_field(cur_pos, xyobfstr("row"), ipos.row);
        L->store_field(cur_pos, xyobfstr("column"), ipos.column);

        std::string detect(count + 1, 0);
        while (limit > 0)
        {
            for (int i=1; i<=count; ++i)
            {
                if (detect[i]) continue;
                if (flag[i] < limit) continue;
                detect[i] = 1;

                L->load_field(enemies, i);
                push_path(L, instance, cur_pos, -1);
                if (L->to_number(-2) <= 1000) goto over;
                L->pop(3);
            }
            --limit;
        }
        if (limit == 0) goto end;
    over:

        // 过滤
        int i, j;
        bool move = false;
        for (i=1, j=0; i<=count; ++i)
        {
            if (flag[i] < limit) { move = true; continue; }
            ++j;
            if (move)
            {
                L->load_field(1, i);
                L->store_field(1, j);
            }
        }
        // 删除
        for (i=j+1; i<=count; ++i) L->store_field(1, i, {});
        // LOGI(">> GetAllEnemies %d -> %d", count, j);
        print = true;
        count = j;
        goto end;
    });
}
void Battle::unhook_enemie_acquire(LuaState* L)
{
    L->revert_global(xyobfstr("Chapter.GetAllEnemies"));
}

void Battle::hook_bosscell_acquire(LuaState* L)
{
    L->swap_global(xyobfstr("Chapter.GetBossCell"), +[](LuaState* L)
    {
        // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: Chapter - GetBossCell"));
        int n = L->count();

        // 若敌人数量大于1，不返回 Boss 位置
        L->load_field(1, xyobfcstr("__old_GetAllEnemies"));
        L->push_copy(1);
        L->call(1, 1);
        if (L->get_length(-1) > 1)
        {
            L->push();
            return 1;
        }
        L->resize(n);

        // 仅剩一个敌人时，再判断是否为 Boss
        L->load_field(1, xyobfcstr("__old_GetBossCell"));
        L->pop_insert(1);
        L->call(n, 1);
        return 1;
    });
}
void Battle::unhook_bosscell_acquire(LuaState* L)
{
    L->revert_global(xyobfstr("Chapter.GetBossCell"));
}

void xyu::event_remove_continuous_limit(LuaState* L, bool on) { battle.set_continuous_limit(L, on); }
void xyu::event_uncontinuous_manual(LuaState* L, bool on) { battle.set_continuous_manual(L, on); }
void xyu::event_autobot_all_clear(LuaState* L, bool on) { battle.set_hide_boss(L, on); }
void xyu::event_autobot_high_priority(LuaState* L, bool on) { battle.set_high_priority(L, on); }
