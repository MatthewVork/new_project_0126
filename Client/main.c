#include "lvgl/lvgl.h"
#include "lv_drv_conf.h"
#include "lv_drivers/sdl/sdl.h" // 必须包含这个，用于驱动显示器和鼠标
#include "ui_generated/ui.h"    // SLS 生成的 UI
#include "network/network_client.h" // 我们的网络模块
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>

// 窗口尺寸，必须和 SquareLine Studio 里的设置一致
#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 480

int main(void)
{
    // --- 1. 基础设施初始化 ---
    lv_init();
    sdl_init(); // 初始化 SDL 驱动

    // --- 2. 显示器驱动配置 (Display Buffer & Driver) ---
    // 创建显示缓冲区
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf[WINDOW_WIDTH * WINDOW_HEIGHT / 10]; // 1/10 屏幕大小的缓冲区
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, WINDOW_WIDTH * WINDOW_HEIGHT / 10);

    // 注册显示驱动
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush; // 使用 SDL 的刷新函数
    disp_drv.hor_res = WINDOW_WIDTH;
    disp_drv.ver_res = WINDOW_HEIGHT;
    lv_disp_drv_register(&disp_drv);

    // --- 3. 输入设备配置 (Mouse) ---
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;    // 使用 SDL 的鼠标读取函数
    lv_indev_drv_register(&indev_drv);

    // --- 4. 业务初始化 ---
    ui_init(); // 初始化界面
    
    printf("正在连接服务器...\n");
    if (net_init("127.0.0.1", 8888) != 0) {
        printf("连接服务器失败！请确认服务器已启动。\n");
        return -1;
    }
    printf("连接成功！\n");

    // --- 5. 主循环 ---
    while(1) {
        // A. LVGL 事务处理 (图形刷新、点击事件)
        lv_timer_handler();

        // B. 网络轮询 (非阻塞读取)
        uint8_t buffer[1024];
        int len = net_poll(buffer);
        if (len > 0) {
            uint8_t cmd = buffer[0];
            
            // 处理登录结果
            if (cmd == CMD_AUTH_RESULT) {
                ResultPacket* res = (ResultPacket*)buffer;
                printf("[Server] %s: %s\n", res->success ? "成功" : "失败", res->message);
                
                // 如果登录成功，甚至可以在这里弹出一个 Toast 提示
                // lv_obj_t * label = lv_label_create(lv_scr_act());
                // lv_label_set_text(label, res->message);
            }
            // 处理房间创建结果
            else if (cmd == CMD_ROOM_RESULT) {
                ResultPacket* res = (ResultPacket*)buffer;
                printf("[Server] 房间: %s\n", res->message);
            }
        }

        usleep(5000); // 休息 5ms，避免 CPU 占用 100%
        lv_tick_inc(5); // 告诉 LVGL 时间流逝了 5ms
    }

    return 0;
}