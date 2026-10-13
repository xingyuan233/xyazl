#include "../event.h"

#include <fstream>

#include "./etools.hpp"
#include "../../utils/hashtable.h"

/* 动画 */

void xyu::event_quick_move(LuaState* L, bool on)
{
    if (on)
    {
        L->swap_global(xyobfstr("ChapterConst.ShipStepDuration"), 0);
        L->swap_global(xyobfstr("ChapterConst.ShipStepQuickPlayScale"), 0);
        L->swap_global(xyobfstr("WorldConst.GetTerrainMoveStepDuration"), +[](LuaState* L)
        {
            L->push(0);
            return 1;
        });
    }
    else
    {
        L->revert_global(xyobfstr("ChapterConst.ShipStepDuration"));
        L->revert_global(xyobfstr("ChapterConst.ShipStepQuickPlayScale"));
        L->revert_global(xyobfstr("WorldConst.GetTerrainMoveStepDuration"));
    }
}

void xyu::event_remove_bbanimation(LuaState* L, bool on)
{
    if (on) L->swap_global(xyobfstr("ys.Battle.BattleManualWeaponAutoBot.SetActive"), +[](LuaState* L)
    {
        L->set(3, LuaValue{false});
        L->load_field(1, xyobfcstr("__old_SetActive"));
        L->pop_insert(1);
        L->call(3, 0);
        return 0;
    });
    else L->revert_global(xyobfstr("ys.Battle.BattleManualWeaponAutoBot.SetActive"));
}

