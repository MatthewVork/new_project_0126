#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "../network/network_client.h"
#include "../game/board_view.h"

bool is_player_ready = false;
extern lv_timer_t * game_reset_timer;

extern void send_raw(void *data, int len); 

void OnLoginClicked(lv_event_t * e)
{
    const char* username = lv_textarea_get_text(ui_InputUser);
    const char* password = lv_textarea_get_text(ui_InputPass);

    if (strlen(username) == 0 || strlen(password) == 0) {
        printf("[UI] Input cannot be empty\n");
        return;
    }
    printf("[UI] Login Request: %s\n", username);
    net_send_login(username, password);
}

void OnRegisterClicked(lv_event_t * e)
{
    const char* username = lv_textarea_get_text(ui_RegUser);
    const char* password = lv_textarea_get_text(ui_RegPass);
    const char* confirm  = lv_textarea_get_text(ui_RegPassConfirm);

    if (strlen(username) == 0 || strlen(password) == 0) return;

    if (strcmp(password, confirm) != 0) {
        printf("[UI] Passwords do not match!\n");
        return;
    }
    printf("[UI] Register Request: %s\n", username);
    net_send_register(username, password);
}

void OnLogoutClicked(lv_event_t * e)
{
    printf("[UI] 玩家点击了注销 (Logout)\n");
    net_send_logout();
    _ui_screen_change(&ui_ScreenLogin, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_ScreenLogin_screen_init);
}

void OnRefreshClicked(lv_event_t * e) {
    net_send_get_room_list(); 
}

void OnCreateRoomClicked(lv_event_t * e) {
    net_send_create_room();   
}

void OnLeaveRoomClicked(lv_event_t * e)
{
    printf("[UI] 玩家点击了离开房间按钮\n");
    net_send_leave_room();

    if (game_reset_timer) {
        lv_timer_del(game_reset_timer);
        game_reset_timer = NULL;
    }

    if (ui_BoardContainer) board_view_clear(ui_BoardContainer);
    if (ui_ImgTurnP1) lv_obj_add_flag(ui_ImgTurnP1, LV_OBJ_FLAG_HIDDEN);
    if (ui_ImgTurnP2) lv_obj_add_flag(ui_ImgTurnP2, LV_OBJ_FLAG_HIDDEN);
    if (ui_PanelMatchWin) lv_obj_add_flag(ui_PanelMatchWin, LV_OBJ_FLAG_HIDDEN);
    if (ui_PanelMatchLoss) lv_obj_add_flag(ui_PanelMatchLoss, LV_OBJ_FLAG_HIDDEN);

    is_player_ready = false;
    if (ui_Labelreadybtninfo) lv_label_set_text(ui_Labelreadybtninfo, "Ready");
    if (ui_Button5) lv_obj_clear_flag(ui_Button5, LV_OBJ_FLAG_HIDDEN);
}

// ★★★ 核心修复：添加 500ms 防抖 + 确认使用 JSON 发送 ★★★
void OnReadyClicked(lv_event_t * e) {
    static uint32_t last_click = 0;
    if (lv_tick_elaps(last_click) < 500) return; // 防抖
    last_click = lv_tick_get();

    if (!is_player_ready) {
        net_send_ready_action(true); 
        lv_label_set_text(ui_Labelreadybtninfo, "I'm Ready");
        is_player_ready = true;
    } else {
        net_send_ready_action(false);
        lv_label_set_text(ui_Labelreadybtninfo, "Not Ready");
        is_player_ready = false;
    }
}

void Texture_clean(lv_event_t * e)
{
	lv_textarea_set_text(ui_InputUser, "");
    lv_textarea_set_text(ui_InputPass, "");
    lv_textarea_set_text(ui_RegUser, "");
    lv_textarea_set_text(ui_RegPass, "");
    lv_textarea_set_text(ui_RegPassConfirm, "");
}