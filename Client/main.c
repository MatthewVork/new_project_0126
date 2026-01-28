#include "lvgl/lvgl.h"
#include "lv_drivers/sdl/sdl.h"
#include "ui_generated/ui.h"
#include "network/network_client.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include "../Common/game_protocol.h" 

// ★★★ 新增：引入游戏绘图模块 ★★★
#include "game/game_config.h"
#include "game/board_view.h"

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 480

// --- 全局游戏状态 ---
int my_game_color = -1;     // 0=黑(Black), 1=白(White)
int current_game_turn = 0;  // 当前轮到谁 (0=黑, 1=白)

// --- 前置声明 ---
void hide_all_popups();

// --- 【新增功能】更新 UI 上的箭头指示 ---
void update_turn_ui() {
    if (!ui_ImgTurnP1 || !ui_ImgTurnP2) return;

    if (current_game_turn == COLOR_BLACK) {
        // 轮到黑棋 (P1)
        lv_obj_clear_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);   
        printf("[UI] 轮到黑方 (P1) -> 显示左箭头\n");
    } else {
        // 轮到白棋 (P2)
        lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);   
        lv_obj_clear_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN); 
        printf("[UI] 轮到白方 (P2) -> 显示右箭头\n");
    }
}

// --- 【新增功能】3秒后自动隐藏“游戏开始”面板 ---
void timer_hide_start_panel(lv_timer_t * timer) {
    if (ui_PanelStartTip) {
        lv_obj_add_flag(ui_PanelStartTip, LV_OBJ_FLAG_HIDDEN);
        printf("[UI] 游戏开始提示已自动关闭\n");
    }
}

// --- 【新增功能】棋盘点击事件 ---
void event_board_click_handler(lv_event_t * e) {
    if (my_game_color != current_game_turn) {
        printf("[Game] 还没轮到你！当前轮到: %d, 你是: %d\n", current_game_turn, my_game_color);
        return;
    }
    int x, y;
    if (board_view_get_click_xy(e->target, &x, &y)) {
        StonePacket pkt;
        pkt.cmd = CMD_PLACE_STONE;
        pkt.x = x;
        pkt.y = y;
        pkt.color = my_game_color; 
        send_raw(&pkt, sizeof(pkt));
        printf("[Game] 请求落子: (%d, %d)\n", x, y);
    }
}

// --- 【原有逻辑】点击房间卡片 ---
void event_join_room_handler(lv_event_t * e) {
    lv_obj_t * item = lv_event_get_target(e);
    int room_id = (int)(intptr_t)lv_obj_get_user_data(item);
    printf("[UI] 玩家点击了房间 #%d，正在发送加入请求...\n", room_id);
    net_send_join_room(room_id);
}

// --- 【原有逻辑】定时器回调函数 ---
void timer_reg_success_callback(lv_timer_t * timer) {
    hide_all_popups();
    _ui_screen_change(&ui_ScreenLogin, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLogin_screen_init);
}
void timer_login_success_callback(lv_timer_t * timer) {
    hide_all_popups();
    _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLobby_screen_init);
    net_send_get_room_list(); 
}
void timer_close_popup_callback(lv_timer_t * timer) {
    hide_all_popups(); 
}

// --- 【原有逻辑】隐藏所有弹窗 ---
void hide_all_popups() {
    if(ui_PanelLoginSuccess) lv_obj_add_flag(ui_PanelLoginSuccess, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelLoginFail)    lv_obj_add_flag(ui_PanelLoginFail, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelLoginRepeat)  lv_obj_add_flag(ui_PanelLoginRepeat, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegSuccess)   lv_obj_add_flag(ui_PanelRegSuccess, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegFail)      lv_obj_add_flag(ui_PanelRegFail, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegRepeat)    lv_obj_add_flag(ui_PanelRegRepeat, LV_OBJ_FLAG_HIDDEN);
}

int main(void)
{
    lv_init();
    sdl_init();

    // --- 【原有逻辑】驱动部分 ---
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

    // --- UI 初始化 ---
    ui_init(); 
    if(ui_InputUser) lv_group_add_obj(g, ui_InputUser);
    if(ui_InputPass) lv_group_add_obj(g, ui_InputPass);
    if(ui_BtnLogin)  lv_group_add_obj(g, ui_BtnLogin);

    // ★★★ 新增：绑定棋盘点击 ★★★
    if (ui_BoardContainer) {
        lv_obj_add_event_cb(ui_BoardContainer, event_board_click_handler, LV_EVENT_CLICKED, NULL);
    }
    // ★★★ 新增：初始化箭头隐藏 ★★★
    if(ui_ImgTurnP1) lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);
    if(ui_ImgTurnP2) lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);

    // --- 网络初始化 ---
    if (net_init("127.0.0.1", 8888) != 0) {
        printf("Connect failed\n");
        return -1;
    }

    // --- 主循环 ---
    while(1) {
        lv_timer_handler(); 
        uint8_t buffer[1024];
        int len = net_poll(buffer);

        if (len > 0) {
            uint8_t cmd = buffer[0];

            // 模块 1: 【原有逻辑】登录/注册
            if (cmd == CMD_AUTH_RESULT) {
                ResultPacket* res = (ResultPacket*)buffer;
                if (lv_scr_act() == ui_ScreenRegister) {
                    hide_all_popups(); 
                    if (res->success) {
                        if(ui_PanelRegSuccess) lv_obj_clear_flag(ui_PanelRegSuccess, LV_OBJ_FLAG_HIDDEN);
                        lv_timer_set_repeat_count(lv_timer_create(timer_reg_success_callback, 1500, NULL), 1);
                    } else {
                        if (strstr(res->message, "Exist") || strstr(res->message, "Repeat")) {
                            if(ui_PanelRegRepeat) lv_obj_clear_flag(ui_PanelRegRepeat, LV_OBJ_FLAG_HIDDEN);
                        } else {
                            if(ui_PanelRegFail) lv_obj_clear_flag(ui_PanelRegFail, LV_OBJ_FLAG_HIDDEN);
                        }
                        lv_timer_set_repeat_count(lv_timer_create(timer_close_popup_callback, 1500, NULL), 1);
                    }
                }
                else if (lv_scr_act() == ui_ScreenLogin) {
                    hide_all_popups();
                    if (res->success) {
                        if(ui_PanelLoginSuccess) lv_obj_clear_flag(ui_PanelLoginSuccess, LV_OBJ_FLAG_HIDDEN);
                        lv_timer_set_repeat_count(lv_timer_create(timer_login_success_callback, 1500, NULL), 1);
                    } else {
                        if (strstr(res->message, "Repeat") || strstr(res->message, "already")) {
                            if(ui_PanelLoginRepeat) lv_obj_clear_flag(ui_PanelLoginRepeat, LV_OBJ_FLAG_HIDDEN);
                        } else {
                            if(ui_PanelLoginFail) lv_obj_clear_flag(ui_PanelLoginFail, LV_OBJ_FLAG_HIDDEN);
                        }
                        lv_timer_set_repeat_count(lv_timer_create(timer_close_popup_callback, 1500, NULL), 1);
                    }
                }
            }
            
            // 模块 2: 【原有逻辑】房间列表刷新
            else if (cmd == CMD_ROOM_LIST_RES) {
                int count = buffer[1];
                RoomInfo *rooms = (RoomInfo*)(buffer + 2);
                uint32_t child_cnt = lv_obj_get_child_cnt(ui_PanelRoomContainer);
                for(int i = child_cnt - 1; i >= 0; i--) {
                    lv_obj_t * child = lv_obj_get_child(ui_PanelRoomContainer, i);
                    if (child != ui_PanelRoomTemplate) lv_obj_del(child);
                }
                for(int i=0; i<count; i++) {
                    lv_obj_t * item = lv_obj_create(ui_PanelRoomContainer);
                    // 你原来的样式设置
                    lv_obj_set_size(item, 260, 120);
                    lv_obj_set_style_bg_color(item, lv_color_hex(0x404040), 0);
                    lv_obj_set_style_border_width(item, 2, 0);
                    lv_obj_set_style_radius(item, 10, 0);
                    lv_obj_set_user_data(item, (void*)(intptr_t)rooms[i].room_id);

                    lv_obj_t * label = lv_label_create(item);
                    lv_label_set_text_fmt(label, "Room #%d\n%s", rooms[i].room_id, rooms[i].status==0?"Wait":"Play");
                    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
                    
                    // 你原来的状态颜色逻辑
                    lv_obj_t * label_stat = lv_label_create(item);
                    if(rooms[i].status == 0) {
                         lv_label_set_text_fmt(label_stat, "Waiting (%d/2)", rooms[i].player_count);
                         lv_obj_set_style_text_color(label_stat, lv_color_hex(0x00FF00), 0);
                    } else {
                         lv_label_set_text(label_stat, "Playing (2/2)");
                         lv_obj_set_style_text_color(label_stat, lv_color_hex(0xFF0000), 0);
                    }
                    lv_obj_align(label_stat, LV_ALIGN_BOTTOM_MID, 0, -15);
                    
                    lv_obj_add_event_cb(item, event_join_room_handler, LV_EVENT_CLICKED, NULL);
                }
            }

            // 模块 3: 【原有逻辑】房间操作结果 (创建、加入、离开)
            else if (cmd == CMD_ROOM_RESULT) {
                ResultPacket* res = (ResultPacket*)buffer;
                if (res->success) {
                    printf("[UI] 房间操作成功: %s\n", res->message);

                    // A: 在大厅 -> 进房成功 (Create 或 Join)
                    if (lv_scr_act() == ui_ScreenLobby) {
                        _ui_screen_change(&ui_ScreenGame, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenGame_screen_init);
                        if(ui_BoardContainer) board_view_clear(ui_BoardContainer); // 顺手清盘
                    } 
                    // B: 在房间 -> 退房成功 (Leave)
                    else if (lv_scr_act() == ui_ScreenGame) {
                        _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_ScreenLobby_screen_init);
                        net_send_get_room_list(); // 刷新列表
                    }
                } else {
                    printf("[UI] 房间操作失败: %s\n", res->message);
                }
            }

            // 模块 4: 【原有逻辑】房间状态更新
            else if (cmd == CMD_ROOM_UPDATE) {
                RoomUpdatePacket *pkt = (RoomUpdatePacket*)buffer;
                
                if (lv_scr_act() == ui_ScreenGame) {
                    // 更新房主 (P1) 状态
                    if (pkt->p1_state && ui_LabelPlayer1) {
                        lv_label_set_recolor(ui_LabelPlayer1, true);
                        // 根据服务器传来的 p1_ready 决定显示什么，不再依赖你有没有按按钮
                        lv_label_set_text_fmt(ui_LabelPlayer1, "Host: %s\n%s", 
                            pkt->p1_name, 
                            pkt->p1_ready ? "#00ff00 [READY]#" : "#ffff00 [WAITING]#");
                    }
                    
                    // 更新挑战者 (P2) 状态
                    if (pkt->p2_state && ui_LabelPlayer2) {
                        lv_label_set_recolor(ui_LabelPlayer2, true);
                        lv_label_set_text_fmt(ui_LabelPlayer2, "Player: %s\n%s", 
                            pkt->p2_name, 
                            pkt->p2_ready ? "#00ff00 [READY]#" : "#ffff00 [WAITING]#");
                    }
                }
            }

            // ★★★ 模块 5: 游戏开始 (疯狂调试版) ★★★
            else if (cmd == CMD_GAME_START) {
                GameStartPacket *pkt = (GameStartPacket*)buffer;
                my_game_color = pkt->your_color;
                current_game_turn = COLOR_BLACK;

                printf("\n========== [DEBUG] 1. 收到 CMD_GAME_START 信号 ==========\n");

                // 1. 清盘
                if(ui_BoardContainer) {
                    board_view_clear(ui_BoardContainer);
                    printf("[DEBUG] 2. 棋盘已清理\n");
                }
                update_turn_ui(); 

                // 2. 隐藏按钮
                if(ui_Button5) lv_obj_add_flag(ui_Button5, LV_OBJ_FLAG_HIDDEN);

                // 3. 显示弹窗 (重头戏)
                if (ui_PanelStartTip) {
                    printf("[DEBUG] 3. 找到 ui_PanelStartTip 控件，准备执行显示命令...\n");

                    // 动作 A: 置顶
                    lv_obj_move_foreground(ui_PanelStartTip);
                    printf("[DEBUG] 4. >> 已执行 Move Foreground (置顶)\n");
                    
                    // 动作 B: 解除隐藏
                    lv_obj_clear_flag(ui_PanelStartTip, LV_OBJ_FLAG_HIDDEN);
                    printf("[DEBUG] 5. >> 已执行 Clear Hidden (解除隐藏) <<<<<<<<<<< \n");
                    
                    // 动作 C: 强制重绘
                    lv_obj_invalidate(ui_PanelStartTip);
                    printf("[DEBUG] 6. >> 已执行 Invalidate (强制重绘)\n");

                    // 启动定时器
                    lv_timer_set_repeat_count(lv_timer_create(timer_hide_start_panel, 3000, NULL), 1);
                    
                    printf("========== [DEBUG] 7. 【成功执行】所有显示代码已跑完！ ==========\n\n");
                } else {
                    printf("========== [ERROR] 严重错误：ui_PanelStartTip 是空的(NULL)！ ==========\n");
                }
            }

            // ★★★ 模块 6: 【新增】收到落子 (绘图+切回合) ★★★
            else if (cmd == CMD_PLACE_STONE) {
                StonePacket *pkt = (StonePacket*)buffer;
                // 画子
                board_view_draw_stone(ui_BoardContainer, pkt->x, pkt->y, pkt->color);
                // 切换回合
                current_game_turn = !current_game_turn;
                // 更新箭头显示
                update_turn_ui();
            }
        }
        usleep(5000); 
        lv_tick_inc(5);
    }
    return 0;
}