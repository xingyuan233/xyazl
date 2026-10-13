#include <sstream>

#include "../event.h"

#include "./etools.hpp"
#include "../../utils/hashtable.h"
#include "../../utils/il2.h"
using namespace xyu;

void xyu::event_remove_world_fog(LuaState* L, bool on)
{
    if (on) L->swap_global(xyobfstr("WorldMap.GetFOVRange"), +[](LuaState* L)
    {
        L->push(66);
        return 1;
    });
    else L->revert_global(xyobfstr("WorldMap.GetFOVRange"));
}

void xyu::event_remove_world_auto_limit(LuaState* L, bool on)
{
    if (on) L->swap_global(xyobfstr("WorldMap.CanAutoFight"), func_always_true);
    else L->revert_global(xyobfstr("WorldMap.CanAutoFight"));
}


static LuaValue WorldInventoryProxy;
static auto get_item_count = [](LuaState* L, int id)
{
    L->push(WorldInventoryProxy);
    L->load_field(-1, xyobfstr("GetItemCount"));
    L->pop_insert(-2);
    L->push(id);
    L->call(2, 1);
    int count = L->to_number(-1);
    L->pop();
    return count;
};
static void hook_item_get(LuaState* L)
{
    static bool hooked = false;
    if (!hooked)
    {
        hooked = true;
        L->swap_global(xyobfstr("WorldInventoryProxy.GetItemCount"), +[](LuaState* L)
        {
            WorldInventoryProxy = L->get(1);
            int n = L->count();
            L->load_field(1, xyobfstr("__old_GetItemCount"));
            L->pop_insert(1);
            L->call(n, 1);
            L->revert_global(xyobfstr("WorldInventoryProxy.GetItemCount"));
            hooked = false;
            return 1;
        });
    }
}

static struct WorldAuto
{
private:
    bool auto_shop = false;
    bool auto_facilities = false;

public:
    void set_autobuy_energy_box(LuaState* L, bool on)
    {
        if (auto_shop == on) return;
        set_public_hook(L, on);
        if (on) hook_item_get(L);
        auto_shop = on;
    }
    void set_autouse_siren_facilities(LuaState* L, bool on)
    {
        if (auto_facilities == on) return;
        set_public_hook(L, on);
        if (on) hook_facilities(L);
        else unhook_facilities(L);
        auto_facilities = on;
    }

protected:
    int public_count = 0;
    void set_public_hook(LuaState* L, bool on)
    {
        if (on) { if (++public_count == 1) hook_public(L); }
        else { if (--public_count == 0) unhook_public(L); }
    }

private:
    LuaValue World, WorldScene, WorldMap;
    bool autofight = false, autoswitch = false;
    int siren_wait = -1;
private:
    static void hook_public(LuaState* L);
    static void unhook_public(LuaState* L);
    static void hook_facilities(LuaState* L);
    static void unhook_facilities(LuaState* L);
private:
    static void call_find_attachments(LuaState* L);
}
world_auto;

void WorldAuto::call_find_attachments(LuaState* L)
{
    L->push(world_auto.WorldMap);
    L->load_field(-1, xyobfstr("FindAttachments"));
    L->pop_insert(-2);
    L->push(22);
    L->call(2, 1);
}

void WorldAuto::hook_public(LuaState* L)
{
    L->swap_global(xyobfstr("World.TriggerAutoFight"), +[](LuaState* L)
    {
        world_auto.World = L->get(1);
        world_auto.autofight = L->to_boolean(2);
        // LOGI(">> TriggerAutoFight %s", L->format(2).c_str());
        int n = L->count();
        L->load_field(1, xyobfstr("__old_TriggerAutoFight"));
        L->pop_insert(1);
        L->call(n, 0);
        return 0;
    });

    L->swap_global(xyobfstr("WorldMap.FindAttachments"), +[](LuaState* L)
    {
        static int last_gid = -1;
        static bool check_attachments = false;

        static bool buy_energy = false;
        static LuaValue world_port_layer;

        static int siren_state = 0;
        static bool siren_moved = false;
        static int siren_row, siren_column;

        bool changed = false;
        if (int gid = L->get_field(1, xyobfstr("fields.gid")).num; gid != last_gid)
        {
            // 更新状态
            changed = true;
            last_gid = gid;
            // LOGI("gid: %d", gid);
            world_auto.WorldMap = L->get(1);

            // 重置状态
            check_attachments = true;
            buy_energy = false;
            siren_state = 0;
            siren_moved = false;
            world_auto.siren_wait = -1;

            // 清理资源
            if (!world_port_layer.is_nil())
            {
                L->push(world_port_layer);
                world_port_layer = {};
                L->load_field(-1, xyobfstr("exit"));
                L->pop_insert(-2);
                L->call(1, 0);
            }

            // L->print_to(gpath.get("WorldMap", last_gid), 1, 5, {"class","player"});
        }

        int n = L->count();
        L->load_field(1, xyobfstr("__old_FindAttachments"));
        L->pop_insert(1);
        L->call(n, 1);

        if (changed)
        {
            // LOGI(">> FindAttachments %d", last_gid);
            // L->print_to(gpath.get("Attachments", last_gid), 1, 4, {"class"});
        }

        static auto get_fleet_pos = [](LuaState* L)
        {
            int tmp = L->count();
            L->push(world_auto.WorldMap);
            L->load_field(-1, xyobfstr("fields"));
            int fleet_id = L->get_field(-1, xyobfstr("findex")).num;
            L->load_field(-1, xyobfstr("fleets"));
            L->load_field(-1, fleet_id);
            L->load_field(-1, xyobfstr("fields"));
            int row = L->get_field(-1, xyobfstr("row")).num;
            int column = L->get_field(-1, xyobfstr("column")).num;
            L->resize(tmp);
            struct Pos { int row, column; };
            return Pos{row, column};
        };

        static auto find_path = [](LuaState* L, int r1, int c1, int r2, int c2)
        {
            if (r1 == r2 && c1 == c2) return 1000;
            L->push(world_auto.WorldMap);
            L->load_field(-1, xyobfstr("FindPath"));
            L->pop_insert(-2);
            L->create_table(0, 2);
            L->store_field(-1, xyobfstr("row"), r1);
            L->store_field(-1, xyobfstr("column"), c1);
            L->create_table(0, 2);
            L->store_field(-1, xyobfstr("row"), r2);
            L->store_field(-1, xyobfstr("column"), c2);
            L->call(3, 2);
            int step = L->to_number(-2);
            L->pop(2);
            return step;
        };

        if (world_auto.auto_facilities)
        {
            int ori_siren_wait = world_auto.siren_wait;
            if (world_auto.siren_wait >= 0) --world_auto.siren_wait;
            // if (siren_state) LOGI(">> FindAttachments siren_wait %d -> %d", ori_siren_wait, world_auto.siren_wait);
        }
        if (check_attachments && last_gid > 1000000 && (world_auto.siren_wait == -1 || !world_auto.auto_facilities))
        {
            check_attachments = false;
            // 检查附着物
            bool cond_break = world_auto.public_count <= 1;
            int ori_siren_state = siren_state;
            L->push();
            while (L->next(1))
            {
                L->load_field(3, xyobfstr("fields"));
                if (L->get_field(4, xyobfstr("type")).num == 22)
                {
                    int id = L->get_field(4, xyobfstr("id")).num;

                    // 神秘商人
                    if (world_auto.auto_shop)
                    {
                        L->load_field(4, xyobfstr("config"));
                        int obstacle = L->get_field(5, xyobfstr("obstacle")).num;
                        int compass = L->get_field(5, xyobfstr("compass_index")).num;
                        if (id == 1900) { buy_energy = true; if (cond_break) break; }
                        if (obstacle == 0 && compass == 5) { buy_energy = true; if (cond_break) break; }
                    }

                    // 塞壬装置
                    if (world_auto.auto_facilities)
                    {
                        if (id == 1101 || id == 1110 || id == 1401 || id == 1502 || (ori_siren_state > 3 && id == 1501))
                        {
                            siren_row = L->get_field(4, xyobfstr("row")).num;
                            siren_column = L->get_field(4, xyobfstr("column")).num;
                        }
                        // 塞壬信息收集装置
                        if (id == 1101 && ori_siren_state == 0) { siren_state = 1; if (cond_break) break; }
                        if (id == 1110 && ori_siren_state == 0) { siren_state = 2; if (cond_break) break; }
                        // 塞壬探测装置
                        if (id == 1401 && ori_siren_state == 0) { siren_state = 3; if (cond_break) break; }
                        // 探测后产生的箱子或物资点
                        if ((id == 1502 || (ori_siren_state > 3 && id == 1501)) && (siren_state == 0 || siren_state > 3))
                        {
                            auto pos = get_fleet_pos(L);
                            int step = find_path(L, pos.row, pos.column, siren_row, siren_column);
                            if (step < 1000)
                            {
                                if (id == 1502) siren_state = 4;
                                if (id == 1501) siren_state = 5;
                                if (ori_siren_state != 0) break;
                            }
                        }
                    }
                }
                L->resize(2);
            }
            L->resize(1);

            // LOGI("  >> buy_energy=%d siren_state=%d", buy_energy, siren_state);
        }

        if (world_auto.auto_shop && buy_energy)
        {
            L->push(world_auto.WorldMap);
            L->load_field(-1, xyobfstr("fields"));
            int map_fields = L->count();
            // L->print_to(gpath.get("WorldMap - fields"), map_fields);

            L->push(world_port_layer);
            if (L->is_nil(-1))
            {
                // 创建商品界面 - 异步加载资源
                L->push(L->get_global(xyobfstr("WorldPortLayer.New")));
                L->call(0, 1);

                L->create_table(0, 2);
                L->store_field(-1, xyobfstr("page"), 2);
                L->store_field(-1, xyobfstr("isLayer"), true);
                L->store_field(-2, xyobfstr("contextData"));

                L->load_field(-1, xyobfstr("load"));
                L->push_copy(-2);
                L->call(1, 0);

                L->store_field(-1, xyobfstr("page"), -1);

                L->create_table();
                L->store_field(-1, xyobfstr("RemoveListener"), func_nil);
                L->store_field(-2, xyobfstr("atlas"));

                world_port_layer = L->get(-1);
            }
            else if (L->get_field(-1, xyobfstr("_isLoaded")).b)
            {
                if (L->get_field(-1, xyobfstr("page")).num == -1)
                {
                    // 异步加载商品
                    L->create_table();
                    L->store_field(-1, xyobfstr("mapId"), (int)L->get_field(map_fields, xyobfstr("id")).num);
                    command_execute(L, xyobfstr("WorldPortReqCommand"), xyobfcstr("world port req"));

                    L->create_table();
                    L->store_field(-2, xyobfstr("nGoodsPortDic"));

                    L->load_field(-1, xyobfstr("SetPort"));
                    L->push_copy(-2);
                    L->load_field(map_fields, xyobfstr("ports"));
                    L->load_field(-1, 1);
                    L->remove(-2);
                    L->call(2, 0);

                    L->store_field(-1, xyobfstr("page"), 2);

                    L->load_field(-1, xyobfstr("UpdateGoods"));
                    L->push_copy(-2);
                    L->call(1, 0);
                }
                else
                {
                    int tmp = L->count();
                    L->load_field(-1, xyobfstr("port"));
                    L->load_field(-1, xyobfstr("fields"));
                    L->load_field(-1, xyobfstr("goods"));
                    if (L->get_length(-1) > 0)
                    {
                        // L->print_to(gpath.get("sync_goods"), -1, 5);

                        // 遍历商品
                        HashTable<int,int> costs;
                        L->push();
                        while (L->next(-2))
                        {
                            L->load_field(-1, xyobfstr("fields"));
                            int id = L->get_field(-1, xyobfstr("id")).num;
                            int count = L->get_field(-1, xyobfstr("count")).num;
                            if (count > 0 && (id >= 200101 && id <= 200399))
                            {
                                int mid = L->get_field(-1, xyobfstr("moneyItem.id")).num;
                                int mcount = L->get_field(-1, xyobfstr("moneyItem.count")).num;
                                int& cost = costs.get(mid);
                                int icount = get_item_count(L, mid) - cost;
                                // LOGI("    >> 所需货币数量: %d, 拥有货币数量: %d", mcount, icount);

                                if (icount >= mcount)
                                {
                                    cost += mcount;

                                    L->create_table();
                                    L->push_copy(-3);
                                    L->store_field(-2, xyobfstr("goods"));
                                    command_execute(L, xyobfstr("WorldPortShoppingCommand"), xyobfcstr("world port shopping"));

                                    std::string mess = xyobfcstr("<color=#92fc63>");
                                    mess += L->get_field(-1, xyobfstr("item.cfg.name")).str->to_utf8();
                                    mess += xyobfcstr("</color>");
                                    mess += xyobfcstr("购买成功");
                                    gtips.show(L, mess.c_str());
                                }
                                else
                                {
                                    std::string mess = xyobfcstr("<color=#92fc63>补给兑换凭证</color>不足，购买失败");
                                    gtips.show(L, mess.c_str());
                                    if (world_auto.autofight)
                                    {
                                        L->push(world_auto.World);
                                        L->load_field(-1, xyobfstr("TriggerAutoFight"));
                                        L->pop_insert(-2);
                                        L->push(false);
                                        L->call(2, 0);
                                        siren_state = 0;
                                    }
                                    break;
                                }
                            }
                            L->pop(2);
                        }

                        world_port_layer = {};
                        buy_energy = false;

                        L->resize(tmp);
                        L->load_field(-1, xyobfstr("exit"));
                        L->pop_insert(-2);
                        L->call(1, 0);
                    }
                }
            }
            L->resize(1);
        }

        if (world_auto.auto_facilities && siren_state)
        {
            if (world_auto.siren_wait != -1) goto siren_over;
            static bool siren_start = false;
            if (changed) siren_start = false;

            // 等待 autofight 开始
            static bool ori_autoswitch = false;
            static bool autoend_check = false;
            if (!siren_start && world_auto.autofight)
            {
                siren_start = true;
                ori_autoswitch = world_auto.autoswitch;
                siren_moved = false;
                autoend_check = false;
                // LOGI("  >> siren_start autoswitch=%d", ori_autoswitch);
            }

            // 等待 autofight 结束
            if (siren_start && !world_auto.autofight)
            {
                if (!autoend_check)
                {
                    autoend_check = true;
                    L->push(world_auto.World);
                    L->load_field(-1, xyobfstr("HasAutoFightDrops"));
                    L->pop_insert(-2);
                    L->call(1, 1);
                    if (L->to_boolean(-1))
                    {
                        world_auto.siren_wait = 1000;
                        goto siren_over;
                    }
                }

                // LOGI(">> siren_state=%d", siren_state);

                if (siren_state <= 5)
                {
                    auto pos = get_fleet_pos(L);
                    if (!siren_moved)
                    {
                        int step = find_path(L, pos.row, pos.column, siren_row, siren_column);
                        // LOGI("  >> FindPath (%d,%d) --%d-> (%d,%d) ", pos.row, pos.column, step, siren_row, siren_column);
                        if (step >= 1000) siren_state = 0;
                        else
                        {
                            if (siren_state != 5) siren_moved = true;
                            else check_attachments = true, siren_state = 10;
                            world_auto.siren_wait = 1000;
                            L->push(world_auto.WorldScene);
                            L->load_field(-1, xyobfstr("OnClickCell"));
                            L->pop_insert(-2);
                            L->push(siren_row);
                            L->push(siren_column);
                            L->call(3, 0);
                        }
                        goto siren_over;
                    }

                    // 等待服务器回调
                    world_auto.siren_wait = 1000;
                    if (siren_state == 1) { siren_state = 2; goto siren_over; }
                    if (siren_state == 2) { siren_state = 8; goto siren_over; }

                    siren_moved = false;
                    check_attachments = true;
                    if (siren_state == 3)
                    {
                        // 等待 WorldMapOpCommand 多次回调，延时唤醒
                        make_timer(L, +[](LuaState* L)
                        {
                            static int times = 0;
                            // LOGI("$> check %d siren_wait=%d", ++times, world_auto.siren_wait);
                            if (world_auto.siren_wait > 10) world_auto.siren_wait = 10;
                            else
                            {
                                L->load_field(1, xyobfstr("Stop"));
                                L->pop_insert(1);
                                L->call(1, 0);
                                world_auto.siren_wait = -1;
                                call_find_attachments(L);
                            }
                            return 0;
                        }, 1, -1);
                    }
                    siren_state = 10;
                    goto siren_over;
                }

                siren_state = 0;

                // 恢复多图自律
                if (ori_autoswitch)
                {
                    L->push(world_auto.WorldScene);
                    L->load_field(-1,xyobfstr("StartAutoSwitch"));
                    L->pop_insert(-2);
                    L->call(1, 0);
                }
            }
        siren_over:;
            L->resize(1);
        }

        return 1;
    });
    
}
void WorldAuto::unhook_public(LuaState* L)
{
    L->revert_global(xyobfstr("World.TriggerAutoFight"));
    L->revert_global(xyobfstr("WorldMap.FindAttachments"));
}

void WorldAuto::hook_facilities(LuaState* L)
{
    L->swap_global(xyobfstr("World.TriggerAutoSwitch"), +[](LuaState* L)
    {
        world_auto.autoswitch = L->to_boolean(2);
        // LOGI(">> TriggerAutoSwitch %s", L->format(2).c_str());
        int n = L->count();
        L->load_field(1, xyobfstr("__old_TriggerAutoSwitch"));
        L->pop_insert(1);
        L->call(n, 0);
        return 0;
    });

    L->swap_global(xyobfstr("WorldScene.init"), +[](LuaState* L)
    {
        // LOGI(">> WorldScene.init");
        world_auto.WorldScene = L->get(1);
        int n = L->count();
        L->load_field(1, xyobfstr("__old_init"));
        L->pop_insert(1);
        L->call(n, 0);
        return 0;
    });
    
    L->swap_global(xyobfstr("StoryPlayer.InitBranches"), +[](LuaState* L)
    {
        // LOGI(">> InitBranches");
        int n = L->count();
        LuaValue self = L->get(1);
        LuaValue callback = L->get(4);

        L->load_field(1, xyobfstr("step"));
        std::string say = L->get_field(-1, xyobfstr("say")).str->to_utf8();
        int flag = -1, index = -1;
        // 1.启动设备-0  2.破坏设备-1  3.离开设备-2
        // 1.迎击-0     2.离开-1
        // 1.探测敌人-2  2.探测资源-1  3.重复探测敌人-4  4.重复探测资源-3  5.离开-0
        // 1.使用存储器-0  2.直接破坏-1  3.暂时离开-2
        if (world_auto.siren_wait != -1)
        {
            if (say.compare(xyobfcstr("启动设备需要消耗一个1个[塞壬能源存储器]，确定启动吗？")) == 0) flag = 1, index = 2;
            else if (say.compare(xyobfcstr("警告:设备的警报装置已启动，塞壬舰队正在接近。")) == 0) flag = 0, index = 1;
            else if (say.compare(xyobfcstr("投入特定材料使设施运作，可以探测到海域中更加隐蔽的调查点……")) == 0) flag = 3, index = 4;
            else if (say.compare(xyobfcstr("检测到塞壬的特殊资源补给。因为能源枯竭，已经无法正常开启了。不过，就算强行破坏也能回收一定材料。")) == 0) flag = 1, index = 2;
        }
        else if (world_auto.autofight)
        {
            if (say.compare(xyobfcstr("启动设备需要消耗一个1个[塞壬能源存储器]，确定启动吗？")) == 0) flag = 2, index = 3;
            else if (say.compare(xyobfcstr("警告:设备的警报装置已启动，塞壬舰队正在接近。")) == 0) flag = 1, index = 2;
            else if (say.compare(xyobfcstr("投入特定材料使设施运作，可以探测到海域中更加隐蔽的调查点……")) == 0) flag = 0, index = 5;
            else if (say.compare(xyobfcstr("检测到塞壬的特殊资源补给。因为能源枯竭，已经无法正常开启了。不过，就算强行破坏也能回收一定材料。")) == 0) flag = 2, index = 3;
        }
        L->resize(n);

        // for (int i=1; i<=n; ++i) L->print_to(gpath.get("InitBranches", i), i);
        L->load_field(1, xyobfstr("__old_InitBranches"));
        L->pop_insert(1);
        L->call(n, 0);

        if (flag >= 0)
        {
            L->push(self);
            L->push(callback);
            L->push(flag);
            L->push(index);
            L->push([](LuaState* L)
            {
                L->resize(0);
                L->push_copy(L->get_param(1));
                L->load_field(1, xyobfstr("SetBranchCode"));
                L->push_copy(1);
                L->load_field(1, xyobfstr("script"));
                L->load_field(1, xyobfstr("step"));
                L->push_copy(L->get_param(3));
                L->call(4, 0);

                L->load_field(1, xyobfstr("step"));
                L->load_field(2, xyobfstr("options"));
                L->load_field(-1, (int)L->to_number(L->get_param(4)));
                L->push_copy(L->get_param(2));
                L->load_field(-2, xyobfstr("content"));
                L->call(1, 0);

                world_auto.siren_wait = -1;
                call_find_attachments(L);

                return 0;
            }, 4);

            make_timer(L, L->get(-1), 0.5);
        }

        return 0;
    });
    
    static auto call_find_attachments = [](LuaState* L)
    {
        L->push(world_auto.WorldMap);
        L->load_field(-1, xyobfstr("FindAttachments"));
        L->pop_insert(-2);
        L->push(22);
        L->call(2, 1);
    };

    L->set_global(xyobfstr("WorldAutoFightRewardLayer.onUILoaded"), 
                L->get_global(xyobfstr("BaseUI.onUILoaded")));
    L->swap_global(xyobfstr("WorldAutoFightRewardLayer.onUILoaded"), +[](LuaState* L)
    {
        // LOGI(">> WorldAutoFightRewardLayer.onUILoaded");
        LuaValue self = L->get(1);

        int n = L->count();
        L->load_field(1, xyobfstr("__old_onUILoaded"));
        L->pop_insert(1);
        L->call(n, 0);

        if (world_auto.siren_wait != -1)
        {
            L->push(self);
            L->load_field(-1, xyobfstr("onBackPressed"));
            L->pop_insert(-2);
            L->call(1, 0);

            world_auto.siren_wait = -1;
            call_find_attachments(L);
        }
        return 0;
    });

    L->swap_global(xyobfstr("AwardInfoLayer.onUIAnimEnd"), +[](LuaState* L)
    {
        // LOGI(">> AwardInfoLayer.onUIAnimEnd %d", world_auto.siren_wait);
        LuaValue self = L->get(1);

        int n = L->count();
        L->load_field(1, xyobfstr("__old_onUIAnimEnd"));
        L->pop_insert(1);
        L->call(n, 0);

        if (world_auto.siren_wait != -1)
        {
            L->push(self);
            L->push([](LuaState* L)
            {
                L->push_copy(L->get_param(1));

                L->load_field(-1, xyobfstr("onBackPressed"));
                L->pop_insert(-2);
                L->call(1, 0);

                world_auto.siren_wait = -1;
                call_find_attachments(L);

                return 0;
            }, 1);

            make_timer(L, L->get(-1), 0.5);
        }

        return 0;
    });

    L->swap_global(xyobfstr("WorldMapOpCommand.execute"), +[](LuaState* L)
    {
        int op = L->get_field(2, xyobfstr("body.fields.op")).num;
        int old_wait = world_auto.siren_wait;
        if (world_auto.siren_wait >= 0 && world_auto.siren_wait < 100 && op == 10) world_auto.siren_wait = 100;
        // if (world_auto.siren_wait >= 0 && op == 10) LOGI(">> WorldMapOpCommand wait=%d -> %d", old_wait, world_auto.siren_wait);
        L->load_field(2, xyobfstr("body"));
        L->load_field(-1, xyobfstr("fields"));
        // L->print_to(gpath.get("WorldMapOpCommand", op), -1, 5, {"class"});
        L->resize(2);
        L->load_field(1, xyobfstr("__old_execute"));
        L->pop_insert(1);
        L->call(2, 0);
        return 0;
    });

}
void WorldAuto::unhook_facilities(LuaState* L)
{
    L->revert_global(xyobfstr("World.TriggerAutoSwitch"));
    L->revert_global(xyobfstr("WorldScene.init"));
    L->revert_global(xyobfstr("StoryPlayer.InitBranches"));
    L->revert_global(xyobfstr("WorldAutoFightRewardLayer.onUILoaded"));
    L->revert_global(xyobfstr("AwardInfoLayer.onUIAnimEnd"));
    L->revert_global(xyobfstr("WorldMapOpCommand.execute"));
}

void xyu::event_autobuy_energy_box(LuaState* L, bool on) { world_auto.set_autobuy_energy_box(L, on); }
void xyu::event_autouse_siren_facilities(LuaState* L, bool on) { world_auto.set_autouse_siren_facilities(L, on); }

void xyu::event_autouse_energy_box(LuaState* L, bool on)
{
    if (on)
    {
        hook_item_get(L);

        L->swap_global(xyobfstr("WorldStaminaManager.ChangeStamina"), +[](LuaState* L)
        {
            int stamina = L->to_number(2);
            // LOGI(">> ChangeStamina %d", stamina);
            int n = L->count();
            L->load_field(1, xyobfstr("__old_ChangeStamina"));
            L->pop_insert(1);
            L->call(n, 0);

            if (stamina < 40)
            {
                auto use_item = [](LuaState* L, int id, int count = 1)
                {
                    L->create_table();
                    L->store_field(-1, xyobfstr("itemID"), id);
                    L->store_field(-1, xyobfstr("count"), count);
                    L->create_table();
                    L->store_field(-2, xyobfstr("args"));
                    command_execute(L, xyobfstr("WorldItemUseCommand"), xyobfcstr("world item use"));
                };

                // 尝试优先级: 100体力箱 -> 50体力箱 -> 20体力箱
                std::string mess = xyobfcstr("使用<color=#92fc63>能源补给箱（");
                L->push(WorldInventoryProxy);
                if (get_item_count(L, 253)) use_item(L, 253), mess += xyobfcstr("大");
                else if (get_item_count(L, 252)) use_item(L, 252), mess += xyobfcstr("中");
                else if (int count = get_item_count(L, 251))
                {
                    if (stamina < 20 && count > 1) use_item(L, 251, 2);
                    else use_item(L, 251, 1);
                    mess += xyobfcstr("小");
                }
                else return 0;

                mess += xyobfcstr("）</color>");
                gtips.show(L, mess.c_str());
            }

            return 0;
        });
    }
    else L->revert_global(xyobfstr("WorldStaminaManager.ChangeStamina"));
}

