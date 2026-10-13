#pragma once

#include "../../utils/lua.h"
#include "../../utils/obfstr.h"
#include "../../utils/log.h"

namespace xyu
{
    // 辅助：抗函数
    inline LuaValue func_nil{[](LuaState*) -> int { return 0; }};

    // 辅助: 始终返回 true 的函数
    inline LuaValue func_always_true{[](LuaState* L) -> int { L->push(true); return 1; }};

    // 辅助：获取类型名称字符串
    inline const char* get_type_name(int type)
    {
        switch (type)
        {
        case 0: return "nil";
        case 1: return "boolean";
        case 2: return "lightuserdata";
        case 3: return "number";
        case 4: return "string";
        case 5: return "table";
        case 6: return "callable";
        case 7: return "userdata";
        case 8: return "coroutine";
        default: return "unknown";
        }
    }

    // 辅助：栈平衡保护
    struct DeepGuard
    {
        LuaState* L;
        int deep;
        DeepGuard(LuaState* L) noexcept : L(L), deep(L->count()) {}
        ~DeepGuard() noexcept { L->resize(deep); }
    };

}

namespace xyu
{
    // 辅助：游戏内提示输出
    inline struct TipsMgr
    {
    private:
        int ref = 0;
    public:
        void create(LuaState* L)
        {
            if (++ref == 1) L->swap_global(xyobfstr("pg.TipsMgr.ShowTips"), LuaValue{[](LuaState* L) -> int
            {
                int n = L->count();
                // LOGI("[%s] %s: %s", SS_EVENT.get().data(), xyobfcstr("函数被调用: ShowTips"), L->format(2).c_str());
                // L->load_global("debug");
                // L->load_field(-1, "traceback");
                // L->push("ShowTips called");
                // L->push(2.);
                // L->call(2, 1);
                // L->print(-1);
                // L->resize(n);

                if (n < 2 || L->is_nil(2)) return 0;
                L->push(L->get_global(xyobfstr("pg.TipsMgr.__old_ShowTips")));
                L->pop_insert(1);
                L->call(n, 0);
                return 0;
            }});
        }
        void destroy(LuaState* L)
        {
            if (--ref == 0) L->revert_global(xyobfstr("pg.TipsMgr.ShowTips"));
        }

        // 可以使用颜色，如: "这是<color=#92fc63>绿色</color>"
        static void show(LuaState* L, const char* mess, bool i18n = false)
        {
            L->push(L->get_global(xyobfstr("pg.TipsMgr.GetInstance")));
            L->call(0, 1);
            L->load_field(-1, xyobfstr("ShowTips"));
            L->pop_insert(-2);
            if (i18n) L->load_global(xyobfstr("i18n"));
            L->push(mess);
            if (i18n) L->call(1, 1);
            L->call(2, 0);
        }
    } gtips;

    // 辅助：核心事件
    inline struct BaseEvent
    {
        template <typename... Args>
        void emit(LuaState* L, int class_idx, const char* op_name, double type, double id, Args... args)
        {
            static_assert(sizeof...(args) <= 9);
            L->push(L->get_global(xyobfstr("BaseEventLogic.emit")));
            L->push_copy(class_idx < 0 ? class_idx - 1 : class_idx);
            L->push(op_name);
            L->create_table(0, 2 + sizeof...(args));
            L->store_field(-1, xyobfstr("type"), LuaValue{type});
            L->store_field(-1, xyobfstr("id"), LuaValue{id});
            char sarg[5] = "arg0";
            (..., (++sarg[3], L->store_field(-1, sarg, LuaValue{(double)args})));
            L->pcall(3, 0);
        }
    } gevent;

    // 辅助：路径生成
    inline struct OutPath
    {
        int times = 0;
        char buf[256];
        char prefix[64] = "output/00000";

        char* get(const char* name)
        {
            inc(prefix, 7, 5);
            sprintf(buf, "%s_%s.txt", prefix, name);
            return buf;
        }

        char* get(const char* name, int num)
        {
            inc(prefix, 7, 5);
            sprintf(buf, "%s_%s_%d.txt", prefix, name, num);
            return buf;
        }

        static void inc(char* path, int offset, int count = 3)
        {
            for (int i = offset + count - 1; i >= offset; --i)
            {
                if (++path[i] == '9' + 1) path[i] = '0';
                else break;
            }
        }
    } gpath;

}

namespace xyu
{
    // 辅助：获取代理实例
    inline void push_proxy(LuaState* L, const char* proxy)
    {
        L->load_global(xyobfstr("getProxy"));
        L->load_global(proxy);
        L->call(1, 1);
    }

    // 辅助：执行指令 (栈顶为 body:Table, 消耗)
    inline void command_execute(LuaState* L, const char* command, LuaValue name)
    {
        LuaValue body = L->get(-1); L->pop();
        L->load_global(command);
        // 获取指令函数
        L->load_field(-1, xyobfcstr("execute"));
        L->pop_insert(-2);
        // 构建第一个参数 Table{class:command, multitonKey:m02, facade:pg.m02}
        L->load_field(-1, xyobfstr("New"));
        L->pop_insert(-2);
        L->call(1, 1);
        L->store_field(-1, xyobfstr("multitonKey"), xyobfcstr("m02"));
        L->store_field(-1, xyobfstr("facade"), L->get_global(xyobfstr("pg.m02")));
        // 构建二一个参数 Table{class:pm.Notification, name:Name, body:Body}
        L->load_global(xyobfstr("pm"));
        L->load_field(-1, xyobfcstr("Notification"));
        L->remove(-2);
        L->load_field(-1, xyobfstr("New"));
        L->pop_insert(-2);
        L->call(1, 1);
        L->store_field(-1, xyobfstr("name"), name);
        L->store_field(-1, xyobfstr("body"), body);
        // 调用
        L->call(2, 0);
    }

    // 辅助：定时任务
    inline void make_timer(LuaState* L, LuaValue fn, double duration, double times = 1)
    {
        // 创建
        L->push(L->get_global(xyobfstr("Timer.New")));
        L->push(fn);
        L->push(duration);
        L->push(times);
        L->call(3, 1);
        // 启动
        L->load_field(-1, xyobfstr("Start"));
        L->pop_insert(-2);
        L->call(1, 0);
    }

    // 辅助：输出调用栈
    inline void print_stack(LuaState* L, const char* name, int print_type = 1, const char** mess = nullptr)
    {
        L->load_global(xyobfstr("debug"));
        L->load_field(-1, xyobfstr("traceback"));
        if (name && name[0])
        {
            char buf[256];
            sprintf(buf, xyobfcstr("%s called"), name);
            L->push(buf);
        }
        else L->push("");
        L->push(2.);
        L->call(2, 1);
        if (print_type & 1) L->print_to(gpath.get((std::string{name} + xyobfcstr(" traceback")).c_str()), -1);
        if (print_type & 2) L->print(-1);
        if (mess) *mess = L->to_cstring(-1);
        L->pop(2);
    }

}
