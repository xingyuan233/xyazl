#pragma once

#include "../utils/lua.h"

namespace xyu
{
    /// 开关组
    extern struct SwitchGroup
    {
        /// 开关
        struct Switch
        {
            using CallBack_t = std::function<void(LuaState*,bool)>;

            int id;                 // 标记
            bool on = false;        // 初始是否开启
            std::string name;       // 显示名称
            CallBack_t callback;    // 回调事件

            int type;
            LuaValue data;
        };

        // 初始化开关
        void init();

        // 获取所有开关
        std::vector<Switch>& get();

        // 获取单个开关 - by id
        Switch& get(int id);

        // 获取单个开关 - by name
        Switch* get(const char* name);

        // 设置开关
        void set(LuaState* L, int id, bool on);
    }
    switch_group;

}
