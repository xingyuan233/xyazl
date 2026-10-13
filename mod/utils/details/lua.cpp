#include "../lua.h"

#include <cstring>
#include <vector>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <ostream>

#include "../il2.h"
#include "../log.h"
#include "../macro.h"
#include "../hashtable.h"
#include "../config.h"

using vec_s = std::vector<std::string>;

namespace xyu
{
    std::string get_part_str(const char* start, const char* end = nullptr)
    {
        size_t len = end ? (end - start) : strlen(start);
        return std::string(start, len);
    };

    Il2CppString* get_part_il2str(const char* start, const char* end = nullptr)
    {
        size_t len = end ? (end - start) : strlen(start);
        std::string temp(start, len);
        return Il2CppString::from(temp.c_str());
    };

    auto lua_info(LuaState* L = nullptr)
    {
        static struct LuaInfo
        {
            bool is_init = false;
            LuaState* main;
            bool has_gt_idx;

            LuaInfo& init(LuaState* L)
            {
                if (is_init) return *this;
                is_init = true;

                main = L->main_coroutine();

                L->push_copy(-10002);
                has_gt_idx = L->is_table(-1);
                L->pop();

                return *this;
            }
        }info;

        return info.init(L);
    }

    const char* get_type_name(LuaValue::Type type)
    {
        using Type = LuaValue::Type;
        switch (type)
        {
            case Type::Nil:             return "nil";
            case Type::Boolean:         return "boolean";
            case Type::LightUserData:   return "lightuserdata";
            case Type::Number:          return "number";
            case Type::String:          return "string";
            case Type::Table:           return "table";
            case Type::Function:        return "function";
            case Type::CClosure:        return "cclosure";
            case Type::LClosure:        return "lclosure";
            case Type::Callable:        return "callable";
            case Type::UserData:        return "userdata";
            case Type::Coroutine:       return "coroutine";
            default:                    return "unknown";
        }
    }

    std::string dump_luajit_value(const TValue& val)
    {
        char buf[64];

#if defined(__i386__)
        enum {
            LJ_TNIL = ~0u,
            LJ_TFALSE = ~1u,
            LJ_TTRUE = ~2u,
            LJ_TLIGHTUD = ~3u,
            LJ_TSTR = ~4u,
            LJ_TUPVAL = ~5u,
            LJ_TTHREAD = ~6u,
            LJ_TPROTO = ~7u,
            LJ_TFUNC = ~8u,
            LJ_TTRACE = ~9u,
            LJ_TCDATA = ~10u,
            LJ_TTAB = ~11u,
            LJ_TUDATA = ~12u,
            LJ_TISNUM = 0xfffeffffu // 关键的分界线
        };

        uint32 tag = val.g >> 32;
        uint32 data = val.g & 0xffffffffu;
#else
        enum {
            LJ_TNIL     = 0x1ffffu,
            LJ_TFALSE   = 0x1ffffu - 1,
            LJ_TTRUE    = 0x1ffffu - 2,
            LJ_TLIGHTUD = 0x1ffffu - 3,
            LJ_TSTR     = 0x1ffffu - 4,
            LJ_TUPVAL   = 0x1ffffu - 5,
            LJ_TTHREAD  = 0x1ffffu - 6,
            LJ_TPROTO   = 0x1ffffu - 7,
            LJ_TFUNC    = 0x1ffffu - 8,
            LJ_TTRACE   = 0x1ffffu - 9,
            LJ_TCDATA   = 0x1ffffu - 10,
            LJ_TTAB     = 0x1ffffu - 11,
            LJ_TUDATA   = 0x1ffffu - 12,
            LJ_TISNUM   = 0x1fff2u
        };

        uint32 tag = val.g >> 47;
        uint64 data = val.g & 0x00007fffffffffffull;
#endif

        // 1. 处理 Double (如果 Tag 小于 LJ_TISNUM，它就是一个纯粹的数字)
        if (tag < LJ_TISNUM)
        {
            if ((int)val.n == val.n) snprintf(buf, sizeof(buf), "double:%d", (int)val.n);
            else snprintf(buf, sizeof(buf), "double:%g", val.n);
            return buf;
        }

        // 2. 根据 Tag 处理其他类型
        switch (tag)
        {
        case LJ_TNIL:
            return "nil";
        case LJ_TFALSE:
            return "bool:false";
        case LJ_TTRUE:
            return "bool:true";
        case LJ_TLIGHTUD:
            snprintf(buf, sizeof(buf), "ptr:%p", (void*)data);
            break;
        case LJ_TSTR:
        {
            auto s = (GCstr*)(uintptr_t)data;
            if (s)
            {
                const char* str_data = (const char*)(s + 1);
                return "str:" + std::string{str_data, s->len};
            }
            return "str:nullptr";
        }
        case LJ_TTHREAD: // 协程
            snprintf(buf, sizeof(buf), "coroutine:%p", (void*)data);
            break;
        case LJ_TFUNC: // 函数
            snprintf(buf, sizeof(buf), "func:%p", (void*)data);
            break;
        case LJ_TTAB: // 表
            snprintf(buf, sizeof(buf), "table:%p", (void*)data);
            break;
        case LJ_TUDATA: // 用户数据
            snprintf(buf, sizeof(buf), "data:%p", (void*)data);
            break;
        case LJ_TISNUM: // 内部整数类型 (仅当开启了特定编译选项或在解释器内部时)
            snprintf(buf, sizeof(buf), "int:%d", (int)data);
            break;
        case LJ_TUPVAL:
            snprintf(buf, sizeof(buf), "upval:%p", (void*)data);
            break;
        case LJ_TPROTO:
            snprintf(buf, sizeof(buf), "proto:%p", (void*)data);
            break;
        case LJ_TTRACE:
            snprintf(buf, sizeof(buf), "trace:%p", (void*)data);
            break;
        case LJ_TCDATA:
            snprintf(buf, sizeof(buf), "cdata:%p", (void*)data);
            break;
        default:
            return "?";
        }
        return buf;
    }

    template <typename Stream>
    Stream& dump_lua_value_norecursion(Stream&& s, LuaState* L, int idx, int type)
    {
        using sv = std::string_view;

        switch (type)
        {
            case LuaValue::Nil:
                return s << sv{"nil"}, s;

            case LuaValue::Boolean:
                return s << (L->to_boolean(idx) ? sv{"true"} : sv{"false"}), s;

            case LuaValue::Number:
            {
                double d = L->to_number(idx);
                if ((int)d == d) return s << (int)d, s;
                return s << d, s;
            }

            case LuaValue::LightUserData:
                s << sv{"light-"};
            case LuaValue::UserData:
                goto saddr;

            case LuaValue::String:
            {
                auto str = L->to_string(idx);
                s << '\"';
                if (str) s << str->to_utf8();
                s << '\"';
                return s;
            }

            case LuaValue::Table:
                s << sv{"Table-"};
                goto saddr;

            case LuaValue::Coroutine:
                s << sv{"coro-"};
                goto saddr;

            case LuaValue::Callable:
            {
                auto cc = (GCfuncC*)L->to_address(idx);
                if (!cc) return s << sv{"nullfun"}, s;

                if (L->is_function(idx)) s << sv{"Cfunc-"} << cc->f;
                else if (L->is_cclosure(idx))
                {
                    s << sv{"Cclos-"} << cc->f;
                    if (cc->nupvalues > 0)
                    {
                        s << sv{"-("};
                        for (int i=0; i<cc->nupvalues; ++i)
                        {
                            auto vp = cc->upvalue + i;
                            if (i != 0) s << ',';
                            s << dump_luajit_value(*vp);
                        }
                        s << ')';
                    }
                }
                else
                {
                    auto lc = (GCfuncL*)cc;
                    s << sv{"Lfunc-"} << lc;
                    if (lc->nupvalues > 0)
                    {
                        s << sv{"-("};
                        for (int i=0; i<lc->nupvalues; ++i)
                        {
                            auto vp = lc->uvptr[i]->v;
                            if (i != 0) s << ',';
                            s << dump_luajit_value(*vp);
                        }
                        s << ')';
                    }
                    if (lc->pc->numparams > 0 || lc->pc->flags & 0x02)
                    {
                        s << '-' << (uint)lc->pc->numparams;
                        if (lc->pc->flags & 0x02) s << '+';
                    }
                }
                return s;
            }

            default:
                return s << '?', s;
        }

        saddr:
            return s << L->to_address(idx), s;
    }

    template <typename Stream>
    Stream& dump_lua_value(Stream&& s, LuaState* L, int idx, int max_depth, bool show_meta,
                           const vec_s& filter, HashTable<void*, bool>& ht, int depth = 0)
    {
        using sv = std::string_view;

        auto type = L->type(idx);

        if (type != LuaValue::Table) dump_lua_value_norecursion(s, L, idx, type);
        else
        {
            void* addr = L->to_address(idx);
            ht.get(addr) = true;
            if (depth == 0) s << sv{"Table-"} << addr;

            if (depth == max_depth) return s;

            s << '{';

            bool first = true;

            L->push();
            if (idx < 0) --idx;
            while (L->next(idx))    // 弹出 key，压入下一对 key,val
            {
                if (!first) s << ',';
                first = false;

                // 栈顶是 value (-1)，下面是 key (-2)

                auto val_t = L->type(-1);
                auto key_t = L->type(-2);

                if (key_t == LuaValue::Table) ht.get(L->to_address(-2)) = true;
                if (val_t != LuaValue::Table)
                {
                    dump_lua_value_norecursion(s, L, -2, key_t) << ':';
                    dump_lua_value_norecursion(s, L, -1, val_t);
                }
                else
                {
                    auto key = dump_lua_value_norecursion(std::stringstream{}, L, -2, key_t).str();
                    s << key << ':';

                    void* vp = L->to_address(-1);
                    s << sv{"Table-"} << vp;
                    if (!ht.get(vp))
                    {
                        bool match = depth == max_depth - 1;
                        if (!filter.empty() && !match && key_t == LuaValue::String)
                            for (auto& fstr : filter)
                                if (key.find(fstr) != -1) { match = true; break; }
                        if (!match) dump_lua_value(s, L, -1, max_depth, show_meta, filter, ht, depth + 1);
                        else ht.get(vp) = true;
                    }
                }
                L->pop();
            }

            if (show_meta && L->load_metatable(idx))
            {
                if (!first) s << ',';
                void* mp = L->to_address(-1);
                s << sv{"__meta:Table-"} << mp;
                if (!ht.get(mp)) dump_lua_value(s, L, -1, max_depth, show_meta, filter, ht, depth + 1);
                L->pop();
            }

            s << '}';
        }

        return s;
    }

    void print_lua_value(std::stringstream& ss, LuaState* L, int idx, int max_depth, bool show_meta,
                         const vec_s& filter, HashTable<void*, bool>& ht, int depth = 0)
    {
        auto type = L->type(idx);
        std::string pad((depth + 1) * 2, ' ');

        if (type != LuaValue::Table)
        {
            LOGI("%s>> %s", pad.c_str(), dump_lua_value_norecursion(ss, L, idx, type).str().c_str());
            ss.clear(); ss.str("");
        }
        else
        {
            void* addr = L->to_address(idx);
            ht.get(addr) = true;
            if (depth == 0) LOGI(">> Table-%p", addr);

            if (depth == max_depth) return;

            L->push();
            if (idx < 0) --idx;
            while (L->next(idx))
            {
                // 栈顶是 value (-1)，下面是 key (-2)
                auto val_t = L->type(-1);
                auto key_t = L->type(-2);
                auto key = dump_lua_value_norecursion(ss, L, -2, key_t).str();
                ss.clear(); ss.str("");

                if (key_t == LuaValue::Table) ht.get(L->to_address(-2)) = true;
                if (val_t != LuaValue::Table)
                {
                    LOGI("%s>> %s: %s", pad.c_str(), key.c_str(),
                          dump_lua_value_norecursion(ss, L, -1, val_t).str().c_str());
                    ss.clear(); ss.str("");
                }
                else
                {
                    void* vp = L->to_address(-1);
                    LOGI("%s>> %s: Table-%p", pad.c_str(), key.c_str(), vp);
                    if (!ht.get(vp))
                    {
                        bool match = depth == max_depth - 1;
                        if (!filter.empty() && !match && key_t == LuaValue::String)
                            for (auto& fstr : filter)
                                if (key.find(fstr) != -1) { match = true; break; }
                        if (!match) print_lua_value(ss, L, -1, max_depth, show_meta, filter, ht, depth + 1);
                        else ht.get(vp) = true;
                    }
                }
                L->pop();
            }

            if (show_meta && L->load_metatable(idx))
            {
                void* mp = L->to_address(-1);
                LOGI("%s>> __meta: Table-%p", pad.c_str(), mp);
                if (!ht.get(mp)) print_lua_value(ss, L, -1, max_depth, show_meta, filter, ht, depth + 1);
                L->pop();
            }
        }
    }

    void print_lua_value_to(std::ofstream& ofs, LuaState* L, int idx, int max_depth,
                            bool show_meta, const vec_s& filter, HashTable<void*, bool>& ht, int depth = 0)
    {
        using ss = std::stringstream;
        using sv = std::string_view;

        auto type = L->type(idx);
        std::string pad((depth + 1) * 2, ' ');

        if (type != LuaValue::Table) dump_lua_value_norecursion(ofs << pad << sv{">> "}, L, idx, type) << '\n';
        else
        {
            void* addr = L->to_address(idx);
            ht.get(addr) = true;
            if (depth == 0) ofs << sv{">> Table-"} << addr << '\n';

            if (depth == max_depth) return;

            L->push();
            if (idx < 0) --idx;
            while (L->next(idx))
            {
                // 此时栈顶是 value (-1)，下面是 key (-2)
                ofs << pad << sv{">> "};

                auto val_t = L->type(-1);
                auto key_t = L->type(-2);

                if (key_t == LuaValue::Table) ht.get(L->to_address(-2)) = true;
                if (val_t != LuaValue::Table)
                {
                    dump_lua_value_norecursion(ofs, L, -2, key_t) << sv{": "};
                    dump_lua_value_norecursion(ofs, L, -1, val_t) << '\n';
                }
                else
                {
                    auto key = dump_lua_value_norecursion(ss{}, L, -2, key_t).str();
                    void* vp = L->to_address(-1);
                    ofs << key <<  sv{": Table-"} << vp << '\n';
                    if (!ht.get(vp))
                    {
                        bool match = depth == max_depth - 1;
                        if (!filter.empty() && !match && key_t == LuaValue::String)
                            for (auto& fstr : filter)
                                if (key.find(fstr) != -1) { match = true; break; }
                        if (!match) print_lua_value_to(ofs, L, -1, max_depth, show_meta, filter, ht, depth + 1);
                        else ht.get(vp) = true;
                    }
                }
                L->pop();
            }

            if (show_meta && L->load_metatable(idx))
            {
                void* mp = L->to_address(-1);
                ofs << pad << sv{">> __meta: Table-"} << mp << '\n';
                if (!ht.get(mp)) print_lua_value_to(ofs,  L, -1, max_depth, show_meta, filter, ht, depth + 1);
                L->pop();
            }
        }
    }

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
    LuaValue::LuaValue(const char* str) : str(Il2CppString::from(str)), type(String) {}

    LuaValue& LuaValue::operator=(const LuaValue& o)
    {
        if (this == &o) return *this;
        LuaState* main = lua_info().main;
        if (is_refer()) main->destroy_ref(ref);
        if (o.is_refer())
        {
            main->load_registry();
            if (main->load_field(-1, o.ref)) ref = main->pop_ref(-2);
            else type = Nil;
            main->pop(1);
            type = o.type;
        }
        else memcpy(this, &o, sizeof(LuaValue));
        return *this;
    }

    LuaValue::~LuaValue() noexcept
    {
        if (is_refer()) lua_info().main->destroy_ref(ref);
        type = Nil;
    }
}

namespace xyu
{
    bool LuaState::is_cclosure(int idx)
    {
        if (!lua_load().lua_iscfunction(this, idx)) return false;
        return ((GCfuncC*)to_address(idx))->nupvalues > 0;
    }
    bool LuaState::is_function(int idx)
    {
        if (!lua_load().lua_iscfunction(this, idx)) return false;
        return ((GCfuncC*)to_address(idx))->nupvalues == 0;
    }

    const char* LuaState::to_cstring(int idx, int* len)
    {
        int tmp;
        if (!len) len = &tmp;
        return lua_load().lua_tolstring(this, idx, len);
    }

    LuaValue LuaState::get(int idx)
    {
        LuaValue::Type t = type(idx);
        switch (t)
        {
            case LuaValue::Nil:             return {};
            case LuaValue::Boolean:         return {to_boolean(idx)};
            case LuaValue::LightUserData:   return {to_userdata(idx)};
            case LuaValue::Number:          return {to_number(idx)};
            case LuaValue::String:          return {to_string(idx)};
            case LuaValue::Callable:
                if (lua_load().lua_iscfunction(this, idx))
                {
                    if (((GCfuncC*)to_address(idx))->nupvalues == 0) return LuaValue{to_function(idx)};
                    t = LuaValue::CClosure;
                }
                else t = LuaValue::LClosure;
                break;
            default:;
        }
        return {create_ref(idx), t};
    }

    std::string LuaState::format(int idx, int max_depth, const vec_s& filter, bool show_meta)
    {
        if (max_depth == 0) return "";
        HashTable<void*, bool> ht;
        return dump_lua_value(std::stringstream{}, this, idx, max_depth, show_meta, filter, ht).str();
    }

    void LuaState::print(int idx, int max_depth, const vec_s& filter, bool show_meta)
    {
        if (max_depth == 0) { LOGI("  >> ..."); return; }
        HashTable<void*, bool> ht;
        std::stringstream ss;
        print_lua_value(ss, this, idx, max_depth, show_meta, filter, ht);
    }

    void LuaState::print_to(std::string name, int idx, int max_depth, const vec_s& filter, bool show_meta)
    {
        std::string path{PathConfig::DOWNLOAD.get()};
        path += name;

        std::filesystem::path fs_path(path);
        std::error_code ec;

        auto parent = fs_path.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent))
        {
            std::filesystem::create_directories(parent, ec);
            if (ec)
            {
                LOGW("  >> 文件夹创建失败: %s", ec.message().c_str());
                return;
            }
        }

        std::ofstream ofs(fs_path, std::ios::out | std::ios::trunc);
        if (ofs.is_open())
        {
            ofs << std::showbase;
            if (max_depth == 0) ofs << "  >> ...\n";
            else
            {
                HashTable<void*, bool> ht;
                print_lua_value_to(ofs, this, idx, max_depth, show_meta, filter, ht);
            }
            ofs.close();
        }
        else LOGW("  >> 文件打开失败: %s", path.c_str());
    }

    void LuaState::push(const char* str)
    {
        push(Il2CppString::from(str));
    }

    void LuaState::push(const LuaValue& value)
    {
        switch (value.type)
        {
            case LuaValue::Nil:             return push();
            case LuaValue::Boolean:         return push(value.b);
            case LuaValue::LightUserData:   return push(value.data);
            case LuaValue::Number:          return push(value.num);
            case LuaValue::String:          return push(value.str);
            case LuaValue::Function:        return push(value.fn);
            default:;
        }
        load_registry();
        load_field(-1, value.ref);
        remove(-2);
    }

    bool LuaState::load_field(int table_idx, Il2CppString* key, bool warn)
    {
        lua_load().lua_getfield(this, table_idx, key);
        if (is_nil(-1))
        {
            pop();
            if (warn) LOGW("  >> %s: %s", xyobfcstr("键未找到"), key->to_utf8().c_str());
            return false;
        }
        return true;
    }
    bool LuaState::load_field(int table_idx, const char* key, bool warn)
    {
        return load_field(table_idx, Il2CppString::from(key), warn);
    }
    bool LuaState::load_field(int table_idx, int n, bool warn)
    {
        lua_load().lua_rawgeti(this, table_idx, n);
        if (is_nil(-1))
        {
            pop();
            if (warn) LOGW("  >> %s: %d", xyobfcstr("键未找到"), n);
            return false;
        }
        return true;
    }
    bool LuaState::load_field(int table_idx, bool warn)
    {
        lua_load().lua_gettable(this, table_idx);
        if (is_nil(-1))
        {
            pop();
            if (warn) LOGW("  >> %s", xyobfcstr("键未找到"));
            return false;
        }
        return true;
    }
    bool LuaState::load_field(int table_idx, native_t, bool warn)
    {
        lua_load().lua_rawget(this, table_idx);
        if (is_nil(-1))
        {
            pop();
            if (warn) LOGW("  >> %s", xyobfcstr("键未找到"));
            return false;
        }
        return true;
    }

    void LuaState::store_field(int table_idx, const char* key)
    {
        store_field(table_idx, Il2CppString::from(key));
    }

    void push_table_copy_help(LuaState* L, int table_idx, HashTable<void*, int>& ht, int rid, const std::vector<std::string>& filter, bool copy_meta)
    {
        // 负索引 -> 正索引
        int tid = L->count() + table_idx + 1;

        // 建立一个新表
        L->create_table();
        int ntid = L->count();
        // 存引用
        L->push_copy(-1);
        ht.get(L->to_address(tid)) = L->pop_ref(rid);

        L->push();
        while (L->next(tid))
        {
            // [... | src_table | ... | new_table(ntid) | key(-2) | value(-1)]
            if (L->is_table(-2))
            {
                if (int stid = ht.get(L->to_address(-2))) L->load_field(rid, stid);
                else push_table_copy_help(L, -2, ht, rid, filter, copy_meta);
            }
            else L->push_copy(-2);
            // [... | src_table | ... | new_table(ntid) | key(-3) | value(-2) | key(-1)]
            if (L->is_table(-2))
            {
                if (int stid = ht.get(L->to_address(-2))) L->load_field(rid, stid);
                else
                {
                    bool match = false;
                    if (!filter.empty() && L->is_string(-1))
                    {
                        int len;
                        std::string key{L->to_cstring(-1, &len), (size_t)len};
                        for (auto& fstr : filter)
                            if (key.find(fstr) != -1) { match = true; break; }
                    }
                    if (match) L->push_copy(-2);
                    else push_table_copy_help(L, -2, ht, rid, filter, copy_meta);
                }
            }
            else L->push_copy(-2);
            // [... | src_table | ... | new_table(ntid) | key(-4) | value(-3) | key(-2) | value(-1)]
            L->store_field(ntid, native_v);
            // [... | src_table | ... | new_table(ntid) | key(-2) | value(-1)]
            L->pop(1);
        }

        if (L->load_metatable(tid))
        {
            if (!copy_meta) L->push_copy(-1);
            else if (int stid = ht.get(L->to_address(-1))) L->load_field(rid, stid);
            else push_table_copy_help(L, -1, ht, rid, filter, copy_meta);
            L->store_metatable(ntid);
            L->pop(1);
        }
    }
    bool LuaState::push_table_copy(int table_idx, const std::vector<std::string>& filter, bool copy_meta)
    {
        if (!is_table(table_idx)) return false;

        // 计算负索引
        int tid = table_idx > 0 ? table_idx - count() - 2
                                : table_idx - 1;

        HashTable<void*, int> ht;
        create_table(3);    // 引用表，预分配3个数组空间 (注册表+全局表+自身)
        int rid = count();

        load_registry();
        ht.get(to_address(-1)) = pop_ref(-2);
        load_globaltable();
        ht.get(to_address(-1)) = pop_ref(-2);

        push_table_copy_help(this, tid, ht, rid, filter, copy_meta);

        // 释放引用
        for (auto& kv : ht)
            if (kv.val) put_ref(-2, kv.val);

        pop_replace(-2);        // 替换引用表
        return true;
    }

    void LuaState::load_globaltable()
    {
        if (lua_info(this).has_gt_idx) push_copy(-10002);
        else
        {
            load_registry();
            load_field(-1, 2);
            pop_replace(-2);
        }
    }

    bool LuaState::load_global(const char* name)
    {
        return load_global(Il2CppString::from(name));
    }
    bool LuaState::load_global(Il2CppString* name)
    {
        lua_load().lua_getglobal(this, name);
        if (is_nil(-1))
        {
            pop();
            LOGW("  >> %s: %s", xyobfcstr("全局变量未找到"), name->to_utf8().c_str());
            return false;
        }
        return true;
    }

    void LuaState::store_global(const char* name)
    {
        return store_global(Il2CppString::from(name));
    }

    bool get_field_impl(LuaState* L, const char* path, const char* next_dot, bool warn)
    {
        const char* p;
        for (;;)
        {
            if (L->is_nil(-1))
            {
                if (warn)
                {
                    if (next_dot) LOGW("  >> %s: %s", xyobfcstr("表未找到"), get_part_str(path, next_dot).c_str());
                    else LOGW("  >> %s: %s", xyobfcstr("字段未找到"), path);
                }
                return false;
            }
            if (next_dot == nullptr) break;
            p = next_dot + 1;
            next_dot = strchr(p, '.');
            lua_load().lua_getfield(L, -1, get_part_il2str(p, next_dot));
        }
        return true;
    }

    bool get_field_help(LuaState* L, int table_idx, const char* path, bool warn = true)
    {
        const char* next_dot = strchr(path, '.');
        lua_load().lua_getfield(L, table_idx, get_part_il2str(path, next_dot));
        return get_field_impl(L, path, next_dot, warn);
    }
    bool LuaState::has_field(int table_idx, const char* path)
    {
        DeepGuard guard(this);
        return get_field_help(this, table_idx, path, false);
    }
    LuaValue LuaState::get_field(int table_idx, const char* path, bool warn)
    {
        DeepGuard guard(this);
        if (!get_field_help(this, table_idx, path, warn)) return {};
        return get(-1);
    }
    bool LuaState::get_field(int table_idx, const char* path, LuaValue* val)
    {
        DeepGuard guard(this);
        if (!get_field_help(this, table_idx, path, false)) return false;
        if (val) *val = get(-1);
        return true;
    }

    bool get_global_help(LuaState* L, const char* path, bool warn)
    {
        const char* next_dot = strchr(path, '.');
        lua_load().lua_getglobal(L, get_part_il2str(path, next_dot));
        return get_field_impl(L, path, next_dot, warn);
    }
    bool LuaState::has_global(const char* path)
    {
        DeepGuard guard(this);
        return get_global_help(this, path, false);
    }
    LuaValue LuaState::get_global(const char* path, bool warn)
    {
        DeepGuard guard(this);
        if (!get_global_help(this, path, warn)) return {};
        return get(-1);
    }
    bool LuaState::get_global(const char* path, LuaValue* val)
    {
        DeepGuard guard(this);
        if (!get_global_help(this, path, false)) return false;
        if (val) *val = get(-1);
        return true;
    }

    void set_field_help(LuaState* L, const char* path, const LuaValue& val, int table_idx, const char* final_key)
    {
        // [类型检测]
        LuaValue::Type old_tp = L->type(-1);
        if (old_tp != val.main_type())
        {
            LOGW("  >> %s: %s -> %s", xyobfcstr("字段类型替换"), get_type_name(old_tp), get_type_name(val.type));
        }

        // [注入值]
        L->push(val);
        if (table_idx < 0) --table_idx;
        L->store_field(table_idx , final_key);

        switch (val.type)
        {
            case LuaValue::Nil:     LOGI("  >> %s: %s -> %s",  xyobfcstr("字段修改成功"), path, "nil"); break;
            case LuaValue::Boolean: LOGI("  >> %s: %s -> %s",  xyobfcstr("字段修改成功"), path, val.b ? "true" : "false"); break;
            case LuaValue::Number:
       if ((int)val.num == val.num) LOGI("  >> %s: %s -> %d",  xyobfcstr("字段修改成功"), path, (int)val.num);
                               else LOGI("  >> %s: %s -> %g",  xyobfcstr("字段修改成功"), path, val.num); break;
            case LuaValue::String:  LOGI("  >> %s: %s -> %s",  xyobfcstr("字段修改成功"), path, val.str->to_utf8().data()); break;
            case LuaValue::LightUserData: LOGI("  >> %s: %s -> %p",xyobfcstr("字段修改成功"), path, val.data); break;
            case LuaValue::Function:LOGI("  >> %s: %s -> %p",  xyobfcstr("字段修改成功"), path, (void*)val.fn); break;
            default:                LOGI("  >> %s: %s",  xyobfcstr("字段修改成功"), path); break;
        }
    }
    bool LuaState::set_field(int table_idx, const char* path, const LuaValue& val)
    {
        const char* final_key = path;
        const char* last_dot = strrchr(path, '.');
        DeepGuard guard(this);
        if (last_dot)
        {
            std::string front(path, last_dot - path);
            if (!get_field_help(this, table_idx, front.c_str())) return false;
            final_key = last_dot + 1;
            table_idx = -2;
        }
        else if (table_idx < 0) --table_idx;
        push(final_key);
        lua_load().lua_rawget(this, table_idx);
        set_field_help(this, path, val, table_idx, final_key);
        return true;
    }
    bool LuaState::set_global(const char* path, const LuaValue& val)
    {
        DeepGuard guard(this);
        load_globaltable();
        return set_field(-1, path, val);
    }

    int check_newname(const char* newname)
    {
        if (!newname || !*newname) return 1;
        if (strcmp(newname, xyobfstr("__gc")) == 0) return -1;
        if (*newname >= '0' && *newname <= '9')
        {
            LOGW("  >> %s: %s", xyobfcstr("新字段名禁止以数字开头"), newname);
            return 0;
        }
        if (strchr(newname + 1, '.'))
        {
            LOGW("  >> %s: %s", xyobfcstr("新字段名禁止包含 '.'"), newname);
            return 0;
        }
        if (*newname == '.')
        {
            if (!*(newname + 1))
            {
                LOGW("  >> %s", xyobfcstr("新字段名前缀禁止为空"));
                return 0;
            }
            return 2;
        }
        return 3;
    }
    LuaValue LuaState::swap_field(int table_idx, const char* path, const LuaValue& newval, const char* newname)
    {
        int ntype = check_newname(newname);
        if (!ntype) return {};

        const char* final_key = path;
        const char* last_dot = strrchr(path, '.');
        DeepGuard guard(this);

        // 定位目标表
        if (last_dot)
        {
            std::string front(path, last_dot - path);
            if (!get_field_help(this, table_idx, front.c_str())) return {};
            final_key = last_dot + 1;
            table_idx = -2;
        }
        else if (table_idx < 0) --table_idx;

        // 获取旧值
        push(final_key);
        lua_load().lua_rawget(this, table_idx);
        LuaValue old_val = get(-1);

        // 保存旧值
        if (ntype != -1)
        {
            push_copy(-1);
            int cache_table_idx = (table_idx < 0) ? table_idx - 1 : table_idx;
            if (ntype == 3) store_field(cache_table_idx, newname);
            else
            {
                std::string cache_name = ntype == 1 ? xyobfstr("__old_") : newname + 1;
                cache_name += final_key;
                store_field(cache_table_idx, cache_name.c_str());
            }
        }

        // 设置新值
        set_field_help(this, path, newval, table_idx, final_key);

        return old_val;
    }
    LuaValue LuaState::swap_global(const char* path, const LuaValue& newval, const char* newname)
    {
        DeepGuard guard(this);
        load_globaltable();
        return swap_field(-1, path, newval, newname);
    }

    bool LuaState::revert_field(int table_idx, const char* path, const char* tmpname)
    {
        int ntype = check_newname(tmpname);
        if (ntype <= 0) return false;

        const char* final_key = path;
        const char* last_dot = strrchr(path, '.');
        DeepGuard guard(this);

        // 定位目标表
        if (last_dot)
        {
            std::string front(path, last_dot - path);
            if (!get_field_help(this, table_idx, front.c_str())) return false;
            final_key = last_dot + 1;
            table_idx = -2;
        }
        else if (table_idx < 0) --table_idx;

        // 计算缓存字段名
        std::string cache_name;
        if (ntype == 3) cache_name = tmpname;
        else
        {
            cache_name = (ntype == 1) ? xyobfstr("__old_") : tmpname + 1;
            cache_name += final_key;
        }

        // 获取缓存值
        push(cache_name.c_str());
        lua_load().lua_rawget(this, table_idx);
        // 缓存不存在，还原失败
        if (is_nil(-1))
        {
            LOGW(" >> %s: %s", xyobfcstr("未找到缓存值"),
                ((last_dot ? std::string(path, last_dot - path) : "") + cache_name).c_str());
            return false;
        }
        LuaValue tmpval = get(-1);
        pop();

        // 获取旧值
        push(final_key);
        lua_load().lua_rawget(this, table_idx);
        LuaValue old_val = get(-1);

        // 将缓存值写回原字段
        set_field_help(this, path, tmpval, table_idx, final_key);
        return true;
    }
    bool LuaState::revert_global(const char* path, const char* tmpname)
    {
        DeepGuard guard(this);
        load_globaltable();
        return revert_field(-1, path, tmpname);
    }

    bool LuaState::load_meta(int idx, const char* field)
    {
        return load_meta(idx, Il2CppString::from(field));
    }

    bool LuaState::call_meta(int idx, const char* field)
    {
        return call_meta(idx, Il2CppString::from(field));
    }

    Il2CppString* LuaState::pop_upvalue(int func_idx, int n)
    {
        Il2CppString* ret = lua_load().lua_setupvalue(this, func_idx, n);
        if (!ret) pop(1);
        return ret;
    }

    bool LuaState::pcall(int nargs, int nresults, int errfunc)
    {
        int ret = lua_load().lua_pcall(this, nargs, nresults, errfunc);
        if (errfunc == 0 && ret != 0)
        {
            LOGW(" >> %s: %s", xyobfcstr("函数调用失败"), to_string(-1)->to_utf8().data());
            pop(1);
        }
        return ret == 0;
    }

    int LuaState::get_param(int arg_idx)
    {
        if (lua_info(this).has_gt_idx) return -10002 - arg_idx;
        return -1001000 - arg_idx;
    }

    int LuaState::create_ref(int idx)
    {
        lua_info(this);
        load_registry();
        push_copy(idx < 0 ? idx - 1 : idx);
        int ref = pop_ref(-2);
        pop(1);
        return ref;
    }
    void LuaState::destroy_ref(int ref_id)
    {
        load_registry();
        put_ref(-1, ref_id);
        pop(1);
    }

    bool LuaState::exec(const char* code)
    {
        return exec(Il2CppString::from(code));
    }
    bool LuaState::exec(Il2CppString* code)
    {
        if (lua_load().luaL_dostring(this, code) == 0) return true;
        auto mess = to_cstring(-1);
        LOGW(" >> %s: %s", xyobfcstr("脚本执行失败"), mess);
        pop(1);
        return false;
    }

    bool LuaState::exec_file(const char* path)
    {
        return exec_file(Il2CppString::from(path));
    }
    bool LuaState::exec_file(Il2CppString* path)
    {
        if (lua_load().luaL_dofile(this, path) == 0) return true;
        auto mess = to_cstring(-1);
        LOGW(" >> %s: %s", xyobfcstr("脚本执行失败"), mess);
        pop(1);
        return false;
    }

    bool LuaState::push_script(const char* code)
    {
        return push_script(Il2CppString::from(code));
    }
    bool LuaState::push_script(Il2CppString* code)
    {
        if (lua_load().luaL_loadstring(this, code) == 0) return true;
        auto mess = to_cstring(-1);
        LOGW(" >> %s: %s", xyobfcstr("脚本执行失败"), mess);
        pop(1);
        return false;
    }

    bool LuaState::push_script_from(const char* path)
    {
        return push_script_from(Il2CppString::from(path));
    }
    bool LuaState::push_script_from(Il2CppString* path)
    {
        if (lua_load().luaL_loadfile(this, path) == 0) return true;
        auto mess = to_cstring(-1);
        LOGW(" >> %s: %s", xyobfcstr("脚本执行失败"), mess);
        pop(1);
        return false;
    }

}

namespace xyu
{
    constexpr LuaValue::Type HT = static_cast<LuaValue::Type>(LuaValue::UserData + (0x66 << 4));
    constexpr LuaValue::Type Arr = static_cast<LuaValue::Type>(LuaValue::UserData + (0x67 << 4));

    using HT_t = HashTable<std::string, LuaValue>;
    using Arr_t = std::vector<LuaValue>;

    HashTable<void*, HashTable<std::string, LuaValue>> ht;

    void release_splv(LuaValue& val)
    {
        if (val.type == HT)
        {
            // delete HT_t 会自动销毁内部的 HashNode，从而完美触发递归
            delete static_cast<HT_t*>(val.data);
        }
        else if (val.type == Arr)
        {
            auto arr_ptr = static_cast<Arr_t*>(val.data);
            // 必须在 delete vector 前，手动清理每个元素
            for (auto& item : *arr_ptr) release_splv(item);
            delete arr_ptr;
        }

        // 清理完毕，安全重置为 Nil
        val.type = LuaValue::Nil;
    }

    template <>
    struct HashNode<std::string, LuaValue>
    {
        std::string key;
        LuaValue val;

        ~HashNode() noexcept { release_splv(val); }
    };

    LuaValue hook_field_help(LuaState* L, int table_idx, const char* path, const LuaValue& newval, HT_t* current_ht)
    {
        const char* final_key = path;
        const char* last_dot = strrchr(path, '.');

        // ================= 1. 定位目标表 =================
        if (last_dot)
        {
            std::string front(path, last_dot - path);
            if (!get_field_help(L, table_idx, front.c_str())) return {};
            final_key = last_dot + 1;
            table_idx = -2;
        }
        else if (table_idx < 0) --table_idx;

        // ================= 2. 获取旧值 =================
        L->push(final_key);
        lua_load().lua_rawget(L, table_idx);
        LuaValue old_val = L->get(-1);

        // ================= 3. 拆分路径并构建/遍历嵌套表 =================
        std::string p = path;
        size_t start = 0;
        size_t end = p.find('.');

        while (end != std::string::npos)
        {
            std::string part = p.substr(start, end - start);

            // 获取当前层级的节点
            LuaValue& node = current_ht->get(part);

            if (node.type == HT)
            {
                // 本身就是表，直接潜入
                current_ht = static_cast<HT_t*>(node.data);
            }
            else if (node.type == Arr)
            {
                Arr_t* arr = static_cast<Arr_t*>(node.data);
                // 如果是数组，必须检查当前最新版本是不是表
                if (!arr->empty() && arr->back().type == HT)
                {
                    current_ht = static_cast<HT_t*>(arr->back().data);
                }
                else
                {
                    // 最新版本不是表（比如被覆盖成了普通值），则追加一张新表作为当前路径
                    LuaValue new_ht_val;
                    new_ht_val.type = HT;
                    new_ht_val.data = new HT_t();
                    arr->emplace_back(std::move(new_ht_val));
                    current_ht = static_cast<HT_t*>(arr->back().data);
                }
            }
            else if (!node.is_nil())
            {
                // 普通值，包裹进数组，并追加一张新表作为下潜路径
                Arr_t* arr = new Arr_t();
                arr->emplace_back(std::move(node));

                LuaValue new_ht_val;
                new_ht_val.type = HT;
                new_ht_val.data = new HT_t();
                arr->emplace_back(std::move(new_ht_val));

                node.type = Arr;
                node.data = arr;
                current_ht = static_cast<HT_t*>(arr->back().data);
            }
            else
            {
                // 全新节点，直接开辟新表
                node.type = HT;
                node.data = new HT_t();
                current_ht = static_cast<HT_t*>(node.data);
            }

            start = end + 1;
            end = p.find('.', start);
        }

        std::string final_part = p.substr(start);

        // ================= 4. 存储旧版本 =================

        if (!old_val.is_nil())
        {
            LuaValue& node = current_ht->get(final_part);

            if (node.is_nil()) node = old_val;
            else if (node.type == Arr) static_cast<Arr_t*>(node.data)->emplace_back(old_val);
            else
            {
                Arr_t* arr = new Arr_t();
                arr->emplace_back(std::move(node));
                arr->emplace_back(old_val);
                node.type = Arr;
                node.data = arr;
            }
        }

        // ================= 5. 更新 Lua 中的新值 =================
        set_field_help(L, path, newval, table_idx, final_key);

        return old_val;
    }
    LuaValue LuaState::hook_field(int table_idx, const char* path, const LuaValue& newv)
    {
        DeepGuard guard(this);
        void* root_ptr = to_address(table_idx);
        return hook_field_help(this, table_idx, path, newv, &ht.get(root_ptr));
    }
    LuaValue LuaState::hook_global(const char* path, const LuaValue& newval)
    {
        DeepGuard guard(this);
        load_globaltable();
        return hook_field_help(this, -1, path, newval, &ht.get((void*)(intptr_t)3));
    }

    bool unhook_field_help(LuaState* L, int table_idx, const char* path, int revert, HT_t* current_ht)
    {
        if (revert <= 0) return false;

        const char* final_key = path;
        const char* last_dot = strrchr(path, '.');

        // ================= 1. 定位目标表 =================
        if (last_dot)
        {
            std::string front(path, last_dot - path);
            if (!get_field_help(L, table_idx, front.c_str())) return false;
            final_key = last_dot + 1;
            table_idx = -2;
        }
        else if (table_idx < 0) --table_idx;

        // ================= 2. 获取 Lua 当前值并拦截不存在的路径 =================
        L->push(final_key);
        lua_load().lua_rawget(L, table_idx);

        // ================= 3. 拆分路径遍历 C++ 哈希表 =================
        std::string p = path;
        size_t start = 0;
        size_t end = p.find('.');

        while (end != std::string::npos)
        {
            std::string part = p.substr(start, end - start);
            LuaValue& node = current_ht->get(part);

            // 中途路径不存在，直接拦截
            if (node.is_nil()) return false;

            if (node.type == HT) current_ht = static_cast<HT_t*>(node.data);
            else if (node.type == Arr)
            {
                Arr_t* arr = static_cast<Arr_t*>(node.data);
                if (arr->empty() || arr->back().type != HT) return false;
                current_ht = static_cast<HT_t*>(arr->back().data);
            }
            else return false; // 中途节点既不是表也不是包含表的数组

            start = end + 1;
            end = p.find('.', start);
        }

        std::string final_part = p.substr(start);
        LuaValue& node = current_ht->get(final_part);

        // ================= 4. 版本回退判定 =================
        // 没有历史记录，或者是纯粹的树结构表，无法还原出有效值
        if (node.is_nil() || node.type == HT) return false;

        LuaValue restored_val;

        if (node.type == Arr)
        {
            Arr_t* arr = static_cast<Arr_t*>(node.data);
            if (arr->empty()) return false; // 空数组无值可退

            int valid_steps = 0;
            for (auto it = arr->end() - 1; it >= arr->begin(); --it)
            {
                if (it->type != HT)
                {
                    valid_steps++;
                    restored_val = std::move(*it);
                }
                else release_splv(*it);
                arr->erase(it);
                if (valid_steps == revert) break;
            }
            if (restored_val.is_nil()) return false;
        }
        else
        {
            // 单个值的情况，它只有唯一的前世
            restored_val = std::move(node);
            node.type = LuaValue::Nil;
        }

        // ================= 5. 将旧版本强制覆盖回 Lua =================
        set_field_help(L, path, restored_val, table_idx, final_key);

        return true;
    }
    bool LuaState::unhook_field(int table_idx, const char* path, int revert)
    {
        DeepGuard guard(this);
        void* root_ptr = to_address(table_idx);
        return unhook_field_help(this, table_idx, path, revert, &ht.get(root_ptr));
    }
    bool LuaState::unhook_global(const char* path, int revert)
    {
        DeepGuard guard(this);
        load_globaltable();
        return unhook_field_help(this, -1, path, revert, &ht.get((void*)(intptr_t)3));
    }

}

namespace xyu
{
    LuaNativeHandle lua_load()
    {
        static LuaNativeHandle funcs = []
        {
            LuaNativeHandle lnfuncs;

        #define XY_name_to_string( name )
        #define XY_lua_assign_gen( name ) \
            lnfuncs.name = decltype(lnfuncs.name)(Il2Cpp::get_lua_method( #name ));

        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_newthread, tolua_getmainstate)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_newthread, lua_status, lua_yield, lua_resume)

        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_gettop, lua_settop, lua_checkstack)

        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_type, tolua_isint64, lua_iscfunction)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_toboolean, lua_tostring, lua_tolstring)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_tonumber, lua_touserdata, tolua_toint64)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_tocfunction, lua_tothread, lua_topointer, lua_concat)

        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_pushnil, lua_pushboolean, lua_pushvalue)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_pushnumber, tolua_pushint64)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_pushstring, lua_pushlightuserdata, lua_pushcclosure)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_insert, lua_replace, lua_remove, lua_xmove)

        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_gettable, lua_getfield, lua_rawget, lua_rawgeti)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_settable, lua_setfield, lua_rawset, lua_rawseti)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_next, lua_getregistry)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_getmetatable, lua_setmetatable, lua_getglobal, lua_setglobal)

        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_call, lua_pcall, lua_upvalueindex, lua_setupvalue, lua_getupvalue)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, luaL_throw, luaL_argerror, luaL_typerror)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_getstack, lua_getinfo)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_getlocal, lua_setlocal, lua_sethook)

        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_newuserdata, lua_createtable)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, lua_equal, lua_objlen, lua_lessthan, lua_rawequal)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, luaL_getmetafield, luaL_callmeta)

        XY_PP_FORWARD(XY_lua_assign_gen, 1, luaL_ref, luaL_unref)

        XY_PP_FORWARD(XY_lua_assign_gen, 1, luaL_dostring, luaL_loadstring, luaL_dofile, luaL_loadfile)

        XY_PP_FORWARD(XY_lua_assign_gen, 1, luaL_checktype, luaL_checknumber, luaL_checkinteger)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, luaL_checklstring, luaL_checkudata)
        XY_PP_FORWARD(XY_lua_assign_gen, 1, luaL_openlibs, lua_gc)

            return lnfuncs;
        }();
        return funcs;
    }

}
