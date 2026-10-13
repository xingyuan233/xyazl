#pragma once

#include <string>
#include <vector>

#include "./type.h"

/// LuaJit 结构体
namespace xyu
{
    union TValue
    {
        double n;
        uint64 g;
    };

    // 注意： 直接继承或作为成员会产生填充
    struct GCObjHeader
    {
        struct
        GCObj* nextgc;     // 指向下一个 GC 对象
        uint8 marked;      // GC 状态标记
        uint8 gct;         // 类型标记 (Type Tag)
    };

    struct GCstr
    {
        GCObj* nextgc;
        uint8 marked;
        uint8 gct;
        uint8 reserved;    // 预留位 (用于关键词等)
        uint8 unused;
        uint32 hash;       // 字符串哈希值
        uint32 len;        // 字符串长度
        char data[];       // 字符数据
    };

    struct GCtab
    {
        GCObj* nextgc;
        uint8 marked;
        uint8 gct;
        uint8 nomm;        // 元方法缺失位标记
        uint8 colo;        // 颜色与分配优化标记
        void* array;       // 指向数组部分的指针
        void* node;        // 指向哈希部分的指针
        GCtab* metatable;  // 元表指针
        uint32 asize;      // 数组大小
        uint32 hmask;      // 哈希掩码 (size = hmask + 1)
    };

    struct GCupval
    {
        GCObj* nextgc;
        uint8 marked;
        uint8 gct;
        uint8 closed;        // 是否已关闭 (0: open, 1: closed)
        uint8 immutable;     // 是否为不可变 (用于优化)

        union
        {
            TValue tv;      // 当闭包 closed 时，值存这里
            struct
            {
                void* prev; // 4 字节
                void* next; // 4 字节
            } l;
        } u;

        TValue* v;          // 真正的 v 指针

        uint32 dhash;
    };

    struct GCproto
    {
        GCObj* nextgc;
        uint8 marked;
        uint8 gct;
        uint8 numparams;    // 参数个数
        uint8 framesize;    // 栈帧大小
        uint32 sizebc;      // 字节码指令的数量

#if !defined(__i386__)
        uint32 unused_gc64;
#endif
        void* gclist;
        void* k;
        void* uv;

        uint32 sizekgc;
        uint32 sizekn;
        uint32 sizept;

        uint8 sizeuv;
        uint8 flags;

        // ...
    };

    struct GCfuncL
    {
        GCObj* nextgc;
        uint8 marked;
        uint8 gct;
        uint8 ffid;        // Fast Function ID
        uint8 nupvalues;   // Upvalue 数量
        void* env;         // 环境表 (通常是全局表)
        void* gclist;      // 用于 GC 的链表
        GCproto* pc;       // 指向原型 (GCproto) 的指令起始处
        GCupval* uvptr[1]; // Upvalue 指针数组 (动态长度)
    };

    struct GCfuncC
    {
        GCObj* nextgc;
        uint8 marked;
        uint8 gct;
        uint8 ffid;
        uint8 nupvalues;
        void* env;
        void* gclist;
        void* pc;
        void* f;           // 真正的 C 函数指针
        TValue upvalue[1]; // 直接存储的 Upvalue 值数组 (动态长度)
    };
}

/// 自定义 Lua 结构体
namespace xyu
{
    struct LuaState;
    struct Il2CppString;

    struct LuaNativeHandle
    {
        LuaState*    (*lua_newthread)(LuaState*);
        LuaState*    (*tolua_getmainstate)(LuaState*);
        int          (*lua_status)(LuaState*);
        int          (*lua_yield)(LuaState*, int);
        int          (*lua_resume)(LuaState*, LuaState*, int);
        int          (*lua_gettop)(LuaState*);
        void         (*lua_settop)(LuaState*, int);
        int          (*lua_checkstack)(LuaState*, int);
        int          (*lua_type)(LuaState*, int);
        bool         (*tolua_isint64)(LuaState*, int);
        bool         (*lua_iscfunction)(LuaState*, int);
        bool         (*lua_toboolean)(LuaState*, int);
        int64        (*tolua_toint64)(LuaState*, int);
        double       (*lua_tonumber)(LuaState*, int);
        Il2CppString*(*lua_tostring)(LuaState*, int);
        const char*  (*lua_tolstring)(LuaState*, int, int*);
        void*        (*lua_touserdata)(LuaState*, int);
        int          (*(*lua_tocfunction)(LuaState*, int))(LuaState*);
        LuaState*    (*lua_tothread)(LuaState*, int);
        void*        (*lua_topointer)(LuaState*, int);
        void         (*lua_concat)(LuaState*, int);
        void         (*lua_pushnil)(LuaState*);
        void         (*lua_pushboolean)(LuaState*, int);
        void         (*tolua_pushint64)(LuaState*, int64);
        void         (*lua_pushnumber)(LuaState*, double);
        void         (*lua_pushstring)(LuaState*, Il2CppString*);
        void         (*lua_pushlightuserdata)(LuaState*, void*);
        void         (*lua_pushcclosure)(LuaState*, int(*)(LuaState*), int);
        void         (*lua_pushvalue)(LuaState*, int);
        void         (*lua_insert)(LuaState*, int);
        void         (*lua_replace)(LuaState*, int);
        void         (*lua_remove)(LuaState*, int);
        void         (*lua_xmove)(LuaState*, LuaState*, int);
        void         (*lua_gettable)(LuaState*, int);
        void         (*lua_getfield)(LuaState*, int, Il2CppString*);
        void         (*lua_rawget)(LuaState*, int);
        void         (*lua_rawgeti)(LuaState*, int, int);
        void         (*lua_settable)(LuaState*, int);
        void         (*lua_setfield)(LuaState*, int, Il2CppString*);
        void         (*lua_rawset)(LuaState*, int);
        void         (*lua_rawseti)(LuaState*, int, int);
        int          (*lua_next)(LuaState*, int);
        int          (*lua_getmetatable)(LuaState*, int);
        void         (*lua_setmetatable)(LuaState*, int);
        void         (*lua_getregistry)(LuaState*);
        void         (*lua_getglobal)(LuaState*, Il2CppString*);
        void         (*lua_setglobal)(LuaState*, Il2CppString*);
        void         (*lua_call)(LuaState*, int, int);
        int          (*lua_pcall)(LuaState*, int, int, int);
        int          (*lua_upvalueindex)(LuaState*, int);
        Il2CppString*(*lua_setupvalue)(LuaState*, int, int);
        Il2CppString*(*lua_getupvalue)(LuaState*, int, int);
        int          (*luaL_throw)(LuaState*, Il2CppString*);
        int          (*luaL_argerror)(LuaState*, int, Il2CppString*);
        int          (*luaL_typerror)(LuaState*, int, Il2CppString*);
        int          (*lua_getstack)(LuaState*, int, void*);
        int          (*lua_getinfo)(LuaState*, Il2CppString*, void*);
        Il2CppString*(*lua_getlocal)(LuaState*, void*, int);
        Il2CppString*(*lua_setlocal)(LuaState*, void*, int);
        int          (*lua_sethook)(LuaState*, void*, int, int);
        void*        (*lua_newuserdata)(LuaState*, size_t);
        void         (*lua_createtable)(LuaState*, int, int);
        int          (*lua_objlen)(LuaState*, int);
        int          (*lua_equal)(LuaState*, int, int);
        int          (*lua_lessthan)(LuaState*, int, int);
        int          (*lua_rawequal)(LuaState*, int, int);
        int          (*luaL_getmetafield)(LuaState*, int, Il2CppString*);
        int          (*luaL_callmeta)(LuaState*, int, Il2CppString*);
        int          (*luaL_ref)(LuaState*, int);
        void         (*luaL_unref)(LuaState*, int, int);
        bool         (*luaL_dostring)(LuaState*, Il2CppString*);
        int          (*luaL_loadstring)(LuaState*, Il2CppString*);
        bool         (*luaL_dofile)(LuaState*, Il2CppString*);
        int          (*luaL_loadfile)(LuaState*, Il2CppString*);
        void         (*luaL_openlibs)(LuaState*);
        void         (*luaL_checktype)(LuaState*, int, int);
        Il2CppString*(*luaL_checklstring)(LuaState*, int, int*);
        double       (*luaL_checknumber)(LuaState*, int);
        int          (*luaL_checkinteger)(LuaState*, int);
        void*        (*luaL_checkudata)(LuaState*, int, Il2CppString*);
        int          (*lua_gc)(LuaState*, int, int);
    };
    // 通过 so 直接获取的原始函数
    LuaNativeHandle lua_load();

    /// Lua 值对象
    struct LuaValue
    {
        // 类型枚举
        enum Type : int
        {
            Nil = 0,            // 空值
            Boolean = 1,        // 布尔值
            LightUserData = 2,  // 轻量用户数据
            Number = 3,         // 双精度浮点数
            String = 4,         // 字符串
            Table = 5,          // 表
            Callable = 6,       // 可调用对象
            UserData = 7,       // 完整用户数据
            Coroutine = 8,      // 协程

            _MASK = 0xf,        // 类型掩码

            LClosure = Callable + (1 << 4), // Lua 闭包
            CClosure = Callable + (2 << 4), // C 闭包
            Function = Callable + (8 << 4), // C 函数
        };

        // 数据
        union
        {
            nullptr_t       nil;        // Nil
            bool            b;          // Boolean
            int             b2;         // Boolean
            void*           data;       // LightUserData
            double          num;        // Number
            Il2CppString*   str;        // String
            int(*fn)(LuaState*);        // Function
            int             ref;        // Table | LClosure | CClosure | UserData | Coroutine
        };
        Type type;                      // 实际类型

        // 构造函数 - Nil
        LuaValue() noexcept : nil{}, type{Nil} {}
        // 构造函数 - bool
        LuaValue(bool b) noexcept : b{b}, type{Boolean} {}
        // 构造函数 - LightUserData
        LuaValue(void* data) noexcept : data{data}, type{LightUserData} {}
        // 构造函数 - number
        LuaValue(double num) noexcept : num{num}, type{Number} {}
        LuaValue(int num) noexcept : num(num), type{Number} {}
        LuaValue(uint num) noexcept : num(num), type{Number} {}
        // 构造函数 - String
        LuaValue(Il2CppString* str) noexcept : str{str}, type{String} {}
        // 构造函数 - const char*
        LuaValue(const char* str);
        // 构造函数 - Function
        LuaValue(int(*fn)(LuaState*)) noexcept : fn{fn}, type{Function} {}
        // 构造函数 - Table | Closure | UserData | Coroutine
        LuaValue(int ref, Type type) noexcept : ref{ref}, type{type} {}

        // 移动构造
        LuaValue(LuaValue&& o) noexcept { memcpy(this, &o, sizeof(LuaValue)); o.type = Nil; }
        // 移动赋值
        LuaValue& operator=(LuaValue&& o) noexcept
        {LuaValue{0};
            this->~LuaValue();
            memcpy(this, &o, sizeof(LuaValue));
            o.type = Nil;
            return *this;
        }

        // 拷贝构造
        LuaValue(const LuaValue& o) : LuaValue() { operator=(o); }
        // 拷贝赋值
        LuaValue& operator=(const LuaValue& o);

        // 析构函数
        ~LuaValue() noexcept;

        int main_type() const noexcept { return type & _MASK; }
        bool is_nil() const noexcept { return type == Nil; }
        bool is_boolean() const noexcept { return type == Boolean; }
        bool is_lightdata() const noexcept { return type == LightUserData; }
        bool is_number() const noexcept { return type == Number; }
        bool is_string() const noexcept { return type == String; }
        bool is_callable() const noexcept { return main_type() == Callable; }
        bool is_table() const noexcept { return type == Table; }
        bool is_function() const noexcept { return type == Function; }
        bool is_cclosure() const noexcept { return type == CClosure; }
        bool is_lclosure() const noexcept { return type == LClosure; }
        bool is_userdata() const noexcept { return type == UserData; }
        bool is_coroutine() const noexcept { return type == Coroutine; }
        bool is_refer() const noexcept { return type >= Table && type < Function; }
    };

    /// 协程状态机
    struct LuaState
    {
        // ==================== 1. 协程与状态 ====================

        // 创建新协程并返回其状态
        LuaState* create_coroutine() { return lua_load().lua_newthread(this); }
        // 获取主协程
        LuaState* main_coroutine() { return lua_load().tolua_getmainstate(this); }

        // 返回协程状态码
        int status() { return lua_load().lua_status(this); }

        // 挂起当前协程，nresults 为返回给恢复者的结果个数
        int yield(int nresults) { return lua_load().lua_yield(this, nresults); }
        // 恢复协程执行，nargs 为传递给协程的参数个数
        int resume(LuaState* coro, int nargs) { return lua_load().lua_resume(coro, this, nargs); }


        // ==================== 2. 空间管理 ====================

        // 栈是否为空
        bool empty() { return count() == 0; }
        // 返回元素个数
        int count() { return lua_load().lua_gettop(this); }

        // 确保栈有至少 n 个空闲位置，返回是否成功
        bool reserve(int n) { return lua_load().lua_checkstack(this, n) != 0; }
        // 设置元素个数 (缩容 或 nil填充)
        void resize(int n) { lua_load().lua_settop(this, n); }


        // ==================== 3. 类型查询与获取 ====================

        // 返回指定索引值的类型码
        LuaValue::Type type(int idx) { return static_cast<LuaValue::Type>(lua_load().lua_type(this, idx)); }

        // 判断是否为 nil
        bool is_nil(int idx) { return type(idx) == LuaValue::Nil; }
        // 判断是否为布尔值
        bool is_boolean(int idx) { return type(idx) == LuaValue::Boolean; }
        // 判断是否为轻量用户数据
        bool is_lightUserData(int idx) { return type(idx) == LuaValue::LightUserData; }
        // 判断是否为浮点数
        bool is_number(int idx) { return type(idx) == LuaValue::Number; }
        // 判断是否为字符串
        bool is_string(int idx) { return type(idx) == LuaValue::String; }
        // 判断是否为表
        bool is_table(int idx) { return type(idx) == LuaValue::Table; }
        // 判断是否为可调用对象
        bool is_callable(int idx) { return type(idx) == LuaValue::Callable; }
        // 判断是否为 Lua 闭包
        bool is_lclosure(int idx) { return is_callable(idx) && !lua_load().lua_iscfunction(this, idx); }
        // 判断是否为 C 闭包
        bool is_cclosure(int idx);
        // 判断是否为 C 函数
        bool is_function(int idx);
        // 判断是否为完整用户数据
        bool is_userdata(int idx) { return type(idx) == LuaValue::UserData; }
        // 判断是否为协程
        bool is_coroutine(int idx) { return type(idx) == LuaValue::Coroutine; }

        // 将指定索引的值按 Lua 规则转换为布尔值
        bool to_boolean(int idx) { return lua_load().lua_toboolean(this, idx); }
        // 将指定索引的值转换为 double
        double to_number(int idx) { return lua_load().lua_tonumber(this, idx); }
        // 将指定索引的值转换为 utf16 字符串
        Il2CppString* to_string(int idx) { return lua_load().lua_tostring(this, idx); }
        // 将指定索引的值转换为 C 字符串
        const char* to_cstring(int idx, int* len = nullptr);
        // 将指定索引的值转换为 C 函数
        auto* to_function(int idx) { return lua_load().lua_tocfunction(this, idx); }
        // 将指定索引的值转换为 lightUserData 或 userData 指针
        void* to_userdata(int idx) { return lua_load().lua_touserdata(this, idx); }
        // 将指定索引的值转换为协程状态
        LuaState* to_coroutine(int idx) { return lua_load().lua_tothread(this, idx); }
        // 将指定索引的值转换为原始地址
        void* to_address(int idx) { return lua_load().lua_topointer(this, idx); }

        // 将栈顶的 n 个值拼接为字符串（触发 __concat）
        void concat_to_string(int n) { lua_load().lua_concat(this, n); }

        // 获取指定索引的值
        LuaValue get(int idx);

        // 将指定索引的值变为字符串
        std::string format(int idx, int max_depth = 3, const std::vector<std::string>& filter = {}, bool show_meta = false);

        // 输出值
        void print(int idx, int max_depth = 3, const std::vector<std::string>& filter = {}, bool show_meta = false);
        // 输出值到指定文件 (键为 filter 的表不会展开)
        void print_to(std::string name, int idx, int max_depth = 3, const std::vector<std::string>& filter = {}, bool show_meta = false);


        // ==================== 4. 栈操作 ====================

        // 压入 nil
        void push() { lua_load().lua_pushnil(this); }
        // 压入 boolean
        void push(bool boolean) { lua_load().lua_pushboolean(this, boolean); }
        // 压入 LightUserData
        void push(void* ptr) { lua_load().lua_pushlightuserdata(this, ptr); }
        // 压入 数值
        void push(double value) { lua_load().lua_pushnumber(this, value); }
        void push(int value) { lua_load().lua_pushnumber(this, value); }
        void push(uint value) { lua_load().lua_pushnumber(this, value); }
        // 压入 utf16 字符串
        void push(Il2CppString* str) { lua_load().lua_pushstring(this, str); }
        // 压入 c 字符串
        void push(const char* str);
        // 压入 C 函数
        void push(int(*fn)(LuaState*)) { return push(fn, 0); }
        // 压入带 n 个上值的 C 闭包
        void push(int(*fn)(LuaState*), int n) { lua_load().lua_pushcclosure(this, fn, n); }
        // 将指定索引处的值浅拷贝一份压入栈顶
        void push_copy(int idx) { lua_load().lua_pushvalue(this, idx); }

        // 弹出 n 个元素
        void pop(int n = 1) { lua_load().lua_settop(this, -n - 1); }
        // 将栈顶元素移动到指定索引处
        void pop_insert(int idx) { lua_load().lua_insert(this, idx); }
        // 弹出栈顶值覆盖指定索引处的值
        void pop_replace(int idx) { lua_load().lua_replace(this, idx); }

        // 移除指定索引处的元素
        void remove(int idx) { lua_load().lua_remove(this, idx); }

        // 将其他协程栈顶的 n 个元素 平移 到当前协程栈顶
        void take(LuaState* from, int n) { lua_load().lua_xmove(from, this, n); }

        // 压入任意值
        void push(const LuaValue& value);
        // 设置索引为任意值
        void set(int idx, const LuaValue& value) { push(value); pop_replace(idx < 0 ? idx - 1 : idx); }


        // ==================== 5. 表操作 ====================

        // 从表中读取键对应的值，若找到则压栈，返回是否压栈（触发 __index）
        bool load_field(int table_idx, Il2CppString* key, bool warn = true);
        // 从表中读取键对应的值，若找到则压栈，返回是否压栈（触发 __index）
        bool load_field(int table_idx, const char* key, bool warn = true);
        // 从表中读取索引对应的值，若找到则压栈，返回是否压栈
        bool load_field(int table_idx, int n, bool warn = true);
        // 弹出栈顶键，从指定索引的表中读取值，若找到则压栈，返回是否压栈（触发 __index）
        bool load_field(int table_idx, bool warn = true);
        // 弹出栈顶键，从指定索引的表中读取值，若找到则压栈，返回是否压栈
        bool load_field(int table_idx, native_t, bool warn = true);

        // 弹出栈顶值，与键一起存入表中（触发 __newindex）
        void store_field(int table_idx, Il2CppString* key) { lua_load().lua_setfield(this, table_idx, key); }
        // 弹出栈顶值，与键一起存入表中（触发 __newindex）
        void store_field(int table_idx, const char* key);
        // 弹出栈顶值，存入索引对应的值
        void store_field(int table_idx, int n) { lua_load().lua_rawseti(this, table_idx, n); }
        // 弹出键和值，设置到指定索引的表中（触发 __newindex）
        void store_field(int table_idx) { lua_load().lua_settable(this, table_idx); }
        // 弹出键和值，设置到指定索引的表中
        void store_field(int table_idx, native_t) { lua_load().lua_rawset(this, table_idx); }
        // 将键值对存入表中（触发 __newindex）
        void store_field(int table_idx, const char* key, const LuaValue& val) { push(val); store_field(table_idx < 0 ? table_idx - 1 : table_idx, key); }
        // 将键值对存入表中
        void store_field(int table_idx, int n, const LuaValue& val) { push(val); store_field(table_idx < 0 ? table_idx - 1 : table_idx, n); }

        // 表遍历：弹出栈顶键，压入下一对键值，返回是否还有元素
        bool next(int table_idx) { return lua_load().lua_next(this, table_idx) != 0; }

        // 浅拷贝一个表并压入栈顶，返回是否成功，filter 为浅拷贝值为元表的子键，copy_meta 为是否深拷贝元表（默认浅拷贝）
        bool push_table_copy(int table_idx, const std::vector<std::string>& filter = {}, bool copy_meta = false);

        // 获取指定对象的元表并压栈，返回是否成功
        bool load_metatable(int table_idx) { return lua_load().lua_getmetatable(this, table_idx) != 0; }
        // 弹出栈顶值作为元表，设置到指定索引的对象上
        void store_metatable(int table_idx) { lua_load().lua_setmetatable(this, table_idx); }

        // 将注册表压栈
        void load_registry() { lua_load().lua_getregistry(this); }
        // 将全局表压栈
        void load_globaltable();

        // 查找全局变量，若找到则压栈，返回是否找到
        bool load_global(Il2CppString* name);
        // 查找全局变量，若找到则压栈，返回是否找到
        bool load_global(const char* name);
        // 弹出栈顶值并设置为全局变量
        void store_global(Il2CppString* name) { lua_load().lua_setglobal(this, name); }
        // 弹出栈顶值并设置为全局变量
        void store_global(const char* name);
        // 将键值对存入全局表中
        void store_global(const char* name, const LuaValue& val) { push(val); store_global(name); }

        // 获取表中的 XX.YY.zz 字段的值，失败返回 nil
        LuaValue get_field(int table_idx, const char* path, bool warn = true);
        // 获取全局表中的 XX.YY.zz 字段的值，失败返回 nil
        LuaValue get_global(const char* path, bool warn = true);

        // 检测表中是否存在 XX.YY.zz 字段
        bool has_field(int table_idx, const char* path);
        // 检测全局表中是否存在 XX.YY.zz 字段
        bool has_global(const char* path);

        // 获取表中的 XX.YY.zz 字段的值，返回是否存在
        bool get_field(int table_idx, const char* path, LuaValue* val);
        // 获取全局表中的 XX.YY.zz 字段的值，返回是否存在
        bool get_global(const char* path, LuaValue* val);

        // 将表中的 XX.YY.zz 字段设置为 val，返回是否成功
        bool set_field(int table_idx, const char* path, const LuaValue& val);
        // 将全局表中的 XX.YY.zz 字段设置为 val，返回是否成功
        bool set_global(const char* path, const LuaValue& val);

        // 将表中的 XX.YY.zz 字段替换为 val，缓存为 XX.YY.newname (默认为 __old_zz) 并返回原值
        // newname 为 "__gc" 则不缓存直接返回值，为 ".xxx" 则在原名基础上加上 xxx 前缀
        // 否则存在 '.' 或 以数字开头 会导致失败不进行字段修改并返回 nil
        LuaValue swap_field(int table_idx, const char* path, const LuaValue& newval, const char* newname = nullptr);
        // 将全局表中的 XX.YY.zz 字段设置为 val，缓存为 XX.YY.newname (默认为 __old_zz) 并返回原值
        // newname 为 "__gc" 则不缓存直接返回值，为 ".xxx" 则在原名基础上加上 xxx 前缀
        // 否则存在 '.' 或 以数字开头 会导致失败不进行字段修改并返回 nil
        LuaValue swap_global(const char* path, const LuaValue& newval, const char* newname = nullptr);

        // 将表中的 XX.YY.zz 字段还原为缓存值，tmpname 模式参考 swap_field，返回是否成功
        bool revert_field(int table_idx, const char* path, const char* tmpname = nullptr);
        // 将全局表中的 XX.YY.zz 字段还原为缓存值，tmpname 模式参考 swap_global，返回是否成功
        bool revert_global(const char* path, const char* tmpname = nullptr);

        // 将表中的 XX.YY.zz 字段替换为 val，并添加一个版本的缓存，返回旧值 (失败返回 nil)
        LuaValue hook_field(int table_idx, const char* path, const LuaValue& newval);
        // 将全局表中的 XX.YY.zz 字段替换为 val，并添加一个版本的缓存，返回旧值 (失败返回 nil)
        LuaValue hook_global(const char* path, const LuaValue& newval);

        // 还原表中的 XX.YY.zz 字段 revert 个版本，返回是否成功
        // 若 revert 超过缓存版本数则还原为 最旧的版本，若无缓存版本则返回 false
        bool unhook_field(int table_idx, const char* path, int revert = 1);
        // 还原表中的 XX.YY.zz 字段 revert 个版本，返回是否成功
        // 若 revert 超过缓存版本数则还原为 最旧的版本，若无缓存版本则返回 false
        bool unhook_global(const char* path, int revert = 1);


        // ==================== 6. 函数操作 ====================

        // 调用栈顶函数，nargs 参数个数，nresults 期望返回个数
        void call(int nargs, int nresults) { lua_load().lua_call(this, nargs, nresults); }
        // 保护调用，返回是否成功；errfunc 为错误处理函数栈索引，若不提供在失败时会改为输出错误信息
        bool pcall(int nargs, int nresults, int errfunc = 0);

        // 获取函数闭包的第 n 个上值的索引（闭包内部调用）
        int get_param(int arg_idx);

        // 获取函数闭包的第 n 个上值，若存在则压栈并返回上值名称，若不存在返回 nullptr
        [[deprecated("对于有效参数会导致原因不明的崩溃，可能发生在封装层的转换中")]]
        Il2CppString* push_upvalue(int func_idx, int n) { return lua_load().lua_getupvalue(this, func_idx, n); }
        // 弹出栈顶值，若第 n 个上至存在则更新闭包的上值并返回上值名称，若不存在返回 nullptr
        Il2CppString* pop_upvalue(int func_idx, int n);

        // 抛出一个 Lua 错误
        int error(Il2CppString* msg) { return lua_load().luaL_throw(this, msg); }
        // 参数类型错误，抛出错误信息
        int arg_error(int narg, Il2CppString* extramsg) { return lua_load().luaL_argerror(this, narg, extramsg); }
        // 类型错误
        int type_error(int idx, Il2CppString* tname) { return lua_load().luaL_typerror(this, idx, tname); }

        // 获取调用栈信息
        int get_stack(int level, void* ar) { return lua_load().lua_getstack(this, level, ar); }
        // 获取调试信息
        int get_info(Il2CppString* what, void* ar) { return lua_load().lua_getinfo(this, what, ar); }
        // 获取局部变量名
        Il2CppString* get_local(void* ar, int n) { return lua_load().lua_getlocal(this, ar, n); }
        // 设置局部变量值
        Il2CppString* set_local(void* ar, int n) { return lua_load().lua_setlocal(this, ar, n); }
        // 设置钩子函数
        int set_hook(void* func, int mask, int count) { return lua_load().lua_sethook(this, func, mask, count); }


        // ==================== 7. 对象操作 ====================

        // 创建新表并压栈，预分配数组和哈希部分的槽位数
        void create_table(int array_slots = 0, int hash_slots = 0) { lua_load().lua_createtable(this, array_slots, hash_slots); }
        // 创建并压入一个完整 userdata，返回分配的内存指针
        void* create_data(size_t size) { return lua_load().lua_newuserdata(this, size); }

        // 获取表或字符串的长度
        int get_length(int idx) { return lua_load().lua_objlen(this, idx); }

        // 比较两个索引的值是否相等（触发 __eq）
        bool equal(int idx1, int idx2) { return lua_load().lua_equal(this, idx1, idx2) != 0; }
        // 比较 idx1 的值是否小于 idx2（触发 __lt）
        bool less(int idx1, int idx2) { return lua_load().lua_lessthan(this, idx1, idx2) != 0; }

        // 原始相等比较
        bool equal(int idx1, int idx2, native_t) { return lua_load().lua_rawequal(this, idx1, idx2) != 0; }

        // 获取对象元表中的指定字段，若成功则压栈，返回是否压栈
        bool load_meta(int idx, Il2CppString* field) { return lua_load().luaL_getmetafield(this, idx, field) != LuaValue::Nil; }
        // 获取对象元表中的指定字段，若成功则压栈，返回是否压栈
        bool load_meta(int idx, const char* field);

        // 若元方法存在则调用，返回是否存在
        bool call_meta(int idx, Il2CppString* field) { return lua_load().luaL_callmeta(this, idx, field); }
        // 若元方法存在则调用，返回是否存在
        bool call_meta(int idx, const char* field);


        // ==================== 8. 引用操作  ====================

        // 将栈顶元素注册到表，返回引用 ID
        int pop_ref(int table_idx) { return lua_load().luaL_ref(this, table_idx); }
        // 释放表索引中的引用
        void put_ref(int table_idx, int ref_id) { lua_load().luaL_unref(this, table_idx, ref_id); }

        // 获取指定索引元素的引用，返回引用 ID
        int create_ref(int idx);
        // 释放注册表中的引用
        void destroy_ref(int ref_id);


        // ==================== 9. 脚本操作 ====================

        // 编译并执行字符串中的 Lua 代码，若成功则压栈，返回是否成功
        [[deprecated("会导致原因不明的崩溃")]]
        bool exec(Il2CppString* code);
        // 编译并执行字符串中的 Lua 代码，若成功则压栈，返回是否成功
        [[deprecated("会导致原因不明的崩溃")]]
        bool exec(const char* code);

        // 加载并执行一个 Lua 文件，若成功则压栈，返回是否成功
        [[deprecated("会导致原因不明的崩溃")]]
        bool exec_file(Il2CppString* path);
        // 加载并执行一个 Lua 文件，若成功则压栈，返回是否成功
        [[deprecated("会导致原因不明的崩溃")]]
        bool exec_file(const char* path);

        // 编译代码字符串为 Lua 函数，若成功则压栈，返回是否成功
        [[deprecated("会导致原因不明的崩溃")]]
        bool push_script(Il2CppString* code);
        // 编译代码字符串为 Lua 函数，若成功则压栈，返回是否成功
        [[deprecated("会导致原因不明的崩溃")]]
        bool push_script(const char* code);

        // 加载并编译一个 Lua 文件，若成功则压栈，返回是否成功
        [[deprecated("会导致原因不明的崩溃")]]
        bool push_script_from(Il2CppString* path);
        // 加载并编译一个 Lua 文件，若成功则压栈，返回是否成功
        [[deprecated("会导致原因不明的崩溃")]]
        bool push_script_from(const char* path);


        // ==================== 10. GC 操作 ====================

        // 垃圾回收控制，what 为 GC 操作类型，data 为参数
        int gc_control(int what, int data) { return lua_load().lua_gc(this, what, data); }
        // 暂停垃圾回收控制
        void gc_stop() { gc_control(0, {}); }
        // 重启垃圾回收控制
        void gc_restart() { gc_control(1, {}); }
        // 强制执行一次垃圾回收（单位: KB）
        void gc_collect() { gc_control(2, {}); }
        // 获取内存占用
        int gc_memory() { return gc_control(3, {}); }
    };

}
