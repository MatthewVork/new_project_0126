#include "lvgl/lvgl.h"
#include "lv_drivers/sdl/sdl.h"
#include "ui_generated/ui.h"
#include "network/network_client.h"
#include "game/chess_data.h"
#include "../Common/cJSON.h"
#include "../Common/game_protocol.h" // <--- 关键修正：加了这一行！
#include <unistd.h>
#include <stdio.h>

// 辅助：隐藏弹窗
void hide_all_popups() {
    if(ui_PanelLoginSuccess) lv_obj_add_flag(ui_PanelLoginSuccess, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelLoginFail)    lv_obj_add_flag(ui_PanelLoginFail, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelLoginRepeat)  lv_obj_add_flag(ui_PanelLoginRepeat, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegSuccess) lv_obj_add_flag(ui_PanelRegSuccess, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegFail)    lv_obj_add_flag(ui_PanelRegFail, LV_OBJ_FLAG_HIDDEN);
    if(ui_PanelRegRepeat)  lv_obj_add_flag(ui_PanelRegRepeat, LV_OBJ_FLAG_HIDDEN);
}

// 定时器回调
void timer_login_success_cb(lv_timer_t * t) {
    hide_all_popups();
    _ui_screen_change(&ui_ScreenLobby, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLobby_screen_init);
    net_send_get_room_list();
}
void timer_reg_success_cb(lv_timer_t * t) {
    hide_all_popups();
    _ui_screen_change(&ui_ScreenLogin, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenLogin_screen_init);
}
void timer_close_popup_cb(lv_timer_t * t) { hide_all_popups(); }

// 点击大厅里的房间
void event_join_room_handler(lv_event_t * e) {
    lv_obj_t * item = lv_event_get_target(e);
    int room_id = (int)(intptr_t)lv_obj_get_user_data(item);
    net_send_join_room(room_id);
}

int main(void) {
    lv_init(); sdl_init();
    
    // --- 驱动初始化 ---
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf[800 * 480 / 10];
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, 800 * 480 / 10);
    static lv_disp_drv_t disp_drv; lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf; disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = 800; disp_drv.ver_res = 480; lv_disp_drv_register(&disp_drv);
    static lv_indev_drv_t indev_drv; lv_indev_drv_init(&indev_drv); indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read; lv_indev_drv_register(&indev_drv);
    static lv_indev_drv_t kb_drv; lv_indev_drv_init(&kb_drv); kb_drv.type = LV_INDEV_TYPE_KEYPAD;
    kb_drv.read_cb = sdl_keyboard_read; lv_indev_t * kb_indev = lv_indev_drv_register(&kb_drv);
    lv_group_t * g = lv_group_create(); lv_group_set_default(g); lv_indev_set_group(kb_indev, g);
    // ----------------------------

    ui_init(); 
    // 绑定键盘组
    if(ui_InputUser) lv_group_add_obj(g, ui_InputUser);
    if(ui_InputPass) lv_group_add_obj(g, ui_InputPass);
    if(ui_BtnLogin)  lv_group_add_obj(g, ui_BtnLogin);

    if(net_init("127.0.0.1", 8888) != 0) return -1;

    while(1) {
        lv_timer_handler();
        uint8_t buffer[4096];
        int len = net_poll(buffer);

        if (len > 0) {
            cJSON *json = cJSON_Parse((char*)buffer);
            if (json) {
                cJSON *cmd_item = cJSON_GetObjectItem(json, "cmd");
                if (cmd_item) {
                    int cmd = cmd_item->valueint;

                    // 1. 登录/注册结果
                    if (cmd == CMD_AUTH_RESULT) {
                        int success = cJSON_GetObjectItem(json, "success")->valueint;
                        char *msg = cJSON_GetObjectItem(json, "message")->valuestring;
                        
                        if (lv_scr_act() == ui_ScreenLogin) {
                            hide_all_popups();
                            if(success) {
                                if(ui_PanelLoginSuccess) lv_obj_clear_flag(ui_PanelLoginSuccess, LV_OBJ_FLAG_HIDDEN);
                                lv_timer_set_repeat_count(lv_timer_create(timer_login_success_cb, 1000, NULL), 1);
                            } else {
                                if(strstr(msg,"Repeat") && ui_PanelLoginRepeat) lv_obj_clear_flag(ui_PanelLoginRepeat, LV_OBJ_FLAG_HIDDEN);
                                else if(ui_PanelLoginFail) lv_obj_clear_flag(ui_PanelLoginFail, LV_OBJ_FLAG_HIDDEN);
                                lv_timer_set_repeat_count(lv_timer_create(timer_close_popup_cb, 1500, NULL), 1);
                            }
                        }
                        else if (lv_scr_act() == ui_ScreenRegister) {
                            hide_all_popups();
                            if(success) {
                                if(ui_PanelRegSuccess) lv_obj_clear_flag(ui_PanelRegSuccess, LV_OBJ_FLAG_HIDDEN);
                                lv_timer_set_repeat_count(lv_timer_create(timer_reg_success_cb, 1500, NULL), 1);
                            } else {
                                if(ui_PanelRegFail) lv_obj_clear_flag(ui_PanelRegFail, LV_OBJ_FLAG_HIDDEN);
                                lv_timer_set_repeat_count(lv_timer_create(timer_close_popup_cb, 1500, NULL), 1);
                            }
                        }
                    }
                    // 2. 房间列表
                    else if (cmd == CMD_ROOM_LIST_RES) {
                        cJSON *arr = cJSON_GetObjectItem(json, "rooms");
                        int cnt = cJSON_GetArraySize(arr);
                        uint32_t child_cnt = lv_obj_get_child_cnt(ui_PanelRoomContainer);
                        for(int i=child_cnt-1; i>=0; i--) {
                            lv_obj_t* child = lv_obj_get_child(ui_PanelRoomContainer, i);
                            if(child != ui_PanelRoomTemplate) lv_obj_del(child);
                        }
                        for(int i=0; i<cnt; i++) {
                            cJSON *item = cJSON_GetArrayItem(arr, i);
                            int rid = cJSON_GetObjectItem(item, "id")->valueint;
                            int players = cJSON_GetObjectItem(item, "players")->valueint;
                            // 创建卡片
                            lv_obj_t * btn = lv_btn_create(ui_PanelRoomContainer);
                            lv_obj_set_size(btn, 260, 120);
                            lv_obj_set_user_data(btn, (void*)(intptr_t)rid);
                            lv_obj_t * l = lv_label_create(btn);
                            lv_label_set_text_fmt(l, "Room %d\n(%d/2)", rid, players);
                            lv_obj_center(l);
                            lv_obj_add_event_cb(btn, event_join_room_handler, LV_EVENT_CLICKED, NULL);
                        }
                    }
                    // 3. 进入房间结果
                    else if (cmd == CMD_ROOM_RESULT) {
                        int success = cJSON_GetObjectItem(json, "success")->valueint;
                        if(success) {
                            _ui_screen_change(&ui_ScreenRoom, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenRoom_screen_init);
                            // 初始化显示
                            lv_label_set_text(ui_LabelP1Name, "Me");
                            lv_label_set_text(ui_LabelP1Status, "Not Ready");
                            lv_label_set_text(ui_LabelP2Name, "Waiting...");
                            lv_label_set_text(ui_LabelP2Status, "");
                        }
                    }
                    // 4. 房间状态更新 (对手进入/准备)
                    else if (cmd == CMD_ROOM_UPDATE) {
                        if(lv_scr_act() == ui_ScreenRoom) {
                            char *p1n = cJSON_GetObjectItem(json, "p1_name")->valuestring;
                            int p1r = cJSON_GetObjectItem(json, "p1_ready")->valueint;
                            char *p2n = cJSON_GetObjectItem(json, "p2_name")->valuestring;
                            int p2r = cJSON_GetObjectItem(json, "p2_ready")->valueint;

                            lv_label_set_text(ui_LabelP1Name, p1n);
                            lv_label_set_text(ui_LabelP1Status, p1r?"READY":"Not Ready");
                            lv_obj_set_style_text_color(ui_LabelP1Status, p1r?lv_color_hex(0x00FF00):lv_color_hex(0xFF0000), 0);

                            lv_label_set_text(ui_LabelP2Name, p2n);
                            lv_label_set_text(ui_LabelP2Status, p2r?"READY":"Not Ready");
                            lv_obj_set_style_text_color(ui_LabelP2Status, p2r?lv_color_hex(0x00FF00):lv_color_hex(0xFF0000), 0);
                        }
                    }
                    // 5. 游戏开始
                    else if (cmd == CMD_GAME_START) {
                        char *opp = cJSON_GetObjectItem(json, "opponent_name")->valuestring;
                        _ui_screen_change(&ui_ScreenGame, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &ui_ScreenGame_screen_init);
                        lv_label_set_text_fmt(ui_LabelGameInfo, "Vs %s", opp);
                        init_chess_board();
                        render_board_ui();
                    }
                }
                cJSON_Delete(json);
            }
        }
        usleep(5000); lv_tick_inc(5);
    }
}