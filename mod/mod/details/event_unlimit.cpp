#include "../event.h"

#include <fstream>
#include <sstream>

#include "./etools.hpp"
#include "../../utils/hashtable.h"

using namespace xyu;

/* 破限 */

void xyu::event_remove_mood_warning(LuaState* L, bool on)
{
    if (on) L->swap_global(xyobfstr("Fleet.EnergyCheck"), +[](LuaState* L)
    {
        // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: Fleet - EnergyCheck"));

        // 将用于判断的临时数据置为 150
        L->push();
        while (L->next(1))
        {
            L->store_field(-1, xyobfstr("energy"), 150);
            L->pop();
        }

        // 原始判断逻辑 (附带其他功能，直接返回会导致无法进入关卡)
        int n = L->count();
        L->push(L->get_global(xyobfstr("Fleet.__old_EnergyCheck")));
        L->pop_insert(1);
        L->call(n, 0);
        return 0;
    });
    else L->revert_global(xyobfstr("Fleet.EnergyCheck"));
}

void xyu::event_remove_mood_full(LuaState* L, bool on)
{
    if (on) L->swap_global(xyobfstr("Ship.getEnergy"), +[](LuaState* L)
    {
        L->push(150);
        return 1;
    });
    else L->revert_global(xyobfstr("Ship.getEnergy"));
}

void xyu::event_remove_hardmode_fleet_limit(LuaState* L, bool on)
{
    if (on)
    {
        // 解除舰船类型限制
        L->swap_global(xyobfstr("LevelFleetView.initAddButton"), +[](LuaState* L)
        {
            // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: LevelFleetView - initAddButton"));
            int n = L->count();
            int count = L->get_length(4);
            for (int i=1; i<=count; ++i) L->store_field(4, i, 0);

            L->load_field(1, xyobfcstr("__old_initAddButton"));
            L->pop_insert(1);
            L->call(n, 0);
            return 0;
        });

        // 解除数值限制
        gtips.create(L);
        L->swap_global(xyobfstr("Chapter.IsEliteFleetLegal"), +[](LuaState* L)
        {
            // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: Chapterdd - IsEliteFleetLegal"));

            // 检查是否所有舰队均为空
            bool empty = true;
            L->load_field(1, xyobfstr("eliteFleetList"));
            L->load_field(-1, 1);
            int count = L->get_length(-1);
            for (int i=1; i<=count; ++i)
            {
                L->load_field(-1, i);
                L->load_field(-1, xyobfstr("ships"));
                if (L->get_length(-1) != 0) { empty = false; break; }
                L->pop(2);
            }
            // 输出错误提示
            if (empty) gtips.show(L, xyobfstr("未出战任何舰船"));

            L->push(!empty);
            return 1;
        });
    }
    else
    {
        L->revert_global(xyobfstr("LevelFleetView.initAddButton"));

        gtips.destroy(L);
        L->revert_global(xyobfstr("Chapter.IsEliteFleetLegal"));
    }
}

void xyu::event_set_autotimes_limit(LuaState* L, int newlimit)
{
    static int autolimit = 0;
    if (newlimit < 0) newlimit = 0;
    if (autolimit == newlimit) return;

    // 替换面板显示的最大值
    if (newlimit != 0 && autolimit == 0)
    {
        LuaValue hook = +[](LuaState* L)
        {
            L->load_field(1, xyobfstr("__old_didEnter"));
            L->push_copy(1);
            L->call(1, 0);

            L->load_field(1, "_pageUtil");
            L->store_field(-1, "_maxNum", autolimit);

            return 0;
        };

        L->swap_global(xyobfstr("ContinuousOperationWindow.didEnter"), hook);
        L->swap_global(xyobfstr("LevelContinuousOperationWindow.didEnter"), hook);
        L->swap_global(xyobfstr("BossRushContinuousOperationWindow.didEnter"), hook);
        L->swap_global(xyobfstr("BossSingleContinuousOperationWindow.didEnter"), hook);
    }
    else if (newlimit == 0 && autolimit != 0)
    {
        L->revert_global(xyobfstr("ContinuousOperationWindow.didEnter"));
        L->revert_global(xyobfstr("LevelContinuousOperationWindow.didEnter"));
        L->revert_global(xyobfstr("BossRushContinuousOperationWindow.didEnter"));
        L->revert_global(xyobfstr("BossSingleContinuousOperationWindow.didEnter"));
    }

    // 次数过大时会被修正为较小值，此处手动恢复
    if (newlimit != 0 && autolimit == 0)
    {
        static double real_times = 0;

        LuaValue record = +[](LuaState* L)
        {
            int n = L->count();
            real_times = L->get_field(1, xyobfstr("contextData.battleTimes")).num;
            L->load_field(1, xyobfcstr("__old_UpdateContent"));
            L->pop_insert(1);
            L->call(n ,0);
            return 0;
        };

        L->swap_global(xyobfstr("ContinuousOperationWindow.UpdateContent"), record);
        L->swap_global(xyobfstr("LevelContinuousOperationWindow.UpdateContent"), record);
        L->swap_global(xyobfstr("BossRushContinuousOperationWindow.UpdateContent"), record);
        L->swap_global(xyobfstr("BossSingleContinuousOperationWindow.UpdateContent"), record);


        L->swap_global(xyobfstr("ChapterProxy.InitContinuousTime"), +[](LuaState* L)
        {
            if (real_times) L->set(3, real_times), real_times = 0;
            int n = L->count();
            L->load_field(1, xyobfcstr("__old_InitContinuousTime"));
            L->pop_insert(1);
            L->call(n ,0);
            return 0;
        });

        L->swap_global(xyobfstr("BeginStageCommand.execute"), +[](LuaState* L)
        {
            if (real_times)
            {
                L->load_field(2, xyobfstr("body"));
                L->store_field(-1, xyobfstr("totalBattleTimes"), real_times);
                L->store_field(-1, xyobfstr("continuousBattleTimes"), real_times);
                L->pop();
                real_times = 0;
            }

            int n = L->count();
            L->load_field(1, "__old_execute");
            L->pop_insert(1);
            L->call(n, 0);

            return 0;
        });
    }
    else if (newlimit == 0 && autolimit != 0)
    {
        L->revert_global(xyobfstr("ContinuousOperationWindow.UpdateContent"));
        L->revert_global(xyobfstr("LevelContinuousOperationWindow.UpdateContent"));
        L->revert_global(xyobfstr("BossRushContinuousOperationWindow.UpdateContent"));
        L->revert_global(xyobfstr("BossSingleContinuousOperationWindow.UpdateContent"));

        L->revert_global(xyobfstr("ChapterProxy.InitContinuousTime"));
        L->revert_global(xyobfstr("BeginStageCommand.execute"));
    }

    autolimit = newlimit;
}

void xyu::event_expand_battle_bounds(LuaState* L, bool on)
{
    if (on)
    {
        constexpr static double left_bound = -115;      // 演习模式下的左边界
        constexpr static double right_bound = 25;       // 演习模式下的右边界

        L->swap_global(xyobfstr("ys.Battle.BattleDataProxy.GetTotalBounds"), +[](LuaState* L)
        {
            // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: ys.Battle.BattleDataProxy - GetTotalBounds"));
            int n = L->count();
            L->load_field(1, xyobfcstr("__old_GetTotalBounds"));
            L->pop_insert(1);
            L->call(n, 4);
            L->set(3, std::min(left_bound, L->get(3).num));
            L->set(4, std::max(right_bound, L->get(4).num));
            return 4;
        });
        L->swap_global(xyobfstr("ys.Battle.BattleFleetBound.GetBound"), +[](LuaState* L)
        {
            // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: ys.Battle.BattleFleetBound - GetBound"));
            int n = L->count();
            L->load_field(1, xyobfcstr("__old_GetBound"));
            L->pop_insert(1);
            L->call(n, 6);
            L->set(3, std::min(left_bound, L->get(3).num));
            L->set(6, std::max(right_bound, L->get(6).num));
            return 6;
        });
    }
    else
    {
        L->revert_global(xyobfstr("ys.Battle.BattleDataProxy.GetTotalBounds"));
        L->revert_global(xyobfstr("ys.Battle.BattleFleetBound.GetBound"));
    }
}
