cmake_minimum_required(VERSION 3.22)

set(CMAKE_SYSTEM_VERSION 21)
set(ANDROID_PLATFORM 21)

# 项目名称
project(xyazl)

set(CMAKE_CXX_STANDARD 20)

# 通用编译选项：隐藏符号、启用 LTO、隐藏本地路径
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -flto -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=.")
# Release 性能优化
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffunction-sections -fdata-sections")
# Release 共享库链接器选项：GC sections、排除静态库符号、剥离符号表
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--gc-sections -Wl,--exclude-libs,ALL -Wl,-s")
# 导出符号
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/build/export.map")

if(ANDROID_ABI STREQUAL "arm64-v8a")
    # ARM64 默认支持 NEON，但可以明确开启加速
    add_definitions(-D__ARM_NEON)
    set(ARCH_FLAGS "-march=armv8-a")
elseif(ANDROID_ABI STREQUAL "x86_64")
    # x86_64 开启 SSE4.1
    set(ARCH_FLAGS "-msse4.1 -mavx2")
elseif(ANDROID_ABI STREQUAL "x86")
    # x86 开启 SSE2
    set(ARCH_FLAGS "-msse2")
endif()

# 安卓系统库
find_library(log-lib log)

# 声明动态库
add_library(xyazler SHARED maker/stub.cpp)
# 链接静态库
target_link_libraries(xyazler PRIVATE ${log-lib})

# 声明动态库
add_library(xyazl SHARED
        mod/main.cpp
        mod/details/hack.cpp
        mod/details/menu.cpp
        mod/details/switch.cpp
        mod/details/switch_order.cpp
        mod/details/local.cpp
        mod/details/event_skin.cpp
        mod/details/event_values.cpp
        mod/details/event_logic.cpp
        mod/details/event_animation.cpp
        mod/details/event_unlimit.cpp
        mod/details/event_world.cpp
        mod/details/event_gameroom.cpp
        mod/details/event_other.cpp
        utils/details/lua.cpp
        utils/details/il2.cpp
        utils/details/hashtable.cpp
)
# dobby 路径
add_library(dobby-lib STATIC IMPORTED)
set_target_properties(dobby-lib PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/${ANDROID_ABI}/libdobby.a")
# 链接静态库
target_link_libraries(xyazl ${log-lib} dobby-lib)
