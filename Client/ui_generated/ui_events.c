#include "ui.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "../network/network_client.h"
#include "../game/board_view.h"

bool is_player_ready = false;
extern lv_timer_t * game_reset_timer;

// 这里的声明其实可以去掉，因为我们引用了 network_client.h
// 但留着也没事
extern void send_raw(void *data, int len); 

// 登录按钮 (已自动适配 JSON，因为 net_send_login 改过了)
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

// 注册按钮 (已自动适配 JSON)
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

// 退出登录按钮 (已自动适配 JSON)
void OnLogoutClicked(lv_event_t * e)
{
    printf("[UI] 玩家点击了注销 (Logout)\n");
    net_send_logout();
    _ui_screen_change(&ui_ScreenLogin, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_ScreenLogin_screen_init);
}

// 刷新房间列表 (已自动适配 JSON)
void OnRefreshClicked(lv_event_t * e) {
    net_send_get_room_list(); 
}

// 创建房间 (已自动适配 JSON)
void OnCreateRoomClicked(lv_event_t * e) {
    net_send_create_room();   
}

// 离开房间 (已自动适配 JSON)
void OnLeaveRoomClicked(lv_event_t * e)
{
    printf("[UI] 玩家点击了离开房间按钮\n");
    net_send_leave_room();

    // 杀掉定时器逻辑
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

// ★★★ 核心修改：这里必须把 send_raw 改成 JSON 发送函数 ★★★
void OnReadyClicked(lv_event_t * e) {
    if (!is_player_ready) {
        // --- 动作：准备 (发送 JSON) ---
        // 旧代码: uint8_t cmd = CMD_READY; send_raw(&cmd, 1);
        
        // 新代码:
        net_send_ready_action(true); 
        
        // 更新 UI
        lv_label_set_text(ui_Labelreadybtninfo, "I'm Ready");
        is_player_ready = true;
    } else {
        // --- 动作：取消准备 (发送 JSON) ---
        // 旧代码: uint8_t cmd = CMD_CANCEL_READY; send_raw(&cmd, 1);
        
        // 新代码:
        net_send_ready_action(false);
        
        // 更新 UI
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