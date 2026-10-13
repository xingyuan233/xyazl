#pragma once

/// 固定长度类型
// (代替 int,long等 不确定长度的整型使用)
namespace xyu
{
    // 默认无符号整型
    using uint = unsigned int;
    // 8位整型
    using int8 = signed char;
    // 无符号8位整型
    using uint8 = unsigned char;
    // 16位整型
    using int16 = __INT16_TYPE__;
    // 无符号16位整型
    using uint16 = __UINT16_TYPE__;
    // 32位整型
    using int32 = __INT32_TYPE__;
    // 无符号32位整型
    using uint32 = __UINT32_TYPE__;
    // 64位整型
    using int64 = __INT64_TYPE__;
    // 无符号64位整型
    using uint64 = __UINT64_TYPE__;
#ifdef __SIZEOF_INT128__
    // 128位整型
    using int128 = __int128_t;
    // 无符号128位整型
    using uint128 = __uint128_t;
#endif
}

/// 特殊类型
namespace xyu
{
    // 大小类型 (无符号整型)
    using size_t = __SIZE_TYPE__;
    // 偏移类型 (有符号整型)
    using diff_t = __PTRDIFF_TYPE__;

    // 空指针类型
    using nullptr_t = decltype(nullptr);
    // 原类型
    constexpr struct native_t  {} native_v;
    // 特殊类型
    constexpr struct special_t {} special_v;

    // 成功类型
    struct success_t {};
    // 失败类型
    struct fail_t {};
}

/// 常用类型
namespace xyu
{
    // 字符串视图
    struct StringView
    {
        size_t size;
        char* data;

        StringView(char* data) noexcept : data(data), size(__builtin_strlen(data)) {}
        StringView(char* data, size_t size) noexcept : data(data), size(size) {}
    };
}
