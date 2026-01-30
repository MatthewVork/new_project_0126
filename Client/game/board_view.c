#include "board_view.h"
#include <stdio.h>
#include <math.h> // 数学库，用于精确转换

// 1. 修正后的落子绘图函数
void board_view_draw_stone(lv_obj_t *parent, int x, int y, int color) {
    if (!parent) return;

    lv_obj_t * stone = lv_obj_create(parent);
    lv_obj_set_size(stone, STONE_SIZE, STONE_SIZE);
    lv_obj_set_style_radius(stone, LV_RADIUS_CIRCLE, 0); 
    lv_obj_set_style_border_width(stone, 0, 0);          
    
    // 设置颜色
    if (color == COLOR_BLACK) {
        lv_obj_set_style_bg_color(stone, lv_color_hex(0x000000), 0);
    } else {
        lv_obj_set_style_bg_color(stone, lv_color_hex(0xFFFFFF), 0);
    }

    // ★★★ 核心修正：浮点数计算中心坐标 ★★★
    // 逻辑：偏移量 + (格子索引 * 间距) - 半径修正
    float fx = BOARD_PADDING + (float)x * CELL_SIZE - (float)STONE_SIZE / 2.0f;
    float fy = BOARD_PADDING + (float)y * CELL_SIZE - (float)STONE_SIZE / 2.0f;
    
    // 四舍五入转为整数 (+0.5f 技巧)
    lv_obj_set_pos(stone, (int)(fx + 0.5f), (int)(fy + 0.5f));
    lv_obj_clear_flag(stone, LV_OBJ_FLAG_SCROLLABLE); 
}

// 清盘函数
void board_view_clear(lv_obj_t *parent) {
    if (!parent) return;
    lv_obj_clean(parent);
}

// 2. 修正后的点击检测函数 (动态获取坐标)
bool board_view_get_click_xy(lv_obj_t * obj, int * x_out, int * y_out) {
    lv_indev_t * indev = lv_indev_get_act();
    lv_point_t point;
    lv_indev_get_point(indev, &point); // 获取点击的绝对坐标

    // 获取容器的绝对坐标区域
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    // 计算相对坐标 (点击点 - 容器左上角)
    int local_x = point.x - coords.x1;
    int local_y = point.y - coords.y1;

    // 反推网格索引
    float fx = ((float)local_x - BOARD_PADDING) / CELL_SIZE;
    float fy = ((float)local_y - BOARD_PADDING) / CELL_SIZE;

    // 四舍五入取最近的交叉点
    int gx = (int)(fx + 0.5f);
    int gy = (int)(fy + 0.5f);

    // 越界检查
    if (gx < 0 || gx >= BOARD_SIZE || gy < 0 || gy >= BOARD_SIZE) {
        return false;
    }

    *x_out = gx;
    *y_out = gy;
    return true;
}