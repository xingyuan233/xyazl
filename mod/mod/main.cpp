#include "./hack.h"

// 模块构造函数，在 so 被加载时自动执行， hook dlopen
extern "C" void __attribute__((visibility("default"))) __attribute__((constructor)) mod_main()
{
    xyu::hack_main();
}
