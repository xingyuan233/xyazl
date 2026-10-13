#pragma once

#include "./type.h"

/// 类型定义
namespace xyu
{
    /**
     * @brief 混淆字符串
     * @tparam N 长度
     * @details 仅存储混淆后的字符串
     */
    template <size_t N>
    class ObfString
    {
        static constexpr size_t NP =  7 - (N + 7) % 8;
    private:
        uint64 key;     // 密钥
        char data[N];   // 混淆字符串
        char pod[NP];   // 填充字符串 (防止'\0'分割)
    public:
        /// 构造函数
        constexpr ObfString(uint64 key, const char(&src)[N + 1]) noexcept : key(key)
        {
            for (size_t i = 0; i < N; i++)
                data[i] = src[i] ^ (key >> (i % 8 * 8));
            for (size_t i = 0; i < NP; i++)
                pod[i] = src[i % N] ^ (key >> ((7 - i % 8) * 8));
        }

        /// 还原字符串 (一次性上下文)
        auto get() const noexcept
        {
            struct SString
            {
            private:
                using ArrRef = char(&)[N + 1];
                using ArrCRef = const char(&)[N + 1];
                char dst[N + 1];
            public:
                /// 构造函数
                SString(const char* data, uint64 key) noexcept
                {
                    for (size_t i = 0; i < N; i++)
                        dst[i] = data[i] ^ (key >> (i % 8 * 8));
                    dst[N] = '\0';
                }
                /// 析构函数
                ~SString() noexcept
                {
                    __builtin_memset(dst, 0, N);
                    asm volatile("" : : "r"(dst) : "memory");
                }
                /// 隐式转换
                operator ArrRef() noexcept { return dst; }
                operator ArrCRef() const noexcept { return dst; }
                /// 显示获取
                ArrRef data() noexcept { return dst; }
                ArrCRef data() const noexcept { return dst; }
            };
            return SString(data, opaque_value(key));
        }

    private:
        /// 阻止编译器对值进行常量传播
        template <typename T>
        [[gnu::noinline]]
        static T opaque_value(T val) noexcept
        {
            asm volatile("" : "+r"(val));
            return val;
        }
    };

    /**
     * @brief 获取混淆密钥
     * @param key 原始密钥 
     * @param file 文件名
     * @return 混淆后的密钥，每个字节均不为 0
     */
    template <size_t N>
    constexpr uint64 make_obfkey(uint64 key, const char(&file)[N]) noexcept
    {
        key *= static_cast<uint64>(file[key % N]);
        key ^= (key >> 33);
        key *= 0xff51afd7ed558ccd;
        key ^= (key >> 33);
        key *= 0xc4ceb9fe1a85ec53;
        key ^= (key >> 33);
        key |= 0x8040201008040201;
        return key;
    }

}

/// 调用工具
namespace xyu
{
    /**
     * @brief 存储混淆字符串
     * @param str 原始字符串
     * @return 返回混淆字符串
     * @details
     *   通过 `inline auto S_XXXX = xyobflstr("xxxxx")` 存储需要复用的混淆字符串。
     *   通过 `S_XXXX.get()` 获取一次性使用的字符串。
     */
#define xyobflstr( str ) \
    [] { \
        using namespace xyu; \
        constexpr auto __line = __LINE__; \
        constexpr char __file[sizeof(__FILE__) + 1] = __FILE__; \
        constexpr char __src[sizeof(str) + 1] = str; \
        constexpr uint64 __key = make_obfkey(__line, __file); \
        constexpr ObfString<sizeof(str)> obf(__key, __src); \
        static const __attribute__((section(".obfdata"))) \
        ObfString store{obf}; \
        return store; \
    }()

    /**
     * @brief 一次性字符串
     * @param str 原始字符串
     * @return 返回一次性使用的原始字符串
     * @details 通过 `xyobfstr("xxxxx")` 获取一次性使用的字符串。
     */
#define xyobfstr( str ) xyobflstr( str ).get()

    /**
     * @brief 一次性字符串
     * @param str 原始字符串
     * @return 返回一次性使用的原始字符串
     * @details 通过 `xyobfcstr("xxxxx")` 获取一次性使用的字符串。
     */
#define xyobfcstr( str ) xyobfstr( str ).data()
}

/// 复用常量
namespace xyu
{
    // mod
    inline auto S_XYAZL = xyobflstr("XYAZL");
    // so
    inline auto S_SO = xyobflstr(".so");

    // abi 检查阶段
    inline auto SS_ABI = xyobflstr("ABI-Detect");
    // mod 更新阶段
    inline auto SS_UPDATE = xyobflstr("Update");
    // setup 阶段
    inline auto SS_SETUP = xyobflstr("Setup");
    // IL2CPP 加载阶段
    inline auto SS_IL2 = xyobflstr("IL2");
    // Menu 阶段
    inline auto SS_MENU = xyobflstr("Menu");
    // Input 加载阶段
    inline auto SS_INPUT = xyobflstr("Input");
    // UI 加载阶段
    inline auto SS_UI = xyobflstr("UI");
    // lua 获取阶段
    inline auto SS_LUA = xyobflstr("Lua");
    // main 核心阶段
    inline auto SS_MAIN = xyobflstr("Main");
    // setting 阶段
    inline auto SS_SET = xyobflstr("Setting");
    // event 阶段
    inline auto SS_EVENT = xyobflstr("Event");

    // unity 动态库 名称
    inline auto SL_UNITY = xyobflstr("libunity.so");
    // il2cpp 动态库 名称
    inline auto SL_IL2CPP = xyobflstr("libil2cpp.so");
    // tolua 动态库 名称
    inline auto SL_TOLUA = xyobflstr("libtolua.so");
}
