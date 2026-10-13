#pragma once

#include <string>

#include "./type.h"

namespace xyu
{
    /**
     * @brief Il2Cpp 核心结构体
     * @details Unity 2022.3 (IL2CPP v29.x)。
     * 用于动态调用 IL2CPP 内部函数。
     * 包含域、程序集、镜像、方法信息及常用导出函数指针。
     */
    struct Il2Cpp
    {
        /// IL2CPP 域（AppDomain）
        struct Domain
        {
            void*        domain;            // 域对象指针
            void*        setup;             // 域设置信息
            void*        default_context;   // 默认上下文
            const char*  friendly_name;     // 友好名称
            uint32       domain_id;         // 域 ID
        };

        /// IL2CPP 程序集名称
        struct AssemblyName
        {
            const char*   name;
            const char*   culture;
            const uint8*  public_key;
            uint32        hash_alg;
            int32         hash_len;
            uint32        flags;
            int32         major;
            int32         minor;
            int32         build;
            int32         revision;
            uint8         public_key_token[8];
        };

        /// IL2CPP 程序集
        struct Assembly
        {
            struct Image* image;                    // 程序集所属镜像
            uint32        token;
            int32         referencedAssembly_start;
            int32         referencedAssembly_count;
            AssemblyName  name;
        };

        /// IL2CPP 镜像
        struct Image
        {
            const char* name;                   // 镜像名称（含扩展名）
            const char* nameNoExt;              // 镜像名称（不含扩展名）
            Assembly*   assembly;               // 所属程序集
            uint32      typeCount;              // 类型数量
            uint32      exportedType_count;     // 导出类型数量
            uint32      customAttribute_count;  // 自定义特性数量
            void*       metadata_handle;        // 元数据句柄（Il2CppMetadataImageHandle）
            void*       name_to_classHashTable; // 名称到类型哈希表
            const void* codeGen_module;         // 代码生成模块（Il2CppCodeGenModule）
            uint32      token;                  // 元数据标记
            uint8       dynamic;                // 是否动态生成
        };

        /// IL2Cpp 类型信息
        struct Type
        {
            void*        data;
            unsigned int bits;
        };

        /// IL2CPP 方法指针
        using MethodPointer = void(*)();
        /// IL2CPP 方法调用
        using InvokerMethod = void(*)(MethodPointer, const struct MethodInfo*, void*, void**, void*);

        /// IL2CPP 方法信息
        struct MethodInfo
        {
            MethodPointer  method_pointer;          // 方法指针（非虚调用）
            MethodPointer  virtualMethod_pointer;   // 虚方法指针
            InvokerMethod  invoker_method;          // 调用器方法
            const char*    name;                    // 方法名称
            struct Class*  klass;                   // 所属类
            const Type*    return_type;             // 返回值类型
            const Type**   parameters;              // 参数类型数组
            union
            {
                const void* rgctx_data;             // 运行时泛型上下文数据
                const void* methodMetadata_handle;  // 方法元数据句柄
            };
            union
            {
                const void* generic_method;         // 泛型方法信息
                const void* genericContainer_handle;// 泛型容器句柄
            };
            uint32  token;              // 方法元数据标记
            uint16  flags;              // 方法标志
            uint16  iflags;             // 实现标志
            uint16  slot;               // 虚表槽位
            uint8   parameters_count;   // 参数个数
            uint8   bitflags;
        };

        /**
         * @brief 从动态库中获取 lua 函数
         * @attention 调用前需要附加线程
         */
        static MethodPointer get_lua_method(const char* method_name);

        /**
         * @brief 创建 IL2CPP 字符串
         * @param str 原始字符串
         */
        static struct Il2CppString* make_string(const char* str);
    };

    /// IL2CPP 字符串
    struct Il2CppString
    {
        /// IL2CPP 对象
        struct Object
        {
            Class*  klass;
            void*   monitor;
        }object;
        int32   length;         // 字符数（不含 null 结尾）
        uint16  chars[32];      // 变长 UTF-16（32 表示占位）

        /**
         * @brief 创建 IL2CPP 字符串
         * @param str utf8 字符串
         */
        static Il2CppString* from(const char* str);

        /**
         * @brief 转换为 utf8 字符串
         * @details 原始为 utf-16
         */
        std::string to_utf8();
    };

}
