#include "../event.h"

#include <fstream>
#include <sstream>

#include "./etools.hpp"
#include "../local.h"
#include "../../utils/hashtable.h"
#include "../../utils/config.h"

using namespace xyu;


/* 皮肤 */

void xyu::event_uncensor(LuaState* L, bool on)
{
    if (!is_mod_loaded()) return;
    auto path = std::string{PathConfig::PUBLIC.get().data()} + xyobfcstr("localization.txt");

    std::ifstream inFile(path);
    if (!inFile.is_open())
    {
        gtips.show(L, xyobfstr("localization.txt 文件打开失败"));
        return;
    }

    // 读取文件
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(inFile, line))
    {
        size_t eqPos = line.find('=');
        if (eqPos != std::string::npos)
            line = line.substr(0, eqPos + 1) + (on ? " true" : " false");
        lines.push_back(line);
    }
    inFile.close();

    // 写回文件
    std::ofstream outFile(path);
    if (!outFile.is_open())
    {
        gtips.show(L, xyobfstr("localization.txt 文件打开失败"));
        return;
    }
    for (const auto& l : lines) outFile << l << '\n';
    outFile.close();

    if (on) gtips.show(L, xyobfstr("<color=#92fc63>反和谐</color>已启用，下次启动时生效"));
    else gtips.show(L, xyobfstr("已恢复<color=#92fc63>和谐状态</color>，下次启动时生效"));
}

void xyu::event_unlock_attire(LuaState* L, bool on)
{
    if (on)
    {
        L->swap_global(xyobfstr("AttireFrame.isOwned"), func_always_true);
        L->swap_global(xyobfstr("CombatUIStyle.isOwned"), func_always_true);

        // 限时装扮显示时会创建计时器，通过获取时间和有效期定时
        // 但对于没获取的时间为 -1 会导致立刻到期刷新状态后判断已获取重新创建计时器导致无限循环
        // 全解锁不需要计时器直接置空
        L->swap_global(xyobfstr("AttireDescPanel.AddTimer"), func_nil);

        // 更新装扮时保存到本地
        L->swap_global(xyobfstr("AttireApplyCommand.execute"), +[](LuaState* L)
        {
            // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: AttireApplyCommand.execute"));

            // 获取 type, id
            L->load_field(2, xyobfstr("body"));
            double type = L->get_field(-1, xyobfstr("type")).num;
            double id = L->get_field(-1, xyobfstr("id")).num;

            // 获取玩家数据
            push_proxy(L, xyobfstr("PlayerProxy"));
            int playerProxy = L->count();
            L->load_field(playerProxy, xyobfstr("getData"));
            L->push_copy(playerProxy);
            L->call(1, 1);
            int player = L->count();

            // 更新装扮
            L->load_field(player, xyobfstr("updateAttireFrame"));
            L->push_copy(player);
            L->push(type);
            L->push(id);
            L->call(3, 0);

            // 更新玩家数据
            L->load_field(playerProxy, xyobfstr("updatePlayer"));
            L->push_copy(playerProxy);
            L->push_copy(player);
            L->call(2, 0);

            // 输出成功提示
            gtips.show(L, xyobfstr("dress_up_success"), true);

            // 本地存储
            update_attire(type, id);

            return 0;
        });

        // 从本地加载装扮函数
        static auto load_attire = [](LuaState* L)
        {
            auto& arrire = get_attire();
            if (!arrire.avatar && !arrire.chatbg && !arrire.combatui) return;

            DeepGuard guard{L};

            // 获取玩家数据
            push_proxy(L, xyobfstr("PlayerProxy"));
            int playerProxy = L->count();
            L->load_field(playerProxy, xyobfstr("getData"));
            L->push_copy(playerProxy);
            L->call(1, 1);
            int player = L->count();

            // 更新头像
            if (arrire.avatar)
            {
                L->load_field(player, xyobfstr("updateAttireFrame"));
                L->push_copy(player);
                L->push(2);
                L->push(arrire.avatar);
                L->call(3, 0);
            }

            // 更新聊天气泡
            if (arrire.chatbg)
            {
                L->load_field(player, xyobfstr("updateAttireFrame"));
                L->push_copy(player);
                L->push(3);
                L->push(arrire.chatbg);
                L->call(3, 0);
            }

            // 更新战斗界面
            if (arrire.combatui)
            {
                L->load_field(player, xyobfstr("updateAttireFrame"));
                L->push_copy(player);
                L->push(4);
                L->push(arrire.combatui);
                L->call(3, 0);
            }

            // 更新玩家数据
            L->load_field(playerProxy, xyobfstr("updatePlayer"));
            L->push_copy(playerProxy);
            L->push_copy(player);
            L->call(2, 0);
        };

        // 装扮初始化时通过本地装扮数据加载 (该函数会且仅会在登录进入主界面后被调用若干次)
        L->swap_global(xyobfstr("AttireProxy.updateAttireFrame"), +[](LuaState* L)
        {
            // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: AttireProxy.updateAttireFrame"));
            int n = L->count();
            L->push(L->get_global(xyobfstr("AttireProxy.__old_updateAttireFrame")));
            L->pop_insert(1);
            L->call(n, 0);

            static int cgid = 0;
            int new_cgid = get_change_gid_times();
            if (new_cgid != cgid)
            {
                cgid = new_cgid;
                load_attire(L);
            }
            return 0;
        });

        // 中途打开则重新加载装扮
        if (is_mod_loaded()) load_attire(L);
    }
    else
    {
        L->revert_global(xyobfstr("AttireFrame.isOwned"));
        L->revert_global(xyobfstr("CombatUIStyle.isOwned"));
        L->revert_global(xyobfstr("AttireDescPanel.AddTimer"));
        L->revert_global(xyobfstr("AttireApplyCommand.execute"));
        L->revert_global(xyobfstr("AttireProxy.updateAttireFrame"));
    }
}

void xyu::event_unlock_ship_skin(LuaState* L, bool on)
{
    if (on)
    {
        // 设置所有类型皮肤无需隐藏 (默认:-1, 普通:0, 誓约:1, 重制版:2, 旧版:3, 限时:5, TB:6，默认隐藏级别大于4)
        L->swap_global(xyobfstr("ShipSkin.SKIN_TYPE_NOT_HAVE_HIDE"), 10);

        // 获取所有拥有皮肤 (商店页面的查看所有皮肤会通过 GetOwnSkins 获取，能显示实际拥有的皮肤)
        L->swap_global(xyobfstr("ShipSkinProxy.getSkinList"), +[](LuaState* L)
        {
            // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: ShipSkinProxy.getSkinList"));

            static int gid = 0;
            int new_gid = get_change_gid_times();
            // 刚登录 pg 尚未加载
            if (gid != new_gid)
            {
                gid = new_gid;
                int n = L->count();
                L->load_field(1, xyobfcstr("__old_getSkinList"));
                L->pop_insert(1);
                L->call(n, 1);
            }
            // 否则直接返回所有皮肤
            else L->push(L->get_global(xyobfstr("pg.ship_skin_template.all")));

            return 1;
        });

        // 获取舰船能使用的所有皮肤 (不包括共享皮肤)
        L->swap_global(xyobfstr("ShipSkinProxy.GetAllSkinForShip"), +[](LuaState* L)
        {
            // 防止原调用后过滤掉未改造角色的改造皮肤
            L->push(L->get_global(xyobfstr("ShipGroup.getSkinList")));
            L->load_field(2, xyobfstr("groupId"));
            L->call(1, 1);
            return 1;
        });

        // 解除共享皮肤需要的好感度限制
        L->swap_global(xyobfstr("ShipSkin.CanUseShareSkinForShip"), func_always_true);

        // 共享皮肤拓展
        static struct ShipMapping
        {
            std::vector<std::vector<uint>> shared;
            HashTable<uint,uint> map;

            ShipMapping()
            {
                // 原本仅支持 μ兵装/二型船 到 原始 的单向映射
                // shared 拓展为 {原始, μ.μ兵装，II.二型船，M.Meta}
                // 来源 原始/μ/II:_G.pg.ship_data_template M:
                // 更新日期 2026.9.4 维护后，按照共享组中最新的实装日期降序排序
                auto add_shared_group = [&](const std::vector<uint>& groups)
                {
                    uint index = shared.size();
                    shared.push_back(groups);
                    for (auto group : groups) map.get(group) = index;
                };

                // 萨拉托加 [No.74, ?M.64]
                add_shared_group({101253, });
                // 纽伦堡 [No.486, ?M.63]
                add_shared_group({105204, });
                // 布里斯托尔 [No.533, M.62]
                add_shared_group({10149, 970112});
                // 柯尼斯堡 [No.238, M.60]
                add_shared_group({40201, 970212});
                // 易北 [No.526, M.61]
                add_shared_group({40602, 970605});
                // 科隆 [No.240, M.59]
                add_shared_group({40203, 970210});
                // 甘古特 [No.437, M.57]
                add_shared_group({70501, 970510});
                // 龙骑兵 [No.415, M.56]
                add_shared_group({60102, 970111});
                // 克利夫兰 [No.37, μ.420, M.55]
                add_shared_group({10209, 10224, 970209});
                // 列克星敦 [No.73, II.715]
                add_shared_group({10702, 10716});
                // 约克城 [No.76, II.574, M.54]
                add_shared_group({10705, 10710, 970708});
                // 博伊西 [No.514, μ.638, M.53]
                add_shared_group({10229, 10232, 970208});
                // 杜威 [No.4, M.52]
                add_shared_group({10102, 970110});
                // 夕立 [No.164, M.51]
                add_shared_group({30114, 970109});
                // 敦刻尔克 [No.352, M.50]
                add_shared_group({90401, 970406});
                // 江风 [No.288, M.49]
                add_shared_group({30149, 970108});
                // 光荣 [No.148, M.48]
                add_shared_group({20706, 970707});
                // 格伦维尔 [No.93, M.47]
                add_shared_group({20113, 970107});
                // 内华达 [No.52, M.46]
                add_shared_group({10501, 970509});
                // 大黄蜂 [No.78, II.572, M.45]
                add_shared_group({10707, 10712, 970706});
                // 安德烈亚·多利亚 [No.595, M.44]
                add_shared_group({60507, 970508});
                // 博尔扎诺 [No.560, M.43]
                add_shared_group({60305, 970306});
                // 希佩尔海军上将 [No.242, μ.422, M.42]
                add_shared_group({40301, 40307, 970305});
                // 大凤 [No.230, μ.474, M.41]
                add_shared_group({30707, 30711, 970705});
                // 朱利奥·凯撒 [No.412, M.40]
                add_shared_group({60506, 970507});
                // 凤翔 [No.220, M.39]
                add_shared_group({30603, 970604});
                // 长门 [No.212, M.37]
                add_shared_group({30505, 970506});
                // 霞 [No.424, M.36]
                add_shared_group({30181, 970106});
                // 威奇塔 [No.49, M.35]
                add_shared_group({10311, 970304});
                // 福煦 [No.524, M.34]
                add_shared_group({90301, 970303});
                // 贝亚恩 [No.458, M.33]
                add_shared_group({80701, 970704});
                // 吸血鬼 [No.103, M.32]
                add_shared_group({20123, 970105});
                // 罗德尼 [No.133, M.31]
                add_shared_group({20504, 970505});
                // 水星纪念 [No.434, M.30]
                add_shared_group({70202, 970207});
                // 金伯利 [No.374, M.29]
                add_shared_group({10138, 970104});
                // 基洛夫 [No.435, M.28]
                add_shared_group({70204, 970206});
                // 黑暗界 [No.149, M.27]
                add_shared_group({21301, 971301});
                // 普林斯顿 [No.473, M.26]
                add_shared_group({10723, 970603});
                // 神通 [No.309, M.25]
                add_shared_group({30213, 970205});
                // 阿尔及利亚 [No.455, M.23]
                add_shared_group({90302, 970302});
                // 女灶神 [No.80, M.22]
                add_shared_group({11201, 971201});
                // U-556 [No.386, M.21]
                add_shared_group({40804, 970801});
                // 拉·加利索尼埃 [No.456, M.20]
                add_shared_group({90201, 970204});
                // 伊丽莎白女王 [No.130, M.19]
                add_shared_group({20501, 970504});
                // 谢菲尔德 [No.111, μ.421, M.18]
                add_shared_group({20208, 20225, 970203});
                // 亚利桑那 [No.55, M.17]
                add_shared_group({10504, 970503});
                // 命运女神 [No.92, M.16]
                add_shared_group({20112, 970102});
                // 猎人 [No.100, M.15]
                add_shared_group({20120, 970101});
                // 声望 [No.127, M.14]
                add_shared_group({20401, 970404});
                // 特伦托 [No.414, M.13]
                add_shared_group({60301, 970301});
                // 反击 [No.128, M.12]
                add_shared_group({20402, 970403});
                // 孟菲斯 [No.360, M.11]
                add_shared_group({10216, 970202});
                // 山城 [No.209, M.10]
                add_shared_group({30502, 970502});
                // 沙恩霍斯特 [No.248, M.9]
                add_shared_group({40401, 970402});
                // 隼鹰 [No.219, M.8]
                add_shared_group({30602, 970602});
                // 格奈森瑙 [No.249, M.7]
                add_shared_group({40402, 970401});
                // 飞鹰 [No.218, M.6]
                add_shared_group({30601, 970601});
                // 扶桑 [No.208, M.5]
                add_shared_group({30501, 970501});
                // 苍龙 [No.226, M.4]
                add_shared_group({30703, 970703});
                // 海伦娜 [No.33, M.3]
                add_shared_group({10205, 970201});
                // 皇家方舟 [No.144, M.2]
                add_shared_group({20702, 970702});
                // 飞龙 [No.227, M.1]
                add_shared_group({30704, 970701});
                // 拉菲 [No.19, II.615]
                add_shared_group({10117, 10151});
                // 哈曼 [No.27, II.576]
                add_shared_group({10125, 10150});
                // 北安普敦 [No.41, II.575]
                add_shared_group({10303, 10326});
                // 兰利 [No.72, II.573]
                add_shared_group({10701, 10727});
                // 休斯敦 [No.43, II.609]
                add_shared_group({10305, 10231});
                // 黛朵 [No.107, μ.476]
                add_shared_group({20204, 20228});
                // 赤城 [No.224, μ.419]
                add_shared_group({30701, 30710});
                // 加斯科涅 [P.12, μ.418]
                add_shared_group({99901, 90503});
                // 可畏 [No.147, μ.633]
                add_shared_group({20705, 20714});
                // 欧根亲王 [No.244, μ.634]
                add_shared_group({40303, 40315});
                // 能代 [No.322, μ.635]
                add_shared_group({30221, 30224});
                // 鲁莽 [No.359, μ.637]
                add_shared_group({80103, 80109});
                // 巴尔的摩 [No.50, μ.478]
                add_shared_group({10316, 10325});
                // 光辉 [No.145, μ.480]
                add_shared_group({20703, 20711});
                // 恶毒 [No.394, μ.481]
                add_shared_group({90111, 90112});
                // 塔什干 [No.433, μ.475]
                add_shared_group({70104, 70105});
                // 罗恩 [P.5, μ.479]
                add_shared_group({49901, 40308});
                // 大青花鱼 [No.358, μ.477]
                add_shared_group({10802, 10805});
                // 金刚 [No.204, μ.636]
                add_shared_group({30401, 30408});
            }
        } smap;
        L->swap_global(xyobfstr("ShipSkinProxy.GetShareSkinsForShipGroup"), +[](LuaState* L)
        {
            // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: ShipSkinProxy.GetShareSkinsForShipGroup"));

            uint group = L->to_number(2);
            // 存在则使用拓展的共享皮肤
            if (uint* ip = smap.map.find(group))
            {
                auto& shared = smap.shared[*ip];
                if (shared.size() > 2) L->create_table();
                int index = 0;
                for (uint g : shared)
                {
                    if (g == group) continue;
                    L->push(L->get_global(xyobfstr("ShipGroup.getSkinList")));
                    L->push((double)g);
                    L->call(1, 1);
                    if (shared.size() > 2)
                    {
                        L->push();
                        while (L->next(-2)) L->store_field(-4, ++index);
                        L->pop();
                    }
                }
            }
            // 否则返回空表
            else L->create_table();
            return 1;
        });

        // 更新皮肤时保存到本地
        L->swap_global(xyobfstr("SetShipSkinCommand.execute"), +[](LuaState* L)
        {
            // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: SetShipSkinCommand.execute"));

            // 获取 shipId, skinId
            L->load_field(2, xyobfstr("body"));
            double skin_id = L->get_field(-1, xyobfstr("skinId")).num;
            uint ori_skin_id = skin_id;
            double ship_id = L->get_field(-1, xyobfstr("shipId")).num;

            // 获取船坞数据的代理对象
            push_proxy(L, xyobfstr("BayProxy"));
            int bayProxy = L->count();

            // 通过 skin_id 获取 ship对象
            L->load_field(bayProxy, xyobfstr("getShipById"));
            L->push_copy(bayProxy);
            L->push(ship_id);
            L->call(2, 1);
            int ship = L->count();

            // 将 skin_id 设置到 ship对象上
            if (skin_id == 0)
            {
                // 对于 skin_id == 0，获取 ship 对应的默认皮肤 id
                L->load_field(ship, xyobfstr("getConfig"));
                L->push_copy(ship);
                L->push(xyobfstr("skin_id"));
                L->call(2, 1);
            }
            else L->push(skin_id);
            L->store_field(ship, xyobfstr("skinId"));

            // 将 ship对象 更新到 船坞
            L->load_field(bayProxy, xyobfstr("updateShip"));
            L->push_copy(bayProxy);
            L->push_copy(ship);
            L->call(2, 0);

            // 通知皮肤更新
            push_proxy(L, xyobfstr("PlayerProxy"));
            L->load_field(-1, xyobfstr("sendNotification"));
            L->pop_insert(-2);
            L->load_global(xyobfstr("SetShipSkinCommand"));
            L->load_field(-1, xyobfstr("SKIN_UPDATED"));
            L->remove(-2);
            L->create_table(0, 1);
            L->push_copy(ship);
            L->store_field(-2, xyobfstr("ship"));
            L->call(3, 0);

            // 输出成功提示
            gtips.show(L, xyobfstr("ship_set_skin_success"), true);

            // 本地存储
            update_ship_skin(ship_id, ori_skin_id);

            return 0;
        });

        // 舰船初始化时通过本地皮肤数据加载
        L->swap_global(xyobfstr("Ship.Ctor"), +[](LuaState* L)
        {
            // LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: Ship.Ctor"));
            int n = L->count();
            LuaValue self = L->get(1);
            L->load_field(1, xyobfcstr("__old_Ctor"));
            L->pop_insert(1);
            L->call(n, 0);

            // 还原皮肤 (新获得角色暂无 id 字段)
            L->push(self);
            if (L->load_field(1, xyobfstr("id"), false))
            {
                uint* skin_id_p = get_ship_skins().find(L->to_number(-1));
                if (skin_id_p) L->store_field(1, xyobfstr("skinId"), *skin_id_p);
            }
            return 0;
        });

        // 中途打开则重新加载皮肤
        if (is_mod_loaded())
        {
            auto& ship_skins = get_ship_skins();
            if (ship_skins.count())
            {
                DeepGuard guard{L};

                // 获取船坞数据的代理对象
                push_proxy(L, xyobfstr("BayProxy"));
                int bayProxy = L->count();

                bool flush = false;
                for (auto& kv : ship_skins)
                {
                    // 通过 shipId 获取 ship对象
                    L->load_field(bayProxy, xyobfstr("getShipById"));
                    L->push_copy(bayProxy);
                    L->push(kv.key);
                    L->call(2, 1);
                    int ship = L->count();

                    // 舰船已失效，删除
                    if (L->is_nil(-1)) { kv.val = 0; flush = true; continue; }

                    // 更新皮肤
                    L->store_field(ship, xyobfstr("skinId"), kv.val);
                    L->load_field(bayProxy, xyobfstr("updateShip"));
                    L->push_copy(bayProxy);
                    L->push_copy(ship);
                    L->call(2, 0);
                }
                if (flush) update_ship_skin(0, 0);
            }
        }
    }
    else
    {
        L->revert_global(xyobfstr("ShipSkin.SKIN_TYPE_NOT_HAVE_HIDE"));
        L->revert_global(xyobfstr("ShipSkinProxy.getSkinList"));
        L->revert_global(xyobfstr("ShipSkinProxy.GetAllSkinForShip"));
        L->revert_global(xyobfstr("ShipSkin.CanUseShareSkinForShip"));
        L->revert_global(xyobfstr("ShipSkinProxy.GetShareSkinsForShipGroup"));
        L->revert_global(xyobfstr("SetShipSkinCommand.execute"));
        L->revert_global(xyobfstr("Ship.Ctor"));
    }
}

void xyu::event_unlock_cryptolalia(LuaState* L, bool on)
{
    if (on) L->swap_global(xyobfstr("Cryptolalia.GetState"), +[](LuaState* L)
    {
        int n = L->count();
        L->store_field(1, xyobfstr("lock"), false);
        L->load_field(1, xyobfcstr("__old_GetState"));
        L->pop_insert(1);
        L->call(n, 1);
        return 1;
    });
    else L->revert_global(xyobfstr("Cryptolalia.GetState"));
}
