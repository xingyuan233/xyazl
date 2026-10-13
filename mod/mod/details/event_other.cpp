#include <sstream>

#include "../event.h"

#include "./etools.hpp"
#include "../local.h"

using namespace xyu;


static void test(LuaState* L)
{
    L->swap_global(xyobfstr("BaseEventLogic.emit"), +[](LuaState* L)
    {
        const char* mess;
        print_stack(L, "", 0, &mess);
        bool flag = strstr(mess, "LevelAmbushView");
        if (flag) LOGI("<emit>");
        int n = L->count();
        if (flag) for (int i=1; i<=n; ++i) L->print_to(gpath.get("emit", i), i);
        L->push(L->get_global(xyobfstr("BaseEventLogic.__old_emit")));
        L->pop_insert(1);
        L->pcall(n, 0);
        if (flag) LOGI("<emit/>");
        return 0;
    });
}


// 辅助：玩家uid 及 服务器名称 存储
static std::string server;
static uint64 uid;

void xyu::load_player_info(LuaState* L)
{
    L->swap_global(xyobfstr("ServerLoginCommand.execute"), +[](LuaState* L)
    {
        // 获取服务器id
        int n = L->count();
        L->load_field(2, xyobfstr("body"));
        L->load_field(-1, xyobfstr("name"));
        server = L->to_cstring(-1);
        LOGI("[%s] %s: %s", SS_EVENT.get().data(), xyobfcstr("服务器名称"), server.c_str());
        L->resize(n);

        // 执行登录
        L->load_field(1, xyobfcstr("__old_execute"));
        L->pop_insert(1);
        L->call(n, 0);

        // hook Player.Ctor 获取 uid
        static bool first = true;
        static LuaValue hook_ctor{[](LuaState* L)
        {
            L->set_global(xyobfstr("Player.Ctor"), L->get_global(xyobfstr("Player.__old_Ctor")));
            LuaValue self = L->get(-1);
            int n = L->count();
            L->load_field(1, xyobfcstr("__old_Ctor"));
            L->pop_insert(1);
            L->call(n, 0);

            L->push(self);
            L->load_field(-1, xyobfstr("id"));
            uid = L->to_number(-1);
            LOGI("[%s] %s: %ld", SS_EVENT.get().data(), xyobfcstr("玩家uid"), (unsigned long)uid);
            update_gid(server, uid);
            return 0;
        }};
        if (first) { L->swap_global(xyobfstr("Player.Ctor"), hook_ctor); first = false; }
        else L->set_global(xyobfstr("Player.Ctor"), hook_ctor);
        return 0;
    });
}


// 替换聊天框
void xyu::event_replace_chat_box(LuaState* L, bool on)
{
    if (on)
    {
        L->set_global(xyobfstr("pg.gametip.notice_input_desc.tip"), xyobfcstr("请输入指令"));

        auto hook_execute = +[](LuaState* L)
        {
            L->load_field(2, xyobfstr("body"));
            execute_self_command(L, L->to_cstring(-1));
            return 0;
        };
        L->swap_global(xyobfstr("GuildSendMsgCommand.execute"), hook_execute);
        L->swap_global(xyobfstr("SendMsgCommand.execute"), hook_execute);

        // 防止指令被当作违禁词屏蔽
        L->swap_global(xyobfstr("wordVer"), +[](LuaState* L)
        {
            L->push(0);
            L->push_copy(1);
            return 2;
        });
    }
    else
    {
        L->set_global(xyobfstr("pg.gametip.notice_input_desc.tip"), LuaValue{});
        L->revert_global(xyobfstr("GuildSendMsgCommand.execute"));
        L->revert_global(xyobfstr("SendMsgCommand.execute"));
        L->revert_global(xyobfstr("wordVer"));
    }
}

// 将 cmd 按空格分割为 tokens
static std::vector<std::string> split_cmd(const std::string& cmd)
{
    std::vector<std::string> tokens;
    std::istringstream iss(cmd);
    std::string token;
    while (iss >> token) tokens.push_back(token);
    return tokens;
}

// 执行指令
static HashTable<std::string, std::string> values;
static struct DefineValue
{
    DefineValue()
    {
        values.get(xyobfcstr("gp")) = xyobfcstr("global print");
        values.get(xyobfcstr("gs")) = xyobfcstr("global set");
        values.get(xyobfcstr("gh")) = xyobfcstr("global hook");
        values.get(xyobfcstr("gu")) = xyobfcstr("global unhook");
        values.get(xyobfcstr("ga")) = xyobfcstr("global all");
    }
} define_value;
void xyu::execute_self_command(LuaState* L, const std::string& command)
{
    if (command.empty()) return;

    // 预处理: 变量替换
    std::string processed_cmd;
    for (size_t i = 0; i < command.size();)
    {
        size_t dollar_pos = command.find('$', i);
        // 替换完毕
        if (dollar_pos == std::string::npos)
        {
            processed_cmd += command.substr(i);
            break;
        }

        // 追加 $ 之前的普通字符
        processed_cmd += command.substr(i, dollar_pos - i);
        size_t start = dollar_pos + 1;
        // 尝试替换变量
        if (start < command.size() && (std::isalpha(uint8(command[start])) || command[start] == '_'))
        {
            // 提取完整变量名
            size_t j = start;
            while (j < command.size() && (std::isalnum(uint8(command[j])) || command[j] == '_')) ++j;
            std::string var = command.substr(start, j - start);
            std::string* val = values.find(var);
            if (val) processed_cmd += *val;
            else processed_cmd += command.substr(dollar_pos, j - dollar_pos);
            i = j;
        }
        // $ 后不是合法变量名开头，原样保留 $
        else
        {
            processed_cmd += '$';
            i = start;
        }
    }

    // 预处理: 按空格拆分为多个 token
    auto tokens = split_cmd(processed_cmd);
    if (tokens.empty()) return;

    DeepGuard guard{L};
    const std::string& head = tokens[0];

    // 0.变量声明: use {name} [value]
    if (head == xyobfcstr("use"))
    {
        if (tokens.size() < 2)
        {
            gtips.show(L, xyobfstr("变量声明: use {name} [value]"));
            return;
        }

        // 变量名合法性校验
        auto& name = tokens[1];
        auto check_valid = [](const std::string& name)
        {
            if (name.empty()) return false;
            if (!std::isalpha(uint8(name[0])) && name[0] != '_') return false;
            for (size_t i = 1; i < name.size(); ++i)
                if (!std::isalnum(uint8(name[i])) && name[i] != '_') return false;
            return true;
        };
        if (!check_valid(name))
        {
            gtips.show(L, xyobfstr("变量声明: 变量名只能使用字母、数字、下划线，且不能以数字开头"));
            return;
        }

        // 拼接值
        std::string value;
        for (int i=2; i<tokens.size(); ++i)
        {
            if (i != 2) value += ' ';
            value += tokens[i];
        }
        values.get(name) = value;
        return;
    }

    // 1.日志打印: log {mess} [-i|-d|-w|-e:日志级别]
    if (head == xyobfcstr("log"))
    {
        if (tokens.size() < 2)
        {
            gtips.show(L, xyobfstr("日志打印: log {信息} [-i|-d|-w|-e]"));
            return;
        }

        // 确定消息结束位置与日志级别
        int log_level = 0; // 0:LOGI, 1:LOGD, 2:LOGW, 3:LOGE

        const std::string& last = tokens.back();
        size_t msg_end = tokens.size() - 2;
        if (last == "-i") log_level = 0;
        else if (last == "-d") log_level = 1;
        else if (last == "-w") log_level = 2;
        else if (last == "-e") log_level = 3;
        else msg_end = tokens.size() - 1;

        // 拼接消息（恢复空格）
        std::string msg;
        for (size_t i = 1; i <= msg_end; ++i)
        {
            if (i > 1) msg += " ";
            msg += tokens[i];
        }

        // 输出日志
        switch (log_level)
        {
            case 0: LOGI("%s", msg.c_str()); break;
            case 1: LOGD("%s", msg.c_str()); break;
            case 2: LOGW("%s", msg.c_str()); break;
            case 3: LOGE("%s", msg.c_str()); break;
            default:;
        }
        return;
    }

    // 2.数值修改：set {key} {value}
    if (head == xyobfcstr("set"))
    {
        if (tokens.size() < 3)
        {
            gtips.show(L, xyobfstr("数值修改: set {字段名} {数值}"));
            return;
        }

        const std::string& key = tokens[1];
        double value = std::atof(tokens[2].c_str());
        if (value < 0)
        {
            gtips.show(L, xyobfstr("数值修改: 不合法的数值"));
            return;
        }

        std::stringstream ss;
        if (key == xyobfcstr("autotimes_limit"))
        {
            event_set_autotimes_limit(L, value);
            ss << xyobfstr("修改自动作战次数上限为<color=#92fc63>") << (int)value << "</color>";
        }
        else if (key == xyobfcstr("attack_scale"))
        {
            event_set_attack_scale(L, value);
            ss << xyobfstr("修改攻击倍率为<color=#92fc63>");
            value == (uint)value ? ss << (uint)value : ss << value;
            ss << xyobfstr("</color>");
        }
        else if (key == xyobfcstr("defence_scale"))
        {
            event_set_defence_scale(L, value);
            ss << xyobfstr("修改防御倍率为<color=#92fc63>");
            value == (uint)value ? ss << (uint)value : ss << value;
            ss << xyobfstr("</color>");
        }
        else if (key == xyobfcstr("cooldown_scale"))
        {
            event_set_coldown_rate(L, value);
            ss << xyobfstr("修改冷却倍率为<color=#92fc63>") << value << "</color>";
        }
        else if (key == xyobfcstr("range_scale"))
        {
            event_set_defence_scale(L, value);
            ss << xyobfstr("修改攻击范围倍率为<color=#92fc63>");
            value == (uint)value ? ss << (uint)value : ss << value;
            ss << xyobfstr("</color>");
        }
        else
        {
            gtips.show(L, xyobfstr("数值修改: 未知的修改字段"));
            return;
        }
        gtips.show(L, ss.str().c_str());
        return;
    }

    // 3.全局表操作: global
    if (head == xyobfcstr("global"))
    {
        if (tokens.size() < 2)
        {
            gtips.show(L, xyobfstr("全局表操作: global {print|hook|unhook} {参数}"));
            return;
        }

        const std::string& subcmd = tokens[1];

        // global print {path} [-d:最大递归深度 deep] [-m:是否打印元表] [-l:是否打印日志] [-f:过滤表 filter]
        if (subcmd == xyobfcstr("print"))
        {
            // 解析路径和可选参数
            std::string path;
            int depth = 3;
            bool show_meta = false;
            bool log = false;
            std::vector<std::string> filter;

            // 解析指令
            for (size_t i = 2; i < tokens.size(); ++i)
            {
                if (tokens[i] == "-d")
                    { if (++i < tokens.size()) depth = std::max(1, atoi(tokens[i].c_str())); }
                else if (tokens[i] == "-m") show_meta = true;
                else if (tokens[i] == "-l") log = true;
                else if (tokens[i] == "-f")
                    { if (++i < tokens.size()) filter.emplace_back(tokens[i]); }
                else if (path.empty()) path = tokens[i];
            }
            if (path.empty())
            {
                gtips.show(L, xyobfstr("全局表操作: 未提供要打印的全局表路径"));
                return;
            }

            // 打印
            LuaValue field;
            if (L->get_global(path.c_str(), &field))
            {
                L->push(field);
                L->print_to(gpath.get(path.c_str()), -1, depth, filter, show_meta);
                if (log) L->print(-1, depth, filter, show_meta);
                L->pop();
            }
            else gtips.show(L, (xyobfcstr("全局表字段 ") + path + xyobfcstr(" 不存在")).c_str());
            return;
        }

        // global set/hook {path} [-d:最大递归深度 deep] [-p:是否打印堆栈] [-c:是否打印变化后的实例]
        // [-v:修改为数值(boolean/number) value] [-s:修改为字符串 str] [-g:通过全局表获取值 path] [-n:nil]
        if (subcmd == xyobfcstr("hook") || subcmd == xyobfcstr("set"))
        {
            // 解析路径和可选参数
            std::string path;
            std::string value;
            int max_deep = 3;
            int set_type = 1; // 1:函数, 2:数值, 3:字符串, 4:全局表字段, 5:nil
            bool print_stack = false;
            bool print_change = false;
            bool swap = subcmd == xyobfcstr("hook");

            // 解析指令
            for (size_t i = 2; i < tokens.size(); ++i)
            {
                if (tokens[i] == "-p") print_stack = true;
                else if (tokens[i] == "-c") print_change = true;
                else if (tokens[i] == "-n") set_type = 5;
                else if (tokens[i] == "-v" || tokens[i] == "-s" || tokens[i] == "-g")
                {
                    if (tokens[i][1] == 'v') set_type = 2;
                    else if (tokens[i][1] == 's') set_type = 3;
                    else set_type = 4;
                    if (++i < tokens.size()) value = tokens[i];
                }
                else if (tokens[i] == "-d")
                {
                    if (++i < tokens.size()) max_deep = std::max(1, atoi(tokens[i].c_str()));
                }
                else if (path.empty()) path = tokens[i];
            }
            if (path.empty())
            {
                gtips.show(L, xyobfstr("全局表操作: 未提供要修改的全局表路径"));
                return;
            }
            if (set_type == 4 && value.empty())
            {
                gtips.show(L, xyobfstr("全局表操作: 未提供要修改的值"));
                return;
            }

            // 构建 hook 函数
            if (set_type == 1)
            {
                std::string old_path;
                size_t pos = path.find_last_of('.');
                if (pos == -1) old_path = "__old_" + path;
                else old_path = path.substr(0, pos + 1) + "__old_" + path.substr(pos + 1);

                L->push(print_stack);
                L->push(print_change);
                L->push(max_deep);
                L->push(path.c_str());
                L->push(old_path.c_str());
                L->push([](LuaState* L)
                {
                    bool print_stack = L->to_boolean(L->get_param(1));
                    bool print_change = L->to_boolean(L->get_param(2));
                    int max_deep = L->to_number(L->get_param(3));
                    const char* func_name = L->to_cstring(L->get_param(4));
                    const char* old_path = L->to_cstring(L->get_param(5));

                    LOGI(">> %s called", func_name);
                    if (print_stack) xyu::print_stack(L, func_name);

                    char buf[256];
                    int n = L->count();
                    LuaValue self = print_change && n >= 1 ? L->get(1) : LuaValue{};
                    sprintf(buf, "%s_in", func_name);
                    for (int i=1; i<=n; ++i) L->print_to(gpath.get(buf, i), i, max_deep);

                    L->push(L->get_global(old_path));
                    L->pop_insert(1);
                    L->pcall(n, -1);

                    int ret = L->count();
                    sprintf(buf, "%s_out", func_name);
                    for (int i=1; i<=ret; ++i) L->print_to(gpath.get(buf, i), i, max_deep);

                    if (print_change && n >= 1)
                    {
                        L->push(self);
                        sprintf(buf, "%s_chg", func_name);
                        L->print_to(gpath.get(buf), -1, max_deep);
                        L->pop();
                    }

                    if (L->count() == 1 && L->type(-1) <= LuaValue::String)
                        LOGI(">> %s end %s", func_name, L->format(-1).c_str());
                    else LOGI(">> %s end", func_name);

                    return ret;
                }, 5);

                LuaValue func = L->get(-1);
                L->pop();
                if (swap) L->swap_global(path.c_str(), func);
                else L->set_global(path.c_str(), func);
            }
            // 修改值 - 数值
            else if (set_type == 2)
            {
                LuaValue val;
                if (value == "true") val = true;
                else if (value == "false") val = false;
                else val = std::atof(value.c_str());
                if (swap) L->swap_global(path.c_str(), val);
                else L->set_global(path.c_str(), val);
            }
            // 修改值 - 字符串
            else if (set_type == 3)
            {
                if (swap) L->swap_global(path.c_str(), value.c_str());
                else L->set_global(path.c_str(), value.c_str());
            }
            // 修改值 - 通过全部表获取
            else if (set_type == 4)
            {
                LuaValue val = L->get_global(value.c_str());
                if (val.is_nil())
                {
                    gtips.show(L, xyobfstr("全局表操作: 未找到修改源字段"));
                    return;
                }
                if (swap) L->swap_global(path.c_str(), val);
                else L->set_global(path.c_str(), val);
            }
            // 修改值 - nil
            else if (set_type == 5)
            {
                if (swap) L->swap_global(path.c_str(), LuaValue{});
                else L->set_global(path.c_str(), LuaValue{});
            }

            return;
        }

        // global unhook {path}
        if (subcmd == xyobfcstr("unhook"))
        {
            std::string path = tokens.size() >= 2 ? tokens[2] : std::string{};
            if (path.empty())
            {
                gtips.show(L, xyobfstr("全局表操作: 未提供要hook的全局表路径"));
                return;
            }
            L->revert_global(path.c_str());
            return;
        }

        // global all {func} [-p:提供时func作为路径 对路径下的所有直接函数进行 hook] [-m:非精确匹配,包含即可]
        if (subcmd == xyobfcstr("all"))
        {
            std::string func;
            bool is_path = false;
            bool include_match = false;

            // 解析指令
            for (size_t i = 2; i < tokens.size(); ++i)
            {
                if (tokens[i] == "-p") is_path = true;
                else if (tokens[i] == "-m") include_match = true;
                else if (func.empty()) func = tokens[i];
            }
            if (func.empty())
            {
                gtips.show(L, xyobfstr("全局表操作: 未提供要hook的全局表函数名或路径"));
                return;
            }

            // 替换所有相同名称函数
            if (!is_path)
            {
                int count = 0;

                L->load_globaltable();
                L->push();
                while (L->next(-2))
                {
                    if (L->is_table(-1) && L->is_string(-2))
                    {
                        // 过滤表
                        std::string class_name = L->to_cstring(-2);
                        std::string filters[]
                        {
                            "Connection","PackStream","GameObject",
                            "Material","AutoLoader","SpineRole",
                            "FuncBuffer","PublicArg","Vector2","Vector3",
                            "Timer","FrameTimer",
                            "Color","Drop","Item","PowerRank",
                            "Equipment","EquipmentItem",
                            "ChapterLevelData",
                        };
                        for (auto& f : filters) if (class_name == f) goto next;

                        // 检查
                        if (!include_match)
                        {
                            L->push(func.c_str());
                            if (L->load_field(-2, native_v, false))
                            {
                                bool is_fun = L->is_callable(-1);
                                L->pop();
                                if (!is_fun) goto next;
                                {
                                    LOGI(">> hook %d: %s.%s", ++count, class_name.c_str(), func.c_str());
                                    L->push_copy(-2);
                                    L->push(func.c_str());
                                    L->push(+[](LuaState* L)
                                    {
                                        const char* name = L->to_cstring(L->get_param(1));
                                        const char* func = L->to_cstring(L->get_param(2));

                                        LOGI(">> %s.%s called", name, func);

                                        int n = L->count();
                                        L->push(L->get_global((std::string{name} + ".__old_" + func).c_str()));
                                        L->pop_insert(1);
                                        L->pcall(n, -1);

                                        int ret = L->count();
                                        return ret;
                                    }, 2);

                                    LuaValue hook = L->get(-1);
                                    L->pop();
                                    L->swap_field(-1, func.c_str(), hook);
                                }
                            }
                        }
                        else
                        {
                            HashTable<std::string, LuaValue> map;

                            L->push();
                            while (L->next(-2))
                            {
                                if (L->is_callable(-1) && L->is_string(-2) &&
                                    std::string{L->to_cstring(-2)}.find(func) != -1)
                                {
                                    LOGI(">> hook %d: %s.%s", ++count, class_name.c_str(), L->to_cstring(-2));
                                    L->push_copy(-2);
                                    L->push(class_name.c_str());
                                    L->push(+[](LuaState* L)
                                    {
                                        const char* func = L->to_cstring(L->get_param(1));
                                        const char* path = L->to_cstring(L->get_param(2));

                                        LOGI(">> %s.%s called", path, func);

                                        int n = L->count();
                                        L->push(L->get_global((std::string{path} + ".__old_" + func).c_str()));
                                        L->pop_insert(1);
                                        L->pcall(n, -1);

                                        int ret = L->count();
                                        return ret;
                                    }, 2);
                                    map.get(std::string{L->to_cstring(-3)}) = L->get(-1);
                                    L->pop();
                                }
                                L->pop();
                            }

                            for (auto& kv : map)
                                L->swap_field(-1, kv.key.c_str(), kv.val);
                        }
                    }
                next:
                    L->pop();
                }
                L->pop();
            }

            // 替换路径下的所有函数
            else
            {
                std::string& path = func;
                if (LuaValue c = L->get_global(path.c_str()); c.is_table())
                {
                    L->push(c);
                    HashTable<std::string, LuaValue> map;

                    L->push();
                    while (L->next(-2))
                    {
                        if (L->is_callable(-1))
                        {
                            L->push_copy(-2);
                            L->push(path.c_str());
                            L->push(+[](LuaState* L)
                            {
                                const char* func = L->to_cstring(L->get_param(1));
                                const char* path = L->to_cstring(L->get_param(2));

                                LOGI(">> %s.%s called", path, func);

                                int n = L->count();
                                L->push(L->get_global((std::string{path} + ".__old_" + func).c_str()));
                                L->pop_insert(1);
                                L->pcall(n, -1);

                                int ret = L->count();
                                return ret;
                            }, 2);
                            map.get(std::string{L->to_cstring(-3)}) = L->get(-1);
                            L->pop();
                        }
                        L->pop();
                    }

                    for (auto& kv : map)
                        L->swap_field(-1, kv.key.c_str(), kv.val);
                    L->pop();
                }
            }

            return;
        }

        return;
    }

    // 9.测试指令
    if (head == xyobfcstr("test"))
    {

        return;
    }

    // 10.未支持指令
    gtips.show(L, (xyobfcstr("未支持的指令: ") + tokens[0]).c_str());
}

