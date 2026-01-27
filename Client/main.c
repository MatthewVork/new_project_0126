#include "lvgl/lvgl.h"
#include "lv_drivers/sdl/sdl.h"
#include "ui_generated/ui.h"
#include "network/network_client.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 480

// --- 辅助：隐藏所有可能的弹窗 (防止残留) ---
void hide_all_popups() {
    // 1. 登录页面的弹窗
    if(ui_PanelLoginSuccess) lv_obj_add_flag(ui_PanelLoginSuccess, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelLoginFail)    lv_obj_add_flag(ui_PanelLoginFail, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelLoginRepeat)  lv_obj_add_flag(ui_PanelLoginRepeat, LV_OBJ_FLAG_HIDDEN);
    
    // 2. 注册页面的弹窗 (这是这次修复的关键)
    if(ui_PanelRegSuccess) lv_obj_add_flag(ui_PanelRegSuccess, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegFail)    lv_obj_add_flag(ui_PanelRegFail, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegRepeat)  lv_obj_add_flag(ui_PanelRegRepeat, LV_OBJ_FLAG_HIDDEN);
}

// --- 定时器回调函数 ---

// 1. 注册成功 -> 跳转登录页
void timer_reg_success_callback(lv_timer_t * timer) {
    hide_all_popups();
    // 注册成功后，自动跳回登录页让用户登录
    _ui_screen_change(&ui_ScreenLogin, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLogin_screen_init);
    printf("[System] 注册完成，自动跳转登录页\n");
}

// 2. 注册/登录失败 -> 仅关闭弹窗
void timer_close_popup_callback(lv_timer_t * timer) {
    hide_all_popups(); // 简单粗暴，关掉所有弹窗
    printf("[System] 自动关闭提示框\n");
}

int main(void)
{
    // --- 1. 基础初始化 ---
    lv_init();
    sdl_init();

    // --- 2. 显示驱动 ---
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf[WINDOW_WIDTH * WINDOW_HEIGHT / 10];
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, WINDOW_WIDTH * WINDOW_HEIGHT / 10);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = WINDOW_WIDTH;
    disp_drv.ver_res = WINDOW_HEIGHT;
    lv_disp_drv_register(&disp_drv);

    // --- 3. 输入设备驱动 ---
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);

    static lv_indev_drv_t kb_drv;
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = sdl_keyboard_read;
    lv_indev_t * kb_indev = lv_indev_drv_register(&kb_drv);
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(kb_indev, g);

    // --- 4. UI 初始化 ---
    ui_init(); 
    
    // 绑定键盘焦点
    if(ui_InputUser) lv_group_add_obj(g, ui_InputUser);
    if(ui_InputPass) lv_group_add_obj(g, ui_InputPass);
    if(ui_BtnLogin)  lv_group_add_obj(g, ui_BtnLogin);

    // --- 5. 网络初始化 ---
    printf("正在连接服务器...\n");
    if (net_init("127.0.0.1", 8888) != 0) {
        printf("连接服务器失败，程序退出。\n");
        return -1;
    }
    printf("连接成功！\n");

    // --- 6. 主循环 ---
    while(1) {
        lv_timer_handler();

        uint8_t buffer[1024];
        int len = net_poll(buffer); // 轮询网络消息

        if (len > 0) {
            uint8_t cmd = buffer[0];
            
            // === 处理 注册/登录 结果 ===
            if (cmd == CMD_AUTH_RESULT) {
                ResultPacket* res = (ResultPacket*)buffer;
                
                // ---------------------------------------------------------
                // 场景 A: 当前在 [注册页]
                // ---------------------------------------------------------
                if (lv_scr_act() == ui_ScreenRegister) {
                    hide_all_popups(); // 清理旧弹窗

                    if (res->success) {
                        printf("[UI-Reg] 注册成功 -> 显示Reg弹窗\n");
                        // 1. 显示注册成功的弹窗 (注意是用 PanelRegSuccess)
                        if(ui_PanelRegSuccess) lv_obj_clear_flag(ui_PanelRegSuccess, LV_OBJ_FLAG_HIDDEN);
                        
                        // 2. 1.5秒后跳转回登录页
                        lv_timer_t * t = lv_timer_create(timer_reg_success_callback, 1500, NULL);
                        lv_timer_set_repeat_count(t, 1);
                    } 
                    else {
                        // 注册失败
                        if (strstr(res->message, "Exist") != NULL || strstr(res->message, "Repeat") != NULL) {
                            printf("[UI-Reg] 用户名已存在\n");
                            if(ui_PanelRegRepeat) lv_obj_clear_flag(ui_PanelRegRepeat, LV_OBJ_FLAG_HIDDEN);
                        } else {
                            printf("[UI-Reg] 注册通用失败: %s\n", res->message);
                            if(ui_PanelRegFail) lv_obj_clear_flag(ui_PanelRegFail, LV_OBJ_FLAG_HIDDEN);
                        }
                        // 1.5秒后自动关闭提示
                        lv_timer_t * t = lv_timer_create(timer_close_popup_callback, 1500, NULL);
                        lv_timer_set_repeat_count(t, 1);
                    }
                }
                
                // ---------------------------------------------------------
                // 场景 B: 当前在 [登录页]
                // ---------------------------------------------------------
                else if (lv_scr_act() == ui_ScreenLogin) {
                    hide_all_popups();

                    if (res->success) {
                        printf("[UI-Login] 登录成功 -> 进入大厅\n");
                        
                        // 【关键】跳转到大厅 (你之前这里是注释掉的，现在解开了)
                        _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLobby_screen_init);
                        
                    } 
                    else {
                        // 登录失败
                        if (strstr(res->message, "Repeat") != NULL || strstr(res->message, "already") != NULL) {
                            printf("[UI-Login] 重复登录\n");
                            if(ui_PanelLoginRepeat) lv_obj_clear_flag(ui_PanelLoginRepeat, LV_OBJ_FLAG_HIDDEN);
                        } else {
                            printf("[UI-Login] 密码错误或用户不存在\n");
                            if(ui_PanelLoginFail) lv_obj_clear_flag(ui_PanelLoginFail, LV_OBJ_FLAG_HIDDEN);
                        }
                        // 1.5秒后自动关闭提示
                        lv_timer_t * t = lv_timer_create(timer_close_popup_callback, 1500, NULL);
                        lv_timer_set_repeat_count(t, 1);
                    }
                }
            }
        }
        usleep(5000);
        lv_tick_inc(5);
    }
    return 0;
}