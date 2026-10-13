#pragma once

#include "./obfstr.h"

namespace xyu
{
    /// 路径 配置
    struct PathConfig
    {
        // 内部存储路径
        inline static auto PRIVATE = xyobflstr("/data/data/com.bilibili.blhx.uc/files/");
        // 外部存储路径
        inline static auto PUBLIC = xyobflstr("/storage/emulated/0/Android/data/com.bilibili.blhx.uc/files/");
        // 下载路径
#if defined(__aarch64__)
        inline static auto DOWNLOAD = xyobflstr("/sdcard/Download/");
#else
        inline static auto DOWNLOAD = xyobflstr("/sdcard/Pictures/");
#endif
    };

}
