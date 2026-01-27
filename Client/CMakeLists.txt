cmake_minimum_required(VERSION 3.10)
project(ChessClient C)

# 设置 C 标准
set(CMAKE_C_STANDARD 99)

# 编译选项 (优化等级 O3, 调试信息 g)
add_compile_options(-O3 -g)

# --- 1. 查找依赖库 (SDL2) ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# --- 2. 包含头文件路径 (Include Directories) ---
include_directories(
    .                   # 根目录 (包含 lv_conf.h, lv_drv_conf.h)
    ../Common           # 公共协议
    game                # 游戏逻辑
    network             # 网络模块
    ui_generated        # SLS 生成的 UI
    lvgl                # LVGL 库
    lv_drivers          # 驱动库
)

# --- 3. 定义宏 ---
# 告诉 LVGL 配置文件 lv_conf.h 在项目根目录下，而不是在 lvgl/ 里面
add_definitions(-DLV_CONF_INCLUDE_SIMPLE)
add_definitions(-DLV_LVGL_H_INCLUDE_SIMPLE)
# 告诉驱动 lv_drv_conf.h 也在根目录下
#add_definitions(-DLV_DRV_NO_CONF) 

# --- 4. 收集源文件 (Source Files) ---

# A. 你的项目代码
file(GLOB_RECURSE MY_SOURCES 
    "main.c"
    "network/*.c"
    "game/*.c"
    "ui_generated/*.c"
)

# B. LVGL 核心代码 (递归扫描 lvgl/src 下的所有 .c)
file(GLOB_RECURSE LVGL_SOURCES "lvgl/src/*.c")

# C. LVGL 驱动代码 (只扫描 lv_drivers 根目录和 sdl 目录)
# 注意：不要递归扫描 lv_drivers 所有目录，因为里面包含 Wayland/X11 等你可能没有装库的驱动，会导致编译失败。
file(GLOB DRIVERS_SOURCES 
    "lv_drivers/*.c" 
    "lv_drivers/sdl/*.c"
)

# --- 5. 生成可执行文件 ---
add_executable(game_client
    ${MY_SOURCES}
    ${LVGL_SOURCES}
    ${DRV_SOURCES} # 如果上面 DRIVERS_SOURCES 变量名写错了，修正为 DRV_SOURCES 逻辑:
    ${DRIVERS_SOURCES}
    "../Common/cJson.c"  # 公共协议的 cJSON 实现文件
)

# --- 6. 链接库文件 ---
# 链接 SDL2, Math(m), Thread(pthread)
target_link_libraries(game_client
    ${SDL2_LIBRARIES}
    m
    pthread
)