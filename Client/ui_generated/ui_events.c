// Client/ui_generated/ui_events.c
#include "ui.h"
#include <stdio.h>
#include <string.h>
#include "../network/network_client.h"

// 登录按钮逻辑
void OnLoginClicked(lv_event_t * e)
{
    const char* username = lv_textarea_get_text(ui_InputUser);
    const char* password = lv_textarea_get_text(ui_InputPass);

    if (strlen(username) == 0 || strlen(password) == 0) {
        printf("[Login Error] Input cannot be empty\n");
        return;
    }

    printf("[UI] Login Request: %s\n", username);
    net_send_login(username, password);
}

// 注册按钮逻辑 (已修复)
void OnRegisterClicked(lv_event_t * e)
{
    // 1. 获取注册页面的三个输入框 (注意变量名变化)
    const char* username = lv_textarea_get_text(ui_RegUser);
    const char* password = lv_textarea_get_text(ui_RegPass);
    const char* confirm  = lv_textarea_get_text(ui_RegPassConfirm);

    // 2. 基础非空校验
    if (strlen(username) == 0 || strlen(password) == 0) {
        printf("[Register Error] Username or Password empty\n");
        return;
    }

    // 3. 校验两次密码是否一致 (这是注册页的核心价值)
    if (strcmp(password, confirm) != 0) {
        printf("[Register Error] Passwords do not match!\n");
        // 如果想高级点，可以用 lv_label_set_text 修改界面上的提示文字
        return;
    }

    // 4. 发送注册请求
    printf("[UI] Register Request: %s\n", username);
    net_send_register(username, password);
}