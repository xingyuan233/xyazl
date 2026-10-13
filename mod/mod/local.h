#pragma once

#include <string>

#include "../utils/hashtable.h"

namespace xyu
{
    struct LuaState;

    // 读取mod数据
    void read_mod(LuaState* L);
    // 写入mod数据
    void write_mod();
    // mod是否加载完毕
    bool is_mod_loaded();

    // 获取切换账号或服务器次数
    int get_change_gid_times();
    // 切换账号或服务器
    void update_gid(std::string server, uint64 uid);

    // 读取舰船皮肤数据
    HashTable<uint,uint>& get_ship_skins();
    // 更改舰船皮肤
    void update_ship_skin(uint ship_id, uint skin_id);

    // 读取装扮数据
    struct AttireInfo { uint avatar, chatbg, combatui; };
    AttireInfo& get_attire();
    // 更改装扮数据
    void update_attire(uint type, uint id);

}
