#include "../switch_order.h"

#include <sys/stat.h>

#include "etools.hpp"
#include "../event.h"

using namespace xyu;
using Switch = OrderSwitchGroup::Switch;

constexpr int start_id = 1314;
static std::vector<Switch> switches;


static void flush(LuaState* L)
{
    L->push(L->get_global(xyobfstr("SettingsOtherPanel.OnUpdate")));
    L->push_copy(1);
    L->call(1, 0);
}
static auto add_switch = [](auto&& mess, auto&& callback)
{
    int index = switches.size();
    switches.emplace_back(Switch{start_id + index, mess, callback});
};

static int step = 1;
static bool disposable = true;

static int code_pos;
static int code_start = sizeof("指令: ") - 1;
static void (*code_history_change)(LuaState*, bool next);
static void add_mode_and_code()
{
    constexpr const char* mode_array[] { "自由指令", "数值修改", "全局表操作" };
    constexpr int mode_count = std::size(mode_array);

    // 数值修改的显示名（用于界面和切换）
    constexpr const char* mode_value_display[] { "作战次数上限", "攻击倍率", "防御倍率", "冷却倍率" };
    // 数值修改的内部键名（传给 execute_command）
    constexpr const char* mode_value_key[] { "autotimes_limit", "attack_scale", "defence_scale", "cooldown_scale" };
    constexpr int mode_value_count = std::size(mode_value_display);
    // 全局表操作的子命令
    constexpr const char* global_subcmd[] { "print", "hook", "unhook" };
    constexpr int global_sub_count = std::size(global_subcmd);

    // 当前状态
    static int mode_index = 0;
    static int mode_sub_index = 0;

    // 历史记录
    struct State { std::string m1, m2; int i1, i2; };
    static std::vector<State> history;
    static int history_index = 0;

    // 更新第二个开关的文本
    static auto update_mess_2 = [](bool update_sub = false)
    {
        auto& mess = switches[code_pos + 1].mess;
        if (mode_index == 0)
        {
            mess = "指令: ";
        }
        else if (mode_index == 1)
        {
            if (update_sub) mode_sub_index = (mode_sub_index + 1) % mode_value_count;
            mess = mode_value_display[mode_sub_index];
            mess += ": ";
        }
        else if (mode_index == 2)
        {
            if (update_sub) mode_sub_index = (mode_sub_index + 1) % global_sub_count;
            mess = global_subcmd[mode_sub_index];
            mess += ": ";
        }
        code_start = mess.size();
    };

    // 开关1：切换一级模式
    add_switch("自由指令", [](LuaState* L, bool open)
    {
        auto& mess = switches[code_pos].mess;
        mode_index = (mode_index + (open ? 1 : (mode_count - 1))) % mode_count;
        mode_sub_index = 0;
        mess = mode_array[mode_index];
        update_mess_2();
        flush(L);
    });

    // 开关2：执行指令
    add_switch("指令: ", [](LuaState* L, bool open)
    {
        auto& mess = switches[code_pos + 1].mess;
        if (open)
        {
            const char* input = mess.data() + code_start;
            std::string full_cmd;

            // 自由指令
            if (mode_index == 0) full_cmd = input;
            // 数值修改：set <内部键名> <数值>
            else if (mode_index == 1)
            {
                full_cmd = "set ";
                full_cmd += mode_value_key[mode_sub_index];
                full_cmd += " ";
                full_cmd += input;
            }
            // 全局表操作：global <子命令> <参数>
            else if (mode_index == 2)
            {
                full_cmd = "global ";
                full_cmd += global_subcmd[mode_sub_index];
                full_cmd += " ";
                full_cmd += input;
            }

            history.push_back({switches[code_pos].mess, mess, mode_index, mode_sub_index});
            history_index = history.size();

            execute_self_command(L, full_cmd);
            if (disposable) mess.resize(code_start);
        }
        else if (mode_index == 1 || mode_index == 2) update_mess_2(true);
        flush(L);
    });

    // 历史记录恢复
    code_history_change = [](LuaState* L, bool next)
    {
        int new_index = history_index + (next ? 1 : -1);
        if (new_index < 0 || new_index >= history.size()) return;
        history_index = new_index;

        auto& state = history[new_index];
        switches[code_pos].mess = state.m1;
        switches[code_pos+1].mess = state.m2;
        mode_index = state.i1;
        mode_sub_index = state.i2;
        flush(L);
    };
}

static int number_pos;
constexpr static int number_start = sizeof("数值: ") - 1;
static void (*number_history_change)(LuaState*, bool next);
static void add_input_number()
{
    constexpr char number_array[] = "0123456789-#";
    constexpr int number_count = sizeof(number_array) - 1;
    static int number_index = 0;

    struct State { std::string m1, m2; int i1; };
    static std::vector<State> history;
    static int history_index = 0;
    
    add_switch("数值: ", [](LuaState* L, bool open)
    {
        auto& mess = switches[number_pos].mess;
        if (open)
        {
            switches[code_pos+1].mess += mess.data() + number_start;
            history.push_back({mess, switches[number_pos+1].mess, number_index});
            history_index = history.size();
            if (disposable) mess.resize(number_start);
        }
        else
        {
            if (number_index <= 9) mess += number_array[number_index];
            else if (number_index == 10) mess.resize(std::max(number_start, (int)mess.size() - 1));
            else if (number_index == 11) mess.resize(number_start);
        }
        flush(L);
    });
    add_switch("数字: 0", [](LuaState* L, bool open)
    {
        auto& mess = switches[number_pos+1].mess;
        int old_index = number_index;
        number_index = (number_index + (open ? step : number_count-(step%number_count))) % number_count;
        if (number_index <= 9)
        {
            if (old_index > 9) mess = "数字: 0";
            mess.back() = number_array[number_index];
        }
        else if (number_index == 10) mess = "退格";
        else if (number_index == 11) mess = "清空";
        flush(L);
    });

    number_history_change = [](LuaState* L, bool next)
    {
        int new_index = history_index + (next ? 1 : -1);
        if (new_index < 0 || new_index >= history.size()) return;
        history_index = new_index;

        auto& state = history[new_index];
        switches[number_pos].mess = state.m1;
        switches[number_pos+1].mess = state.m2;
        number_index = state.i1;
        flush(L);
    };
}

static int alpha_pos;
constexpr static int word_start = sizeof("单词: ") - 1;
static void (*alpha_history_change)(LuaState*, bool next);
static void add_input_alpha()
{
    constexpr char alpha_array[] = "abcdefghijklmnopqrstuvwxyz-#^";
    constexpr int alpha_count = sizeof(alpha_array) - 1;
    static int alpha_index = 0;
    static bool alpha_upper = false;

    struct State { std::string m1, m2; int i1; };
    static std::vector<State> history;
    static int history_index = 0;
    
    add_switch("单词: ", [](LuaState* L, bool open)
    {
        auto& mess = switches[alpha_pos].mess;
        if (open)
        {
            switches[code_pos+1].mess += mess.data() + word_start;
            history.push_back({mess, switches[alpha_pos+1].mess, alpha_index});
            history_index = history.size();
            if (disposable) mess.resize(word_start);
        }
        else
        {
            if (alpha_index < 26) mess += alpha_array[alpha_index] - (alpha_upper ? 32 : 0);
            else if (alpha_index == 26) mess.resize(std::max(word_start, (int)mess.size() - 1));
            else if (alpha_index == 27) mess.resize(word_start);
            else if (alpha_index == 28) alpha_upper = !alpha_upper;
        }
        flush(L);
    });
    add_switch("字母: a", [](LuaState* L, bool open)
    {
        auto& mess = switches[alpha_pos+1].mess;
        int old_index = alpha_index;
        alpha_index = (alpha_index + (open ? step : alpha_count-(step%alpha_count))) % alpha_count;
        if (alpha_index < 26)
        {
            if (old_index >= 26) mess = "字母: 0";
            mess.back() = alpha_array[alpha_index] - (alpha_upper ? 32 : 0);
        }
        else if (alpha_index == 26) mess = "退格";
        else if (alpha_index == 27) mess = "清空";
        else if (alpha_index == 28) mess = "切换大小写";
        flush(L);
    });

    alpha_history_change = [](LuaState* L, bool next)
    {
        int new_index = history_index + (next ? 1 : -1);
        if (new_index < 0 || new_index >= history.size()) return;
        history_index = new_index;

        auto& state = history[new_index];
        switches[alpha_pos].mess = state.m1;
        switches[alpha_pos+1].mess = state.m2;
        alpha_index = state.i1;
        flush(L);
    };
}

static int symbol_pos;
static void add_input_symbol()
{
    constexpr char symbol_array[]
    {
        ' ','.','-',';',',',':','=','(',')','{','}','[',']',
        '+','*','/','%','&','|','^','~','!','?',
        '<','>','\"','\'','\\','#','_','@','`','$'
    };
    constexpr int symbol_count = sizeof(symbol_array);
    static int symbol_index = 0;
    
    static struct SymbolUsedCache
    {
    private:
        uint times[128]{};
        char pair[128]{};
        char map[symbol_count];

    public:
        SymbolUsedCache() noexcept
        {
            pair['('] = ')'; pair[')'] = '(';
            pair['{'] = '}'; pair['}'] = '{';
            pair['['] = ']'; pair[']'] = '[';
            pair['<'] = '>'; pair['>'] = '<';
            for (int i=0; i<symbol_count; ++i) map[i] = i;
        }

        char get()
        {
            return symbol_array[map[symbol_index]];
        }
        void use()
        {
            char ch = get();
            uint t = ++times[ch];

            int pos, start = symbol_index;
            char another = pair[ch];
            if (another) { t = std::max(t, times[another]); pos = symbol_index - 1 - (ch > another); }
            else pos = symbol_index - 1;
            if (pos < 0 || get_times(pos) >= t) return;

            // 个数较少直接逐个查找
            if (pos <= 8) { while (--pos >= 0) if (get_times(pos) >= t) break; }
            // 否则二分查找
            else
            {
                int i = 0;
                do
                {
                    int mid = (i + pos) / 2;
                    if (get_times(mid) >= t) i = mid + 1;
                    else pos = mid - 1;
                }
                while (i <= pos);
            }

            // 移动
            ++pos;
            if (!another)
            {
                char tmp = map[start];
                __builtin_memmove(map+pos+1, map+pos, start-pos);
                map[pos] = tmp;
            }
            else if (ch < another)
            {
                char tmp1 = map[start], tmp2 = map[start+1];
                __builtin_memmove(map+pos+2, map+pos, start-pos);
                map[pos] = tmp1, map[pos+1] = tmp2;
            }
            else
            {
                char tmp1 = map[start-1], tmp2 = map[start];
                __builtin_memmove(map+pos+2, map+pos, start-pos-1);
                map[pos] = tmp1, map[++pos] = tmp2;
            }
            symbol_index = pos;
        }
        void reset()
        {
            char ch = get();
            __builtin_memset(times, 0, sizeof(times));
            for (int i=0; i<symbol_count; ++i)
            {
                map[i] = i;
                if (symbol_array[i] == ch) symbol_index = i;
            }
        }

    private:
        uint get_times(int index)
        {
            char ch = symbol_array[map[index]];
            uint t = times[ch];
            char another = pair[ch];
            if (another && times[another] > t) t = times[another];
            return t;
        }
    } symbol_cache;
    
    add_switch("重置记忆 | 追加到指令", [](LuaState* L, bool open)
    {
        if (open)
        {
            switches[code_pos+1].mess += symbol_cache.get();
            symbol_cache.use();
        }
        else symbol_cache.reset();
        flush(L);
    });
    add_switch("字符: 空格", [](LuaState* L, bool open)
    {
        auto& mess = switches[symbol_pos+1].mess;
        char old_ch = symbol_cache.get();
        symbol_index = (symbol_index + (open ? 1 : symbol_count-1)) % symbol_count;
        char new_ch = symbol_cache.get();
        if (new_ch == ' ') mess = "字符: 空格";
        else
        {
            if (old_ch == ' ') mess = "字符:  ";
            mess.back() = new_ch;
        }
        flush(L);
    });
}

static int text_pos;
constexpr static int test_start = sizeof("文本: ") - 1;
static int hex_index = 0;
static void (*text_history_change)(LuaState*, bool next);
static void add_input_text()
{
    constexpr char hex_array[] = "0123456789ABCDEF-#";
    constexpr int hex_count = sizeof(hex_array) - 1;
    static struct HexCtrl
    {
    private:
        char cell[5]{};
        int cell_index = 0;
        int cell_size = 0;
        std::string complete;
        std::string part;
        std::vector<uint> indexs;

    public:
        std::string get() { return complete + part; }
        std::string get_complate() { return complete; }

        std::string append(LuaState* L)
        {
            uint8 nibble = hex_index;

            // 首半字节确定长度
            if (cell_size == 0)
            {
                if (nibble <= 0x7) cell_size = 2;
                else if (nibble >= 0xC && nibble <= 0xD) cell_size = 4;
                else if (nibble == 0xE) cell_size = 6;
                else if (nibble == 0xF) cell_size = 8;
                else
                {
                    gtips.show(L, "非法的 UTF-8 字符");
                    return complete + part;
                }
                part = "0x_";
            }
            else if (cell_size == 8 && cell_index == 1)
            {
                if (nibble > 0x7)
                {
                    gtips.show(L, "非法的 UTF-8 字符");
                    cell_index = cell_size = 0;
                    part.clear();
                    return complete;
                }
            }

            // 将半字节写入对应位置
            int byte_pos = cell_index / 2;
            if (cell_index % 2 == 0) cell[byte_pos] = nibble << 4;
            else cell[byte_pos] |= nibble;
            cell_index++;

            // 拼成一个完整字符
            if (cell_index == cell_size)
            {
                indexs.emplace_back(complete.size());
                cell[cell_size / 2] = '\0';
                complete += cell;
                cell_index = 0;
                cell_size = 0;
                part.clear();
                return complete;
            }

            // 尚未完成
            part.back() = hex_array[hex_index];
            part += '_';
            return complete + part;
        }
        void pop()
        {
            // 弹出一个半字节
            if (cell_index > 0)
            {
                int byte_pos = --cell_index / 2;
                if (cell_index % 2) cell[byte_pos] &= 0xF0;
                else cell[byte_pos] = 0;

                if (cell_index == 0)
                {
                    cell_size = 0;
                    part.clear();
                }
                else
                {
                    part.resize(part.size() - 2);
                    part.back() = '_';
                }
            }
            // 弹出一个完整字符
            else if (!indexs.empty())
            {
                complete.resize(indexs.back());
                indexs.pop_back();
            }
        }
        void clear()
        {
            cell_index = cell_size = 0;
            complete.clear();
            part.clear();
            indexs.clear();
        }
    } hex_ctrl;

    struct State { HexCtrl ctrl; std::string m1, m2; int i1; };
    static std::vector<State> history;
    static int history_index = 0;
    
    add_switch("文本: ", [](LuaState* L, bool open)
    {
        auto& mess = switches[text_pos].mess;
        if (open)
        {
            switches[code_pos+1].mess += hex_ctrl.get_complate();
            history.push_back({hex_ctrl, mess, switches[text_pos+1].mess, hex_index});
            history_index = history.size();
            if (disposable) { hex_ctrl.clear(); mess.resize(test_start); }
        }
        else
        {
            if (hex_index < 16) hex_ctrl.append(L);
            else if (hex_index == 16) hex_ctrl.pop();
            else if (hex_index == 17) hex_ctrl.clear();
            mess.resize(test_start);
            mess += hex_ctrl.get();
        }
        flush(L);
    });
    add_switch("半字节: 0", [](LuaState* L, bool open)
    {
        auto& mess = switches[text_pos+1].mess;
        int old_index = hex_index;
        hex_index = (hex_index + (open ? step : hex_count-(step%hex_count))) % hex_count;
        if (hex_index < 16)
        {
            if (old_index >= 16) mess = "半字节: 0";
            mess.back() = hex_array[hex_index];
        }
        else if (hex_index == 16) mess = "退格";
        else if (hex_index == 17) mess = "清空";
        flush(L);
    });

    text_history_change = [](LuaState* L, bool next)
    {
        int new_index = history_index + (next ? 1 : -1);
        if (new_index < 0 || new_index >= history.size()) return;
        history_index = new_index;

        auto& state = history[new_index];
        hex_ctrl = state.ctrl;
        switches[text_pos].mess = state.m1;
        switches[text_pos+1].mess = state.m2;
        hex_index = state.i1;
        flush(L);
    };
}

static int ctrl_pos;
static void add_ctrl()
{
    constexpr const char* ctrl_array[] {"输入配置", "退格", "历史记录"};
    constexpr int ctrl_count = std::size(ctrl_array);
    constexpr const char* ctrl_input_array[] {"追加后清空", "滚动步长"};
    constexpr int ctrl_input_count = std::size(ctrl_input_array);
    constexpr const char* ctrl_object_array[] {"指令", "数值", "单词", "文本"};
    constexpr int ctrl_object_count = std::size(ctrl_object_array);
    static int ctrl_index = 0;
    static int ctrl_sub_index = 0;
    static int ctrl_arg = 0;

    // 更新第二个开关显示文本
    static auto update_mess_2 = []
    {
        auto& mess = switches[ctrl_pos+1].mess;
        if (ctrl_index == 0)
        {
            if (ctrl_sub_index == 0) mess = disposable ? "true" : "false";
            else if (ctrl_sub_index == 1) mess = std::to_string(step);
        }
        else if (ctrl_index == 1)
        {
            if (ctrl_arg == 0) mess = "清空";
            else mess = std::to_string(ctrl_arg);
        }
        else if (ctrl_index == 2) mess = "上一条 | 下一条";
    };

    // 更新第一个开关显示文本
    static auto update_mess_1 = [](bool new_ctrl_index)
    {
        auto& mess = switches[ctrl_pos].mess;
        if (new_ctrl_index)
        {
            ctrl_sub_index = 0;
            if (++ctrl_index >= ctrl_count) ctrl_index = 0;
        }
        else ctrl_sub_index++;

        mess = ctrl_array[ctrl_index];
        if (ctrl_index == 0)
        {
            if (ctrl_sub_index >= ctrl_input_count) ctrl_sub_index = 0;
            mess += ": ";
            mess += ctrl_input_array[ctrl_sub_index];
        }
        else if (ctrl_index == 1 || ctrl_index == 2)
        {
            if (new_ctrl_index) ctrl_arg = 1;
            else if (ctrl_sub_index >= ctrl_object_count) ctrl_sub_index = 0;
            mess += ": ";
            mess += ctrl_object_array[ctrl_sub_index];
        }

        update_mess_2();
    };

    add_switch("输入配置: 追加后清空", [](LuaState* L, bool open)
    {
        if (open) update_mess_1(true);
        else update_mess_1(false);
        flush(L);
    });
    add_switch("true", [](LuaState* L, bool open)
    {
        if (ctrl_index == 0)
        {
            if (ctrl_sub_index == 0) disposable = open;
            else if (ctrl_sub_index == 1)
            {
                if (open) { if (++step > 100) step = 100; }
                else if (--step < 1) step = 1;
            }
        }
        else if (ctrl_index == 1)
        {
            if (open)
            {
                if (ctrl_sub_index <= 2)
                {
                    std::string* messes[] {&switches[code_pos+1].mess, &switches[number_pos].mess, &switches[alpha_pos].mess};
                    int start[] {code_start, number_start, word_start};
                    auto& mess = messes[ctrl_sub_index];
                    int new_size = std::max(start[ctrl_sub_index], ctrl_arg == 0 ? 0 : (int)mess->size() - ctrl_arg);
                    mess->resize(new_size);
                }
                else if (ctrl_sub_index == 3)
                {
                    int tmp_hex_index = hex_index;
                    if (ctrl_arg == 0) hex_index=17, switches[text_pos].callback(L, false);
                    else { hex_index=16; for (int i=0; i<ctrl_arg; ++i) switches[text_pos].callback(L, false); }
                    hex_index = tmp_hex_index;
                }
            }
            else ctrl_arg = (ctrl_arg + 1) % 4;
        }
        else if (ctrl_index == 2)
        {
            using func_t = void (*)(LuaState*, bool);
            func_t funcs[] = {code_history_change, number_history_change, alpha_history_change, text_history_change};
            funcs[ctrl_sub_index](L, open);
        }
        update_mess_2();
        flush(L);
    });
}


OrderSwitchGroup::OrderSwitchGroup()
{
    code_pos = switches.size();
    add_mode_and_code();

    number_pos = switches.size();
    add_input_number();

    alpha_pos = switches.size();
    add_input_alpha();

    symbol_pos = switches.size();
    add_input_symbol();

    text_pos = switches.size();
    add_input_text();

    ctrl_pos = switches.size();
    add_ctrl();
}

std::vector<Switch>& OrderSwitchGroup::get()
{
    return switches;
}

Switch& OrderSwitchGroup::get(int id)
{
    int index = id - start_id;
    if (index <= 0) return switches.front();
    if (index >= switches.size()) return switches.back();
    return switches[index];
}

void OrderSwitchGroup::deal(LuaState* L, int id, bool open)
{
    if (id < start_id) return;
    int index = id - start_id;
    if (index >= switches.size()) return;
    switches[index].callback(L, open);
}

OrderSwitchGroup order_switch_group;
