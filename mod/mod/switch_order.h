#pragma once

#include "../utils/lua.h"

namespace xyu
{
    /// 开关组
    extern struct OrderSwitchGroup
    {
        /// 开关
        struct Switch
        {
            using CallBack_t = void(*)(LuaState*,bool);

            int id;                 // 标记
            std::string mess;       // 显示文字
            CallBack_t callback;    // 回调事件
        };

        // 初始化
        OrderSwitchGroup();

        // 获取所有开关
        std::vector<Switch>& get();

        // 获取单个开关 - by id
        Switch& get(int id);

        // 执行开关
        void deal(LuaState* L, int id, bool open);
    }
    order_switch_group;

}
