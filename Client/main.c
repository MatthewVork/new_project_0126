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

// --- 前置声明 ---
void hide_all_popups();

// --- 事件回调：点击房间卡片 ---
// 这个函数会在点击动态生成的房间卡片时触发
void event_join_room_handler(lv_event_t * e) {
    lv_obj_t * item = lv_event_get_target(e);
    
    // 从 user_data 中取出绑定的房间ID
    // (intptr_t 用于确保指针和整数转换的兼容性)
    int room_id = (int)(intptr_t)lv_obj_get_user_data(item);
    
    printf("[UI] 玩家点击了房间 #%d，正在发送加入请求...\n", room_id);
    
    // 调用网络层发送加入请求
    net_send_join_room(room_id);
}

// --- 定时器回调函数 ---

// 1. [注册成功] -> 跳转登录页
void timer_reg_success_callback(lv_timer_t * timer) {
    hide_all_popups();
    _ui_screen_change(&ui_ScreenLogin, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLogin_screen_init);
    printf("[System] 注册完成，自动跳转登录页\n");
}

// 2. [登录成功] -> 跳转大厅
void timer_login_success_callback(lv_timer_t * timer) {
    hide_all_popups();
    _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLobby_screen_init);
    
    // 进入大厅后，自动刷新一次房间列表
    printf("[System] 进入大厅，自动刷新列表\n");
    net_send_get_room_list(); 
}

// 3. [失败/通用] -> 仅关闭弹窗
void timer_close_popup_callback(lv_timer_t * timer) {
    hide_all_popups(); 
    printf("[System] 自动关闭提示框\n");
}

// --- 辅助：隐藏所有可能的弹窗 ---
void hide_all_popups() {
    // 登录页弹窗
    if(ui_PanelLoginSuccess) lv_obj_add_flag(ui_PanelLoginSuccess, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelLoginFail)    lv_obj_add_flag(ui_PanelLoginFail, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelLoginRepeat)  lv_obj_add_flag(ui_PanelLoginRepeat, LV_OBJ_FLAG_HIDDEN);
    
    // 注册页弹窗
    if(ui_PanelRegSuccess) lv_obj_add_flag(ui_PanelRegSuccess, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegFail)    lv_obj_add_flag(ui_PanelRegFail, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegRepeat)  lv_obj_add_flag(ui_PanelRegRepeat, LV_OBJ_FLAG_HIDDEN);
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

    // --- 3. 输入设备驱动 (鼠标) ---
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);

    // --- 4. 键盘驱动 (物理键盘) ---
    static lv_indev_drv_t kb_drv;
    lv_indev_drv_init(&kb_drv);
    kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = sdl_keyboard_read;
    lv_indev_t * kb_indev = lv_indev_drv_register(&kb_drv);
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(kb_indev, g);

    // --- 5. UI 初始化 ---
    ui_init(); 
    
    // 绑定输入框焦点
    if(ui_InputUser) lv_group_add_obj(g, ui_InputUser);
    if(ui_InputPass) lv_group_add_obj(g, ui_InputPass);
    if(ui_BtnLogin)  lv_group_add_obj(g, ui_BtnLogin);

    // --- 6. 网络初始化 ---
    printf("正在连接服务器...\n");
    // ⚠️ 如果是局域网联机，请修改 IP 为服务器真实 IP
    if (net_init("127.0.0.1", 8888) != 0) {
        printf("连接服务器失败，程序退出。\n");
        return -1;
    }
    printf("连接成功！\n");

    // --- 7. 主循环 ---
    while(1) {
        lv_timer_handler(); // 处理 UI 刷新

        uint8_t buffer[1024];
        int len = net_poll(buffer); // 处理网络接收

        if (len > 0) {
            uint8_t cmd = buffer[0];
            
            // ============================
            // 模块 1: 登录/注册结果处理
            // ============================
            if (cmd == CMD_AUTH_RESULT) {
                ResultPacket* res = (ResultPacket*)buffer;
                
                // --- 场景 A: 注册页 ---
                if (lv_scr_act() == ui_ScreenRegister) {
                    hide_all_popups(); 
                    if (res->success) {
                        printf("[UI-Reg] 注册成功\n");
                        if(ui_PanelRegSuccess) lv_obj_clear_flag(ui_PanelRegSuccess, LV_OBJ_FLAG_HIDDEN);
                        lv_timer_t * t = lv_timer_create(timer_reg_success_callback, 1500, NULL);
                        lv_timer_set_repeat_count(t, 1);
                    } else {
                        if (strstr(res->message, "Exist") != NULL || strstr(res->message, "Repeat") != NULL) {
                            if(ui_PanelRegRepeat) lv_obj_clear_flag(ui_PanelRegRepeat, LV_OBJ_FLAG_HIDDEN);
                        } else {
                            if(ui_PanelRegFail) lv_obj_clear_flag(ui_PanelRegFail, LV_OBJ_FLAG_HIDDEN);
                        }
                        lv_timer_t * t = lv_timer_create(timer_close_popup_callback, 1500, NULL);
                        lv_timer_set_repeat_count(t, 1);
                    }
                }
                // --- 场景 B: 登录页 ---
                else if (lv_scr_act() == ui_ScreenLogin) {
                    hide_all_popups();
                    if (res->success) {
                        printf("[UI-Login] 登录成功 -> 显示成功提示\n");
                        if(ui_PanelLoginSuccess) lv_obj_clear_flag(ui_PanelLoginSuccess, LV_OBJ_FLAG_HIDDEN);
                        lv_timer_t * t = lv_timer_create(timer_login_success_callback, 1500, NULL);
                        lv_timer_set_repeat_count(t, 1);
                    } else {
                        printf("[UI-Login] 登录失败: %s\n", res->message);
                        if (strstr(res->message, "Repeat") != NULL || strstr(res->message, "already") != NULL) {
                            if(ui_PanelLoginRepeat) lv_obj_clear_flag(ui_PanelLoginRepeat, LV_OBJ_FLAG_HIDDEN);
                        } else {
                            if(ui_PanelLoginFail) lv_obj_clear_flag(ui_PanelLoginFail, LV_OBJ_FLAG_HIDDEN);
                        }
                        lv_timer_t * t = lv_timer_create(timer_close_popup_callback, 1500, NULL);
                        lv_timer_set_repeat_count(t, 1);
                    }
                }
            }
            
            // ============================
            // 模块 2: 房间列表刷新 (新增)
            // ============================
            else if (cmd == CMD_ROOM_LIST_RES) {
                // 协议格式：[CMD] [Count] [Room1] [Room2] ...
                int count = buffer[1];
                RoomInfo *rooms = (RoomInfo*)(buffer + 2);

                printf("[UI] 刷新房间列表: 收到 %d 个房间\n", count);

                // 1. 清理旧卡片 (保留 Hidden 的 Template)
                uint32_t child_cnt = lv_obj_get_child_cnt(ui_PanelRoomContainer);
                for(int i = child_cnt - 1; i >= 0; i--) {
                    lv_obj_t * child = lv_obj_get_child(ui_PanelRoomContainer, i);
                    if (child != ui_PanelRoomTemplate) {
                        lv_obj_del(child);
                    }
                }

                // 2. 动态生成新卡片
                for(int i=0; i<count; i++) {
                    // 创建卡片
                    lv_obj_t * item = lv_obj_create(ui_PanelRoomContainer);
                    lv_obj_set_size(item, 260, 120);
                    lv_obj_set_style_bg_color(item, lv_color_hex(0x404040), 0);
                    lv_obj_set_style_border_width(item, 2, 0);
                    lv_obj_set_style_radius(item, 10, 0);
                    
                    // 绑定 user_data (存房间ID)
                    lv_obj_set_user_data(item, (void*)(intptr_t)rooms[i].room_id);

                    // 房间号 Label
                    lv_obj_t * label_id = lv_label_create(item);
                    lv_label_set_text_fmt(label_id, "Room #%03d", rooms[i].room_id);
                    lv_obj_align(label_id, LV_ALIGN_TOP_MID, 0, 15);

                    // 状态 Label
                    lv_obj_t * label_stat = lv_label_create(item);
                    if(rooms[i].status == 0) {
                        lv_label_set_text_fmt(label_stat, "Waiting (%d/2)", rooms[i].player_count);
                        lv_obj_set_style_text_color(label_stat, lv_color_hex(0x00FF00), 0); // 绿色
                    } else {
                        lv_label_set_text(label_stat, "Playing (2/2)");
                        lv_obj_set_style_text_color(label_stat, lv_color_hex(0xFF0000), 0); // 红色
                    }
                    lv_obj_align(label_stat, LV_ALIGN_BOTTOM_MID, 0, -15);

                    // 绑定点击加入事件
                    lv_obj_add_event_cb(item, event_join_room_handler, LV_EVENT_CLICKED, NULL);
                }
            }

            // ============================
            // 模块 3: 房间操作结果 (新增)
            // ============================
            else if (cmd == CMD_ROOM_RESULT) {
                ResultPacket* res = (ResultPacket*)buffer;
                if(res->success) {
                    printf("[UI] 加入/创建房间成功: %s\n", res->message);
                    // TODO: 这里未来要写跳转到【下棋界面】的代码
                    // 例如: _ui_screen_change(&ui_ScreenGame, ...);
                } else {
                    printf("[UI] 加入/创建失败: %s\n", res->message);
                    // 可以弹一个简单的 Toast 提示，目前先打印日志
                }
            }
        }

        usleep(5000); // 5ms 睡眠，降低 CPU 占用
        lv_tick_inc(5);
    }
    return 0;
}