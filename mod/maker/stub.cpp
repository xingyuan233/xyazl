#include <dlfcn.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <cstdio>
#include <sys/system_properties.h>
#include <sys/stat.h>

#include "../utils/config.h"
#include "../utils/obfstr.h"
#include "../utils/log.h"

// 递归创建目录
bool mkdir_recursive(const std::string& path)
{
    size_t pos = 0;
    std::string dir;
    while ((pos = path.find('/', pos + 1)) != std::string::npos) {
        dir = path.substr(0, pos);
        if (mkdir(dir.c_str(), 0755) && errno != EEXIST) return false;
    }
    if (mkdir(path.c_str(), 0755) && errno != EEXIST) return false;
    return true;
}

// 纯粹的二进制文件复制
bool copy_file(const std::string& src, const std::string& dst)
{
    std::ifstream in(src, std::ios::binary);
    if (!in) return false;
    std::ofstream out(dst, std::ios::binary);
    if (!out) return false;
    out << in.rdbuf();
    return true;
}

// hook
extern "C" void __attribute__((visibility("default"))) __attribute__((constructor)) stub_main()
{
    std::string update_dir{xyu::PathConfig::PUBLIC.get()}; update_dir += "mod/";
    std::string inner_dir{xyu::PathConfig::PRIVATE.get()}; inner_dir += "mod/";
    std::string abis[] = {"arm64", "x86_64", "x86"};

    // ---------- 1. 检测当前 ABI ----------
#if defined(__aarch64__)
    const char* current_abi = "arm64";
#elif defined(__x86_64__)
    const char* current_abi = "x86_64";
#elif defined(__i386__)
    const char* current_abi = "x86";
#else
    char current_abi[PROP_VALUE_MAX];
    __system_property_get("ro.product.cpu.abi", current_abi);
    LOGW("[ABI-Detect] 不支持的设备架构: %s", current_abi);
    return;
#endif
    LOGI("[%s] %s: %s", xyu::SS_ABI.get().data(), xyobfcstr("游戏启动，当前运行架构"), current_abi);

    // ---------- 2. 检查更新 ----------
    for (auto& abi : abis)
    {
        std::string update_path = update_dir + abi + xyu::S_SO.get().data();
        std::string inner_path  = inner_dir  + abi + xyu::S_SO.get().data();

        if (access(update_path.c_str(), F_OK) == 0)
        {
            LOGI("[%s] %s: %s", xyu::SS_UPDATE.get().data(), xyobfcstr("发现更新文件"),
                                (abi + xyu::S_SO.get().data()).c_str());

            if (!mkdir_recursive(inner_dir))
            {
                LOGW("[%s] %s", xyu::SS_UPDATE.get().data(), xyobfcstr("创建内部目录失败"));
                break;
            }

            if (copy_file(update_path, inner_path))
            {
                chmod(inner_path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR);
                LOGI("[%s] %s", xyu::SS_UPDATE.get().data(), xyobfcstr("文件更新成功"));
            }
            else LOGW("[%s] %s", xyu::SS_UPDATE.get().data(), xyobfcstr("拷贝文件失败"));

            remove(update_path.c_str());
        }
    }

    // ---------- 3. 加载 mod ----------
    std::string mod_path = inner_dir + current_abi + ".so";
    if (access(mod_path.c_str(), F_OK) == 0)
    {
        void* handle = dlopen(mod_path.c_str(), RTLD_NOW);
        if (!handle) LOGW("[%s] %s: %s", xyu::SS_SETUP.get().data(), xyobfcstr("mod加载失败"),  dlerror());
    }
    else LOGI("[%s] %s", xyu::SS_SETUP.get().data(), xyobfcstr("mod未安装"));
}
