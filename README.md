# 🌟 XY-Azl ModEngine

![Android](https://img.shields.io/badge/Platform-Android-green.svg)
![Kotlin](https://img.shields.io/badge/Language-Kotlin-blue.svg)
![C++](https://img.shields.io/badge/Language-C++-purple.svg)
![License](https://img.shields.io/badge/License-Custom-red.svg)

**XY-Azl ModEngine** 是一个强大的 Android 游戏模组注入与运行框架。本框架将注入器逻辑与实际的 Mod 核心彻底分离，让你能够在不影响官方游戏后续更新的前提下，轻松实现底层逻辑接管与功能定制。

## 📂 项目结构 (Structure)

本项目划分为两个主要子模块：
*   `/packer`：基于 Java/Kotlin 开发的 Android 端启动器 App，负责对官方 APK 进行解包、修改 Classes 字节码注入代理加载逻辑、重打包并签名。
*   `/mod`：基于 C++ 开发的原生 Mod 核心。其中 `mod/maker/stub.cpp` 编译后作为启动注入器 (`libxyazler.so`)，其余部分编译为实际的功能核心库 (`libxyazl.so`)。

## 🚀 详细使用指南 (Usage Guide)

### 方案一：直接使用 Release 预编译文件（推荐）
如果你不想从头配置编译环境，可以直接前往仓库的 **Release** 页面下载编译好的 `packer.apk` 与 `libxyazl.so`。
1.  安装并使用 `packer.apk` 对官方游戏进行重打包（过程见下文）。
2.  将下载的 `libxyazl.so` 放置到指定目录并重命名即可。

### 方案二：从源码完整编译与注入
#### 第一步：编译代理注入器与 Mod 核心
1. 编译 `mod` 目录下的 C++ 工程，系统会通过 `mod/maker/stub.cpp` 编译出对应三种架构（`x86` / `x86_64` / `arm64-v8a`）的代理注入器 `libxyazler.so`，同时也会生成实际功能核心库 `libxyazl.so`。
2. 将编译好的代理注入器 `.so` 文件移动到 `packer/app/src/main/assets/` 目录下，并分别重命名为 `stub_arm64-v8a.so`、`stub_x86.so` 等。
3. 准备一个名为 `xyazl.keystore` 的签名文件放入同级 `assets` 目录中。
4. 使用 Android Studio 编译打包 `/packer` 工程，生成我们的**启动器 APK**。

#### 第二步：应用重打包（手术阶段）
1. 在手机端安装生成的启动器 APK。
2. 打开启动器，选择官方原版游戏 APK，点击开始注入。启动器会自动将其代理寄生并进行流式重打包。
3. **卸载官方原版游戏**，并安装经过启动器重打包后的新 APK。

#### 第三步：放置功能 Mod 核心
重打包仅植入了代理加载器，实际的 Mod 功能需手动放入指定私有目录：
1. 首次打开修改后的游戏（此时会建立对应的数据文件夹）。
2. 使用 MT管理器 等文件管理工具，进入路径：`Android/data/com.xxxxx(你的游戏包名)/files/mod/`。
3. 将之前编译好（或从Release下载）的对应架构的 `libxyazl.so` 复制到该目录下，**并必须重命名**：
   * 实体手机（ARM架构）请重命名为：`arm64.so`
   * 模拟器（x86架构）请重命名为：`x86.so`
4. 重新启动游戏，代理会自动识别并加载该 Mod 核心！

> 💡 **热更新优势**：官方游戏更新或 Mod 功能迭代时，**无需再次重打包**。只需替换 `files/mod/` 目录下的 `arm64.so` / `x86.so` 并重启游戏即可完成更新。

## 💾 数据持久化与多账号隔离

框架自带了完善的数据存储机制，所有持久化文件均保存在 `Android/data/com.xxxxx/files/mod/` 目录下：
*   **功能开关状态**：统一保存在 `.../mod/switch.ini` 中，下次启动自动恢复之前的选项。
*   **多服务器/多 UID 隔离**：针对皮肤解锁、特定角色数据修改等信息，框架会自动在 `mod/` 目录下创建以“对应服务器名称”或“UID”命名的子文件夹，确保不同账号的数据相互独立，互不污染。

## ⚖️ 开源协议与声明 (License & Disclaimer)

### 📌 自定义授权协议 (Anti-Commercial License)
本项目的核心源代码**允许**个人用户进行：
✔️ 个人学习与日常使用
✔️ 代码修改与二次开发
✔️ 衍生版本的无署名分发
**严禁任何形式的商业行为**：
❌ 绝对禁止将本项目或其任何衍生版本用于出售、倒卖、盈利性质的代打包或任何商业变现逻辑。一经发现，保留追究权利。

### 📌 第三方库鸣谢 (Acknowledgments)
本项目在开发过程中引用了优秀的开源 Hook 框架 [Dobby](https://github.com/jmpews/Dobby)。
Dobby 采用 **Apache License 2.0** 协议开源，在此对其作者的卓越贡献表示由衷的感谢。