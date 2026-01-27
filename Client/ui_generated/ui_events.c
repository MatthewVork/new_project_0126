#include "ui.h"
#include <stdio.h>
#include <string.h>
#include "../network/network_client.h"

// 登录按钮
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

// 注册按钮
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

// 退出登录按钮 (新增)
void OnLogoutClicked(lv_event_t * e)
{
    printf("[UI] 玩家点击了注销 (Logout)\n");

    // 1. 发送网络请求：告诉服务器清除我的在线状态
    // (这需要你在 network_client.c 里已经实现了 net_send_logout)
    net_send_logout();
    
    // 2. 界面跳转：切回登录页面
    // 参数说明: 目标屏幕, 动画方式(向右滑出), 动画时间500ms, 延迟0ms, 初始化函数
    _ui_screen_change(&ui_ScreenLogin, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0, &ui_ScreenLogin_screen_init);
}
void OnRefreshClicked(lv_event_t * e) {
    net_send_get_room_list(); // 发送请求
}

// 创建房间按钮被点击
void OnCreateRoomClicked(lv_event_t * e) {
    net_send_create_room();   // 发送请求
}
