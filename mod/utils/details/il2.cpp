#include "../il2.h"

#include <dlfcn.h>
#include <codecvt>
#include <locale>

#include "../log.h"
#include "../obfstr.h"

namespace xyu
{
    Il2Cpp::MethodPointer Il2Cpp::get_lua_method(const char* method_name)
    {
        static struct Temp
        {
            using get_method_t = const MethodInfo*(*)(Class* klass, const char* method_name, int args_count);

            Class* lua_class;
            get_method_t get_method;

            Temp()
            {
                auto lib = dlopen(SL_IL2CPP.get(), 4);

                // 获取 IL2CPP 域
                using get_domain_t = Domain*(*)();
                auto get_domain = get_domain_t(dlsym(lib, xyobfstr("il2cpp_domain_get")));
                if (!get_domain) { LOGW("  >> %s", xyobfcstr("符号未找到: il2cpp_domain_get")); return; }
                Domain* domain = get_domain();
                if (!domain) { LOGW("  >> %s", xyobfcstr("域获取失败: il2cpp_domain_get")); return; }

                // 打开程序集
                using open_assembly_t = Assembly*(*)(Domain*, const char*);
                auto open_assembly = open_assembly_t(dlsym(lib, xyobfstr("il2cpp_domain_assembly_open")));
                if (!open_assembly) { LOGW("  >> %s", xyobfcstr("符号未找到: il2cpp_domain_assembly_open")); return; }
                Assembly* assembly = open_assembly(domain, xyobfstr("azurlane.tolua.main"));
                if (!assembly) { LOGW("  >> %s", xyobfcstr("程序集打开失败: azurlane.tolua.main")); return; }

                // 获取程序集镜像
                using get_image_t = Image*(*)(const Assembly*);
                auto get_image = get_image_t(dlsym(lib, xyobfstr("il2cpp_assembly_get_image")));
                if (!get_image) { LOGW("  >> %s", xyobfcstr("符号未找到: il2cpp_assembly_get_image")); return; }
                Image* image = get_image(assembly);
                if (!image) { LOGW("  >> %s", xyobfcstr("程序集镜像获取失败")); return; }

                // 获取类
                using get_class_t = Class*(*)(const Image*, const char*, const char*);
                auto get_class = get_class_t(dlsym(lib, xyobfstr("il2cpp_class_from_name")));
                if (!get_class) { LOGW("  >> %s", xyobfcstr("符号未找到: il2cpp_class_from_name")); return; }
                lua_class = get_class(image, xyobfstr("LuaInterface"), xyobfstr("LuaDLL"));
                if (!lua_class) { LOGW("  >> %s", xyobfcstr("类未找到: LuaInterface::LuaDLL")); return; }

                // 获取方法查找函数
                get_method = get_method_t(dlsym(lib, xyobfstr("il2cpp_class_get_method_from_name")));
                if (!get_method) { LOGW("  >> %s", xyobfcstr("符号未找到: il2cpp_class_get_method_from_name")); return; }
            }
        } tool;

        auto info = tool.get_method(tool.lua_class, method_name, -1);
        if (!info)
        {
            LOGW("  >> %s::%s", xyobfcstr("方法未找到: LuaDLL"), method_name);
            return nullptr;
        }
        return info->method_pointer;
    }

    Il2CppString* Il2CppString::from(const char* str)
    {
        using make_string_t = Il2CppString*(*)(const char*);
        static make_string_t maker = []
        {
            auto lib = dlopen(SL_IL2CPP.get(), 4);
            auto smaker = make_string_t(dlsym(lib, xyobfstr("il2cpp_string_new")));
            if (!smaker) LOGW("  >> %s", xyobfcstr("符号未找到: il2cpp_string_new"));
            return smaker;
        }();
        auto p = maker(str);
        if (!p) LOGW("  >> %s", xyobfcstr("创建字符串失败"));
        return p;
    }

    std::string Il2CppString::to_utf8()
    {
        std::u16string u16_str((const char16_t*)chars, length);

        try {
            std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
            return convert.to_bytes(u16_str);
        } catch (const std::exception& e) {
            LOGW("  >> %s", xyobfcstr("转换字符串失败"));
            return "";
        }
    }

}
