#include "../event.h"

#include "./etools.hpp"

using namespace xyu;

void xyu::event_gameroom_shooting_game(LuaState* L, bool on)
{
    if (on)
    {
        L->swap_global(xyobfstr("GameRoomShootingView.initData"), +[](LuaState* L)
        {
            int n = L->count();
            LuaValue self = L->get(1);
            L->load_field(1, xyobfcstr("__old_initData"));
            L->pop_insert(1);
            L->pcall(n, 0);

            L->push(self);
            L->load_field(1, xyobfstr("tempConfig"));
            L->store_field(-1, xyobfstr("waitCountdown"), 0);
            L->store_field(-1, xyobfstr("baseTime"), 0);
            return 0;
        });
        L->swap_global(xyobfstr("GameRoomShootingView.gameFinish"), +[](LuaState* L)
        {
            int n = L->count();
            L->store_field(1, xyobfstr("score"), 35);
            L->load_field(1, xyobfstr("targetCount"));
            L->store_field(-1, 1, 0);        // 积分10靶子 2->0 +20分
            L->store_field(-1, 2, 1);        // 积分 5靶子 4->1 +15分
            L->resize(n);

            L->load_field(1, xyobfcstr("__old_gameFinish"));
            L->pop_insert(1);
            L->call(n, 0);
            return 0;
        });
    }
    else
    {
        L->revert_global(xyobfstr("GameRoomShootingView.initData"));
        L->revert_global(xyobfstr("GameRoomShootingView.gameFinish"));
    }
}
