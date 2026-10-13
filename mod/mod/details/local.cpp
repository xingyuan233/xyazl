#include "../local.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <sys/stat.h>

#include "../event.h"
#include "../switch.h"
#include "../../utils/log.h"
#include "../../utils/config.h"
#include "../../utils/mpscqueue.h"
#include "../../utils/obfstr.h"

using namespace xyu;

/* 辅助工具 */

// 辅助 - 去除首位空格
static void trim(std::string& s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// 辅助 - 读取 ini 文件
template <typename Callback>
static void read_ini(const std::string& path, Callback&& callback)
{
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line))
    {
        // 移除首尾空白
        trim(line);

        // 跳过空行和注释行（以 # 开头）
        if (line.empty() || line[0] == '#') continue;

        // 查找等号分隔符
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue; // 格式错误，跳过

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        trim(key);
        trim(value);

        callback(std::move(key), std::move(value));
    }
}

// 辅助 - 压入 string
template <typename Queue64>
static void push_string(Queue64& infos, const std::string& str)
{
    // 作为 uint64 压入
    int i;
    uint64 data;
    for (i = 0; i + 8 <= str.size(); i += 8)
    {
        __builtin_memcpy(&data, str.data() + i, 8);
        infos.push(data);
    }
    // 剩余部分
    if (str.size() & 7)
    {
        data = 0;
        __builtin_memcpy(&data, str.data() + i, str.size() - i);
        infos.push(data);
    }
}
// 辅助 - 读取 string
template <typename Queue64>
static void pop_string(Queue64& infos, std::string& str, uint size)
{
    bool empty;
    str.clear();
    for (int i=0; i < size; i += 8)
    {
        char buf[9]{0,0,0,0,0,0,0,0,0};
        uint64 tmp = infos.pop(&empty);
        __builtin_memcpy(buf, &tmp, 8);
        str += buf;
    }
}


/* 异步写入线程 */

// 状态枚举
enum StateType : int
{
    FREE = 0,
    WORK = 1,
    OVER = -1,
};
static std::atomic<int> state = FREE;
static std::atomic<int> sync_point = FREE;

// 事件枚举
enum InfoType : uint64
{
    SAVE_SWITCHES = 1 << 0,     // 保存mod设置
    UPDATE_GID = 1 << 1,        // 更改账号和服务器
    LOAD_SHIP_SKIN = 1 << 2,    // 读取舰船皮肤数据
    UPDATE_SHIP_SKIN = 1 << 3,  // 更改舰船皮肤数据
    LOAD_ATTIRE = 1 << 6,       // 读取装扮数据
    UPDATE_ATTIRE = 1 << 5,     // 更改装扮数据

    // 高32位作为额外信息
};
static MPSCQueue<uint64> infos;

// 工作函数
static void work_handle();
// 工作线程
static struct ThreadCtrl
{
    bool init = false;
    std::thread handle;

    void start()
    {
        if (init) return;
        handle = std::thread(work_handle);
        init = true;
    }

    void end()
    {
        if (!init) return;
        state.store(OVER, std::memory_order_release);
        state.notify_one();
        handle.join();
        init = false;
    }

    void work()
    {
        start();
        state.store(WORK, std::memory_order_release);
        state.notify_one();
    }

    ~ThreadCtrl() noexcept { end(); }

} worker;


/* mod配置 */

// 获取开关配置路径
static std::string get_switch_ini()
{
    return std::string{PathConfig::PUBLIC.get()} + xyobfcstr("/mod/switch.ini");
}
// mod 是否加载完毕
static bool is_loaded = false;
bool xyu::is_mod_loaded() { return is_loaded; }
// 读取mod配置
void xyu::read_mod(LuaState* L)
{
    switch_group.init();
    bool flush = false;
    read_ini(get_switch_ini(), [L, &flush](std::string&& name, std::string&& value)
    {
        auto* sw = switch_group.get(name.c_str());
        if (!sw) { flush = true; return; }
        if (value != "true") return;
        if (sw->on == true) return;
        sw->on = true;
        sw->callback(L, true);
    });
    is_loaded = true;
    if (flush) write_mod();
}
// 写入mod配置
void xyu::write_mod()
{
    infos.push(SAVE_SWITCHES);
    worker.work();
}

// 写入mod函数
static void save_mod()
{
    static struct FolderMaker
    {
        FolderMaker()
        {
            std::filesystem::path file_path(get_switch_ini());
            auto parent_dir = file_path.parent_path();
            if (!parent_dir.empty())
            {
                std::error_code ec;
                std::filesystem::create_directories(parent_dir, ec);
            }
        }
    } folder_maker;

    std::ofstream file(get_switch_ini(), std::ios::out | std::ios::trunc);
    if (!file.is_open()) return;

    for (const auto& sw : switch_group.get())
        file << sw.name << " = " << (sw.on ? "true" : "false") << "\n";
}

/* 舰船皮肤 */

// 舰船皮肤数据
static HashTable<uint,uint> ship_skins;
// 读取舰船皮肤数据
HashTable<uint,uint>& xyu::get_ship_skins() { return ship_skins; }
// 更改舰船皮肤数据
void xyu::update_ship_skin(uint ship_id, uint skin_id)
{
    infos.push(UPDATE_SHIP_SKIN | ((uint64)skin_id << 32));
    infos.push(ship_id);
    worker.work();
}

// 获取皮肤配置路径
static std::string get_skin_ini(const std::string& server, const std::string& uid)
{
    std::string path = PathConfig::PUBLIC.get().data();
    path += xyobfstr("mod/");
    path += server; path += '/';
    path += uid; path += '/';
    path += xyobfstr("ship_skins.ini");
    return path;
}
// 读取皮肤函数
static void load_ship_skins(const std::string& server, const std::string& uid)
{
    ship_skins.clear();
    read_ini(get_skin_ini(server, uid), [](std::string&& ship_sid, std::string&& skin_sid)
    {
        uint ship_id = std::atoi(ship_sid.c_str());
        uint skin_id = std::atoi(skin_sid.c_str());
        if (!ship_id || !skin_id) return;
        ship_skins.get(ship_id) = skin_id;
    });
}
// 保存皮肤函数
static void save_ship_skins(const std::string& server, const std::string& uid)
{
    std::ofstream file(get_skin_ini(server, uid), std::ios::out | std::ios::trunc);
    if (!file.is_open()) return;

    for (const auto& kv : ship_skins)
        if (kv.key && kv.val) file << kv.key << " = " << kv.val << "\n";
}

/* 装扮 */

// 装扮数据
static AttireInfo attire_info;
// 读取装扮数据
AttireInfo& xyu::get_attire() { return attire_info; }
// 更改装扮数据
void xyu::update_attire(uint type, uint id)
{
    infos.push(UPDATE_ATTIRE | ((uint64)type << 32));
    infos.push(id);
    worker.work();
}

// 获取装扮配置路径
static std::string get_attire_ini(const std::string& server, const std::string& uid)
{
    std::string path = PathConfig::PUBLIC.get().data();
    path += xyobfstr("mod/");
    path += server; path += '/';
    path += uid; path += '/';
    path += xyobfstr("attrie.ini");
    return path;
}
// 读取装扮数据
static void load_attire(const std::string& server, const std::string& uid)
{
    read_ini(get_attire_ini(server, uid), [](std::string&& name, std::string&& skin_sid)
    {
        uint skin_id = std::atoi(skin_sid.c_str());
        if (name.empty() || !skin_id);
        else if (name == xyobfcstr("头像框")) attire_info.avatar = skin_id;
        else if (name == xyobfcstr("聊天气泡")) attire_info.chatbg = skin_id;
        else if (name == xyobfcstr("战斗界面")) attire_info.combatui = skin_id;
    });
}
// 保存装扮数据
static void save_attire(const std::string& server, const std::string& uid)
{
    std::ofstream file(get_attire_ini(server, uid), std::ios::out | std::ios::trunc);
    if (!file.is_open()) return;

    file << xyobfstr("头像框") << " = " << attire_info.avatar << '\n';
    file << xyobfstr("聊天气泡") << " = " << attire_info.chatbg << '\n';
    file << xyobfstr("战斗界面") << " = " << attire_info.combatui << '\n';
}

/* 账号登录 | 切换 */

// 切换次数
static int change_times = 0;
// 账号 | 服务器 切换
void xyu::update_gid(std::string server, uint64 uid)
{
    std::filesystem::path file_path(get_skin_ini(server, std::to_string(uid)));
    auto parent_dir = file_path.parent_path();
    if (!parent_dir.empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(parent_dir, ec);
    }

    infos.push(UPDATE_GID | ((uint64)server.size() << 32));
    infos.push(uid);
    push_string(infos, server);

    infos.push(LOAD_ATTIRE);
    infos.push(LOAD_SHIP_SKIN);
    worker.work();
    sync_point.wait(FREE, std::memory_order_acquire);
    sync_point.store(FREE, std::memory_order_release);
    ++change_times;
}
// 获取切换账号或服务器次数
int xyu::get_change_gid_times() { return change_times; }


/* 异步写入函数 */

// 工作函数
void work_handle()
{
    std::string server, uid;
    bool need_save_mod, need_save_skin, need_save_attire;
    for (;;)
    {
        int sta = state.exchange(FREE, std::memory_order_acq_rel);

        // 无事件，挂起等待
        if (sta == FREE)
        {
            state.wait(FREE, std::memory_order_acquire);
            continue;
        }

        // 处理事件
        need_save_mod = need_save_skin = need_save_attire = false;
        for (;;)
        {
            bool empty;
            uint64 info = infos.pop(&empty);
            if (empty) break;

            uint32 type = info & 0xffffffffu;
            uint32 data = info >> 32;

            // 保存mod配置
            if (type == SAVE_SWITCHES) need_save_mod = true;
            // 更改账号信息
            else if (type == UPDATE_GID)
            {
                uid = std::to_string(infos.pop(&empty));
                pop_string(infos, server, data);
            }
            // 读取皮肤数据
            else if (type == LOAD_SHIP_SKIN)
            {
                load_ship_skins(server, uid);
                sync_point.store(OVER, std::memory_order_release);
                sync_point.notify_one();
            }
            // 更改皮肤数据
            else if (type == UPDATE_SHIP_SKIN)
            {
                uint64 skip_id = infos.pop(&empty);
                if (skip_id) ship_skins.get(skip_id) = data;
                need_save_skin = true;
            }
            // 读取装扮数据
            else if (type == LOAD_ATTIRE)
            {
                load_attire(server, uid);
            }
            // 更改装扮数据
            else if (type == UPDATE_ATTIRE)
            {
                uint id = infos.pop(&empty);
                if (data == 2) attire_info.avatar = id;
                else if (data == 3) attire_info.chatbg = id;
                else if (data == 4) attire_info.combatui = id;
                else continue;
                need_save_attire = true;
            }
        }

        // 写入文件
        if (need_save_mod) save_mod();
        if (need_save_skin) save_ship_skins(server, uid);
        if (need_save_attire) save_attire(server, uid);

        // 结束线程
        if (sta == OVER) return;
    }
}
