#include "../menu.h"

#include "../switch.h"
#include "../../utils/log.h"
#include "../../utils/obfstr.h"
#include "../../utils/il2.h"
#include "./etools.hpp"
#include "../switch_order.h"

using namespace xyu;

// 工具宏：将混淆后的字符串转换为 Il2Cpp 可用的 Lua 字符串对象
#define xyil2str( str ) Il2Cpp::make_string(xyobfstr( str ))


void xyu::hook_menu(LuaState* L)
{
    DeepGuard guard{L};

    L->load_global(xyobfstr("SettingsOtherPanel"));
    L->push_table_copy(-1, {xyobfcstr("super")});
    L->store_field(-1, xyobfstr("GetTitleEn"), +[](LuaState* L)
    {
        L->push(xyobfstr("MOD SETTINGS"));
        return 1;
    });
    L->store_field(-1, xyobfstr("GetDefaultValue"), +[](LuaState* L)
    {
        int id = L->get_field(2, "id").num;
        L->push(switch_group.get(id).on);
        return 1;
    });
    L->store_field(-1, xyobfstr("OnItemSwitch"), +[](LuaState* L)
    {
        int id = L->get_field(2, "id").num;
        switch_group.set(L, id, L->to_boolean(3));
        return 0;
    });

    static auto appearance_panel = xyobflstr("SettingsModAppearancePanel");
    L->push_table_copy(-1, {xyobfcstr("super")});
    L->store_field(-1, xyobfstr("__cname"), appearance_panel.get().data());
    L->store_field(-1, xyobfstr("GetTitle"), +[](LuaState* L)
    {
        L->push(xyobfstr("资源解锁"));
        return 1;
    });
    L->store_field(-1, xyobfstr("New"), +[](LuaState* L)
    {
        L->push(L->get_global(xyobfstr("SettingsOtherPanel.New")));
        L->pop_insert(1);
        L->call(1, 1);
        L->load_global(appearance_panel.get());
        L->push_copy(2);
        L->store_field(1, xyobfstr("class"));
        L->store_metatable(1);
        return 1;
    });
    L->store_field(-1, xyobfstr("GetList"), +[](LuaState* L)
    {
        L->create_table();
        int tid = L->count();
        int index = 1;
        for (auto& swich : switch_group.get())
        {
            if (swich.type != 1) continue;
            L->create_table();
            L->store_field(-1, xyobfstr("id"), swich.id);
            L->store_field(-1, xyobfstr("type"), 0);
            L->store_field(-1, xyobfstr("title"), swich.name.c_str());
            L->store_field(-1, xyobfstr("name"), "");
            L->store_field(-1, xyobfstr("default"), 0);
            L->store_field(tid, index);
            ++index;
        }
        return 1;
    });
    L->store_global(appearance_panel.get());

    static auto combat_panel = xyobflstr("SettingsModCombatPanel");
    L->push_table_copy(-1, {xyobfcstr("super")});
    L->store_field(-1, xyobfstr("__cname"), appearance_panel.get().data());
    L->store_field(-1, xyobfstr("GetTitle"), +[](LuaState* L)
    {
        L->push(xyobfstr("战斗优化"));
        return 1;
    });
    L->store_field(-1, xyobfstr("New"), +[](LuaState* L)
    {
        L->push(L->get_global(xyobfstr("SettingsOtherPanel.New")));
        L->pop_insert(1);
        L->call(1, 1);
        L->load_global(combat_panel.get());
        L->push_copy(2);
        L->store_field(1, xyobfstr("class"));
        L->store_metatable(1);
        return 1;
    });
    L->store_field(-1, xyobfstr("GetList"), +[](LuaState* L)
    {
        L->create_table();
        int tid = L->count();
        int index = 1;
        for (auto& swich : switch_group.get())
        {
            if (swich.type != 2) continue;
            L->create_table();
            L->store_field(-1, xyobfstr("id"), swich.id);
            L->store_field(-1, xyobfstr("type"), 0);
            L->store_field(-1, xyobfstr("title"), swich.name.c_str());
            L->store_field(-1, xyobfstr("name"), "");
            L->store_field(-1, xyobfstr("default"), 0);
            L->store_field(tid, index);
            ++index;
        }
        return 1;
    });
    L->store_global(combat_panel.get());

    static auto auto_panel = xyobflstr("SettingsModAutoPanel");
    L->push_table_copy(-1, {xyobfcstr("super")});
    L->store_field(-1, xyobfstr("__cname"), appearance_panel.get().data());
    L->store_field(-1, xyobfstr("GetTitle"), +[](LuaState* L)
    {
        L->push(xyobfstr("自律作战"));
        return 1;
    });
    L->store_field(-1, xyobfstr("New"), +[](LuaState* L)
    {
        L->push(L->get_global(xyobfstr("SettingsOtherPanel.New")));
        L->pop_insert(1);
        L->call(1, 1);
        L->load_global(auto_panel.get());
        L->push_copy(2);
        L->store_field(1, xyobfstr("class"));
        L->store_metatable(1);
        return 1;
    });
    L->store_field(-1, xyobfstr("GetList"), +[](LuaState* L)
    {
        L->create_table();
        int tid = L->count();
        int index = 1;
        for (auto& swich : switch_group.get())
        {
            if (swich.type != 3) continue;
            L->create_table();
            L->store_field(-1, xyobfstr("id"), swich.id);
            L->store_field(-1, xyobfstr("type"), 0);
            L->store_field(-1, xyobfstr("title"), swich.name.c_str());
            L->store_field(-1, xyobfstr("name"), "");
            L->store_field(-1, xyobfstr("default"), 0);
            L->store_field(tid, index);
            ++index;
        }
        return 1;
    });
    L->store_global(auto_panel.get());

    static auto value_panel = xyobflstr("SettingsModValuePanel");
    L->push_table_copy(-1, {xyobfcstr("super")});
    L->store_field(-1, xyobfstr("__cname"), value_panel.get().data());
    L->store_field(-1, xyobfstr("GetTitle"), +[](LuaState* L)
    {
        L->push(xyobfstr("数值调整"));
        return 1;
    });
    L->store_field(-1, xyobfstr("New"), +[](LuaState* L)
    {
        L->push(L->get_global(xyobfstr("SettingsOtherPanel.New")));
        L->pop_insert(1);
        L->call(1, 1);
        L->load_global(value_panel.get());
        L->push_copy(2);
        L->store_field(1, xyobfstr("class"));
        L->store_metatable(1);
        return 1;
    });
    L->store_field(-1, xyobfstr("GetList"), +[](LuaState* L)
    {
        L->create_table();
        int tid = L->count();
        int index = 1;
        for (auto& swich : switch_group.get())
        {
            if (swich.type != 4) continue;
            L->create_table();
            L->store_field(-1, xyobfstr("id"), swich.id);
            L->store_field(-1, xyobfstr("type"), 0);
            L->store_field(-1, xyobfstr("title"), swich.name.c_str());
            L->store_field(-1, xyobfstr("name"), "");
            L->store_field(-1, xyobfstr("default"), 0);
            L->store_field(tid, index);
            ++index;
        }
        return 1;
    });
    L->store_field(-1, xyobfstr("UpdateItem"), +[](LuaState* L)
    {
        int n = L->count();
        L->load_field(1, xyobfstr("list"));
        L->load_field(-1, (int)L->to_number(2));
        switch_group.get(L->get_field(-1, xyobfstr("id")).num).data = L->get(3);
        L->resize(n);

        L->push(L->get_global(xyobfstr("SettingsOtherPanel.UpdateItem")));
        L->pop_insert(1);
        L->call(n, 0);
        return 0;
    });
    L->store_global(value_panel.get());

    static auto misc_panel = xyobflstr("SettingsModMiscPanel");
    L->push_table_copy(-1, {xyobfcstr("super")});
    L->store_field(-1, xyobfstr("__cname"), appearance_panel.get().data());
    L->store_field(-1, xyobfstr("GetTitle"), +[](LuaState* L)
    {
        L->push(xyobfstr("杂项"));
        return 1;
    });
    L->store_field(-1, xyobfstr("New"), +[](LuaState* L)
    {
        L->push(L->get_global(xyobfstr("SettingsOtherPanel.New")));
        L->pop_insert(1);
        L->call(1, 1);
        L->load_global(misc_panel.get());
        L->push_copy(2);
        L->store_field(1, xyobfstr("class"));
        L->store_metatable(1);
        return 1;
    });
    L->store_field(-1, xyobfstr("GetList"), +[](LuaState* L)
    {
        L->create_table();
        int tid = L->count();
        int index = 1;
        for (auto& swich : switch_group.get())
        {
            if (swich.type != 5) continue;
            L->create_table();
            L->store_field(-1, xyobfstr("id"), swich.id);
            L->store_field(-1, xyobfstr("type"), 0);
            L->store_field(-1, xyobfstr("title"), swich.name.c_str());
            L->store_field(-1, xyobfstr("name"), "");
            L->store_field(-1, xyobfstr("default"), 0);
            L->store_field(tid, index);
            ++index;
        }
        return 1;
    });
    L->store_global(misc_panel.get());

    static auto order_panel = xyobflstr("SettingsModOrderPanel");
    L->push_table_copy(-1, {xyobfcstr("super")});
    L->store_field(-1, xyobfstr("__cname"), order_panel.get().data());
    L->store_field(-1, xyobfstr("GetTitle"), +[](LuaState* L)
    {
        L->push(xyobfstr("Mod指令"));
        return 1;
    });
    L->store_field(-1, xyobfstr("New"), +[](LuaState* L)
    {
        L->push(L->get_global(xyobfstr("SettingsOtherPanel.New")));
        L->pop_insert(1);
        L->call(1, 1);
        L->load_global(order_panel.get());
        L->push_copy(2);
        L->store_field(1, xyobfstr("class"));
        L->store_metatable(1);
        return 1;
    });
    L->store_field(-1, xyobfstr("GetList"), +[](LuaState* L)
    {
        L->create_table();
        int tid = L->count();
        int index = 1;
        for (auto& swich : order_switch_group.get())
        {
            L->create_table();
            L->store_field(-1, xyobfstr("id"), swich.id);
            L->store_field(-1, xyobfstr("type"), 0);
            L->store_field(-1, xyobfstr("title"), swich.mess.c_str());
            L->store_field(-1, xyobfstr("name"), "");
            L->store_field(-1, xyobfstr("default"), 0);
            L->store_field(tid, index);
            ++index;
        }
        return 1;
    });
    L->store_field(-1, xyobfstr("OnItemSwitch"), +[](LuaState* L)
    {
        int id = L->get_field(2, "id").num;
        order_switch_group.deal(L, id, L->to_boolean(3));
        return 0;
    });
    L->store_field(-1, xyobfstr("GetDefaultValue"), func_always_true);
    L->store_global(order_panel.get());

    // 构建 Mod 面板组: SettingsModPanel
    static auto mod_panel = xyobflstr("SettingsModPanel");
    int index = 0;
    L->create_table();
    L->store_field(-1, ++index, L->get_global(appearance_panel.get()));
    L->store_field(-1, ++index, L->get_global(combat_panel.get()));
    L->store_field(-1, ++index, L->get_global(auto_panel.get()));
    L->store_field(-1, ++index, L->get_global(value_panel.get()));
    L->store_field(-1, ++index, L->get_global(misc_panel.get()));
    L->store_field(-1, ++index, L->get_global(order_panel.get()));
    L->store_global(mod_panel.get());


    // 寄生到原始设置页面
    L->swap_global(xyobfstr("SettingsOptionPage.GetPanels"), +[](LuaState* L)
    {
        // LOGI("[%s] %s", xyu::SS_UI.get().data(), xyobfcstr("函数被调用: SettingsOptionPage - GetPanels"));

        // 调用原始 GetPanels
        L->load_field(1, xyobfstr("__old_GetPanels"));
        L->pop_insert(1);
        L->call(1, 1);

        // 获取mod面板
        L->load_global(mod_panel.get());
        int mod_count = L->get_length(2);

        // 向后移动留出mod位置
        int len = L->get_length(1);
        for (int i = len; i >= 1; --i)
        {
            L->load_field(1, i);
            L->store_field(1, i + mod_count);
        }

        // 写入 Mod 界面
        for (int i = 1; i <= mod_count; ++i)
        {
            L->load_field(2, i);
            L->store_field(1, i);
        }
        L->resize(1);

        return 1;
    });
}
