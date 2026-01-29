#include "lvgl/lvgl.h"
#include "lv_drivers/sdl/sdl.h"
#include "ui_generated/ui.h"
#include "network/network_client.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <sys/socket.h> 
#include "../Common/game_protocol.h" 

// 引入游戏绘图模块
#include "game/game_config.h"
#include "game/board_view.h"

// --- 全局变量 ---
extern bool is_player_ready;
int my_game_color = -1;     // 0=黑(Black), 1=白(White)
int current_game_turn = 0;  // 当前轮到谁 (0=黑, 1=白)
lv_timer_t * game_reset_timer = NULL;

// 全局名字缓存
char p1_name_cache[32] = "";
char p2_name_cache[32] = "";

#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 480

// --- 前置声明 ---
void hide_all_popups();
void timer_reset_game(lv_timer_t * timer); 
extern void send_raw(void *data, int len); // 声明 network_client.c 里的函数

// --- 强制刷新标签函数 ---
void ui_reset_labels_to_waiting() {
    if (ui_LabelPlayer1) {
        lv_label_set_recolor(ui_LabelPlayer1, true);
        if (p1_name_cache[0] != '\0') lv_label_set_text_fmt(ui_LabelPlayer1, "Host: %s\n#ffff00 [WAITING]#", p1_name_cache);
        else lv_label_set_text(ui_LabelPlayer1, "Host: ...\n#ffff00 [WAITING]#");
    }
    if (ui_LabelPlayer2) {
        lv_label_set_recolor(ui_LabelPlayer2, true);
        if (p2_name_cache[0] != '\0') lv_label_set_text_fmt(ui_LabelPlayer2, "Player: %s\n#ffff00 [WAITING]#", p2_name_cache);
        else lv_label_set_text(ui_LabelPlayer2, "Player: ...\n#ffff00 [WAITING]#");
    }
}

// --- 定时器回调 ---
void timer_reset_game(lv_timer_t * timer) {
    if (ui_PanelMatchWin) lv_obj_add_flag(ui_PanelMatchWin, LV_OBJ_FLAG_HIDDEN);
    if (ui_PanelMatchLoss) lv_obj_add_flag(ui_PanelMatchLoss, LV_OBJ_FLAG_HIDDEN);
    if (ui_ImgTurnP1) lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);
    if (ui_ImgTurnP2) lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);
    if(ui_BoardContainer) board_view_clear(ui_BoardContainer);
    if (ui_Button5) lv_obj_clear_flag(ui_Button5, LV_OBJ_FLAG_HIDDEN);
    if (ui_Labelreadybtninfo) lv_label_set_text(ui_Labelreadybtninfo, "Ready?");
    game_reset_timer = NULL;
}

void update_turn_ui() {
    if (!ui_ImgTurnP1 || !ui_ImgTurnP2) return;
    if (current_game_turn == 0) { 
        lv_obj_clear_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN); 
        lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);   
    } else { 
        lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);   
        lv_obj_clear_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN); 
    }
}

void timer_hide_start_panel(lv_timer_t * timer) {
    if (ui_PanelStartTip) lv_obj_add_flag(ui_PanelStartTip, LV_OBJ_FLAG_HIDDEN);
}

// --- 事件处理 ---
void event_board_click_handler(lv_event_t * e) {
    if (my_game_color != current_game_turn) return;
    int x, y;
    if (board_view_get_click_xy(e->target, &x, &y)) {
        StonePacket pkt;
        pkt.cmd = CMD_PLACE_STONE;
        pkt.x = x; pkt.y = y; pkt.color = my_game_color; 
        send_raw(&pkt, sizeof(pkt));
    }
}

void event_join_room_handler(lv_event_t * e) {
    lv_obj_t * item = lv_event_get_target(e);
    int room_id = (int)(intptr_t)lv_obj_get_user_data(item);
    net_send_join_room(room_id);
}

void timer_reg_success_callback(lv_timer_t * timer) {
    hide_all_popups();
    _ui_screen_change(&ui_ScreenLogin, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLogin_screen_init);
}
void timer_login_success_callback(lv_timer_t * timer) {
    hide_all_popups();
    _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLobby_screen_init);
    net_send_get_room_list(); 
}
void timer_close_popup_callback(lv_timer_t * timer) { hide_all_popups(); }

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

    // 驱动初始化
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

    ui_init();
    
    if (ui_BoardContainer) {
        lv_obj_set_style_pad_all(ui_BoardContainer, 0, 0);       
        lv_obj_set_style_border_width(ui_BoardContainer, 0, 0);  
        lv_obj_add_event_cb(ui_BoardContainer, event_board_click_handler, LV_EVENT_CLICKED, NULL);
    }
    if(ui_InputUser) lv_group_add_obj(g, ui_InputUser);
    if(ui_InputPass) lv_group_add_obj(g, ui_InputPass);
    if(ui_BtnLogin)  lv_group_add_obj(g, ui_BtnLogin);

    if(ui_ImgTurnP1) lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);
    if(ui_ImgTurnP2) lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);

    if (net_init("172.24.139.145", 8888) != 0) {
        printf("Connect failed\n");
        return -1;
    }

    // --- 主循环 ---
    while(1) {
        lv_timer_handler(); 
        uint8_t buffer[1024];
        int len = net_poll(buffer);

        if (len > 0) {
            int offset = 0;
            // ★★★ 核心修复：循环解析所有包！不再丢包！ ★★★
            while (offset < len) {
                uint8_t cmd = buffer[offset];
                int packet_len = 0;
                uint8_t* pData = buffer + offset;

                // 根据 cmd 计算包长度，防止越界读取
                if (cmd == CMD_AUTH_RESULT) packet_len = sizeof(ResultPacket);
                else if (cmd == CMD_ROOM_LIST_RES) {
                    int count = buffer[offset + 1];
                    packet_len = 2 + count * sizeof(RoomInfo);
                }
                else if (cmd == CMD_ROOM_RESULT) packet_len = sizeof(ResultPacket);
                else if (cmd == CMD_ROOM_UPDATE) packet_len = sizeof(RoomUpdatePacket);
                else if (cmd == CMD_GAME_START) packet_len = sizeof(GameStartPacket);
                else if (cmd == CMD_PLACE_STONE) packet_len = sizeof(StonePacket);
                else if (cmd == CMD_GAME_OVER) packet_len = sizeof(GameOverPacket);
                else {
                    printf("未知指令: %d, 跳过剩余数据\n", cmd);
                    break;
                }

                if (offset + packet_len > len) {
                    printf("数据包不完整，丢弃\n");
                    break;
                }

                // --- 处理逻辑开始 ---
                if (cmd == CMD_AUTH_RESULT) {
                    ResultPacket* res = (ResultPacket*)pData;
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
                    } else if (lv_scr_act() == ui_ScreenLogin) {
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
                else if (cmd == CMD_ROOM_LIST_RES) {
                    int count = pData[1];
                    RoomInfo *rooms = (RoomInfo*)(pData + 2);
                    uint32_t child_cnt = lv_obj_get_child_cnt(ui_PanelRoomContainer);
                    for(int i = child_cnt - 1; i >= 0; i--) {
                        lv_obj_t * child = lv_obj_get_child(ui_PanelRoomContainer, i);
                        if (child != ui_PanelRoomTemplate) lv_obj_del(child);
                    }
                    for(int i=0; i<count; i++) {
                        lv_obj_t * item = lv_obj_create(ui_PanelRoomContainer);
                        lv_obj_set_size(item, 260, 120);
                        lv_obj_set_style_bg_color(item, lv_color_hex(0x404040), 0);
                        lv_obj_set_style_border_width(item, 2, 0);
                        lv_obj_set_style_radius(item, 10, 0);
                        lv_obj_set_user_data(item, (void*)(intptr_t)rooms[i].room_id);

                        lv_obj_t * label = lv_label_create(item);
                        lv_label_set_text_fmt(label, "Room #%d\n%s", rooms[i].room_id, rooms[i].status==0?"Wait":"Play");
                        lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
                        
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
                else if (cmd == CMD_ROOM_RESULT) {
                    ResultPacket* res = (ResultPacket*)pData;
                    if (res->success) {
                        printf("[UI] 房间操作成功: %s\n", res->message);
                        if (strstr(res->message, "Left")) {
                            _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_ScreenLobby_screen_init);
                            net_send_get_room_list(); 
                        } else {
                            if (lv_scr_act() != ui_ScreenGame) {
                                _ui_screen_change(&ui_ScreenGame, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenGame_screen_init);
                                if(ui_BoardContainer) board_view_clear(ui_BoardContainer); 
                            }
                        }
                    } else {
                        printf("[UI] 房间操作失败: %s\n", res->message);
                    }
                }
                else if (cmd == CMD_ROOM_UPDATE) {
                    RoomUpdatePacket *pkt = (RoomUpdatePacket*)pData;
                    if (ui_LabelPlayer1) {
                        lv_label_set_recolor(ui_LabelPlayer1, true);
                        if (pkt->p1_state == 1) {
                            strncpy(p1_name_cache, pkt->p1_name, 31); 
                            lv_label_set_text_fmt(ui_LabelPlayer1, "Host: %s\n%s", pkt->p1_name, pkt->p1_ready ? "#00ff00 [READY]#" : "#ffff00 [WAITING]#");
                        } else {
                            p1_name_cache[0] = '\0';
                            lv_label_set_text(ui_LabelPlayer1, "Host: [EMPTY]\n#808080 [OFFLINE]#");
                        }
                    }
                    if (ui_LabelPlayer2) {
                        lv_label_set_recolor(ui_LabelPlayer2, true);
                        if (pkt->p2_state == 1) {
                            strncpy(p2_name_cache, pkt->p2_name, 31);
                            lv_label_set_text_fmt(ui_LabelPlayer2, "Player: %s\n%s", pkt->p2_name, pkt->p2_ready ? "#00ff00 [READY]#" : "#ffff00 [WAITING]#");
                        } else {
                            p2_name_cache[0] = '\0';
                            lv_label_set_text(ui_LabelPlayer2, "Waiting for\nPlayer...");
                        }
                    }
                }
                else if (cmd == CMD_GAME_START) {
                    GameStartPacket *pkt = (GameStartPacket*)pData;
                    my_game_color = pkt->your_color;
                    if (game_reset_timer) { lv_timer_del(game_reset_timer); game_reset_timer = NULL; }
                    if (ui_BoardContainer) board_view_clear(ui_BoardContainer);
                    if (ui_PanelMatchWin) lv_obj_add_flag(ui_PanelMatchWin, LV_OBJ_FLAG_HIDDEN);
                    if (ui_PanelMatchLoss) lv_obj_add_flag(ui_PanelMatchLoss, LV_OBJ_FLAG_HIDDEN);
                    if (ui_ImgTurnP1) lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);
                    if (ui_ImgTurnP2) lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);
                    is_player_ready = false; 
                    if(ui_Labelreadybtninfo) lv_label_set_text(ui_Labelreadybtninfo, "Ready?");
                    if (ui_PanelStartTip) {
                        lv_obj_clear_flag(ui_PanelStartTip, LV_OBJ_FLAG_HIDDEN);
                        lv_obj_move_foreground(ui_PanelStartTip);
                        lv_timer_set_repeat_count(lv_timer_create(timer_hide_start_panel, 1500, NULL), 1);
                    }
                    if (ui_Button5) lv_obj_add_flag(ui_Button5, LV_OBJ_FLAG_HIDDEN);
                }
                else if (cmd == CMD_PLACE_STONE) {
                    StonePacket *pkt = (StonePacket*)pData;
                    board_view_draw_stone(ui_BoardContainer, pkt->x, pkt->y, pkt->color);
                    current_game_turn = !current_game_turn;
                    update_turn_ui();
                }
                else if (cmd == CMD_GAME_OVER) {
                    GameOverPacket *pkt = (GameOverPacket*)pData;
                    if (pkt->winner_color == my_game_color) {
                        if (ui_PanelMatchWin) { lv_obj_clear_flag(ui_PanelMatchWin, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(ui_PanelMatchWin); }
                    } else {
                        if (ui_PanelMatchLoss) { lv_obj_clear_flag(ui_PanelMatchLoss, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(ui_PanelMatchLoss); }
                    }
                    ui_reset_labels_to_waiting();
                    if (game_reset_timer) lv_timer_del(game_reset_timer);
                    game_reset_timer = lv_timer_create(timer_reset_game, 3000, NULL);
                    lv_timer_set_repeat_count(game_reset_timer, 1);
                }

                // 移动偏移量，处理下一个包
                offset += packet_len;
            }
        }
        usleep(5000); 
        lv_tick_inc(5);
    }
    return 0;
}