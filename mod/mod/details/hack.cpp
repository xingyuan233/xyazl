#include "../hack.h"

#include <dlfcn.h>
#include <fstream>
#include <string>
#include <atomic>
#include <filesystem>

#include "../event.h"
#include "../local.h"
#include "../menu.h"
#include "../../utils/config.h"
#include "../../utils/log.h"
#include "../../utils/dobby.h"

using namespace xyu;
namespace fs = std::filesystem;

// 工具宏：将混淆后的字符串转换为 Il2Cpp 可用的 Lua 字符串对象
#define xyil2str( str ) Il2CppString::from(xyobfstr( str ))

// 空函数，替换使其不执行任何操作
static int nil_func(LuaState*) { return 0; }

// 下载需要的全局表
static void download_table(LuaState* L, std::vector<std::string>&& tables)
{
    for (auto& table : tables)
    {
        L->push(L->get_global(table.c_str()));
        const char* last_dot = strrchr(table.c_str(), '.');
        std::string name = last_dot ? last_dot + 1 : table;
        L->print_to(name + ".txt", -1, 5);
        L->pop();
    }
}

// Hook 函数：在首次调用 lua_tolstring 时执行核心 Lua 脚本修改
static void* to_lstring;
static const char* (*orig_lua_tolstring)(LuaState* L, int index, int& len);
static const char* hook_tolstring(LuaState* L, int index, int& len)
{
    static std::atomic is_init = false;
    auto ret = orig_lua_tolstring(L, index, len);

    if (L && !is_init.exchange(true, std::memory_order_acq_rel))
    {
        DobbyDestroy(to_lstring);
        LOGI("[%s] %s", xyu::SS_MAIN.get().data(), xyobfcstr("hook卸载完毕: lua_tolstring"));
        orig_lua_tolstring = nullptr;
        to_lstring = nullptr;

        auto ctx = L->create_coroutine();
        LOGI("[%s] %s", xyu::SS_MAIN.get().data(), xyobfcstr("协程创建完毕"));

        LOGI("[%s] %s", xyu::SS_MAIN.get().data(), xyobfcstr("反作弊指令清空"));
        ctx->set_global(xyobfstr("CheaterMarkCommand.execute"), nil_func);

        LOGI("[%s] %s", xyu::SS_MAIN.get().data(), xyobfcstr("加载mod配置"));
        load_player_info(ctx);
        read_mod(ctx);

        LOGI("[%s] %s", xyu::SS_UI.get().data(), xyobfcstr("添加mod界面: SettingsModPanel"));
        hook_menu(ctx);

        // download_table(L, {"SettingsRedeemPanel","SettingsAdjustScreenPanle"});


        // ctx->swap_global(xyobfstr("TrackingCommand.sendProto"), LuaValue{[](LuaState* L) -> int
        // {
        //     LOGI("[%s] %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: TrackingCommand - sendProto"));
        //
        //     int n = L->count();
        //     // L->load_global("debug");
        //     // L->load_field(-1, "traceback");
        //     // L->push("TrackingCommand.sendProto called");
        //     // L->push(2.);
        //     // L->call(2, 1);
        //     // L->print(-1);
        //     // L->resize(n);
        //
        //     L->push(L->get_global(xyobfstr("TrackingCommand.__old_sendProto")));
        //     L->pop_insert(1);
        //     L->pcall(n, 0);
        //
        //     return 0;
        // }});


        L->pop(1);
    }

    return ret;
}

// Hook 函数：在 il2cpp 初始化完成后加载 Lua 相关函数并安装 lua_tolstring 的 hook
static void* init_sym;
static int(*orig_il2cpp_init)(const char*);
static int hook_il2cpp_init(const char* domain_name)
{
    DobbyDestroy(init_sym);
    LOGI("[%s] %s", xyu::SS_IL2.get().data(), xyobfcstr("hook卸载完毕: il2cpp_init"));
    init_sym = nullptr;

    int ret = orig_il2cpp_init(domain_name);

    LOGI("[%s] %s", xyu::SS_LUA.get().data(), xyobfcstr("函数开始加载: lua_xxx"));
    lua_load();
    LOGI("[%s] %s", xyu::SS_LUA.get().data(), xyobfcstr("函数加载完毕: lua_xxx"));

    to_lstring = (void*)lua_load().lua_tolstring;
    if (to_lstring)
    {
        DobbyHook(to_lstring, (void*)hook_tolstring, (void**)&orig_lua_tolstring);
        LOGI("[%s] %s", xyu::SS_MAIN.get().data(), xyobfcstr("hook安装完毕: lua_tolstring"));
    }

    orig_il2cpp_init = nullptr;
    return ret;
}

// Hook 函数：拦截 dlopen，在 libil2cpp.so 加载时安装 il2cpp_init 的 hook
static void* (*orig_dlopen)(const char* path, int flag);
static void* hook_dlopen(const char* path, int flag)
{
    void* handle = orig_dlopen(path, flag);
    if (handle && path && strstr(path, SL_IL2CPP.get()))
    {
        DobbyDestroy((void*)dlopen);
        orig_dlopen = nullptr;
        LOGI("[%s] %s", xyu::SS_SETUP.get().data(), xyobfcstr("hook卸载完毕: dlopen"));

        init_sym = dlsym(handle, xyobfstr("il2cpp_init"));
        if (!init_sym)
        {
            LOGW("[%s] %s", xyu::SS_IL2.get().data(), xyobfcstr("符号未找到: il2cpp_init"));
            return nullptr;
        }
        DobbyHook(init_sym, (void*)hook_il2cpp_init, (void**)&orig_il2cpp_init);
        LOGI("[%s] %s", xyu::SS_IL2.get().data(), xyobfcstr("hook安装完毕: il2cpp_init"));
    }
    return handle;
}

// hack_main 核心注入线程，安装 dlopen 的 hook
void xyu::hack_main()
{
    DobbyHook((void*)dlopen, (void*)hook_dlopen, (void**)&orig_dlopen);
    LOGI("[%s] %s", "Setup", xyobfcstr("hook安装完毕: dlopen"));
}
