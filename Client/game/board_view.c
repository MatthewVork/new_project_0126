#include "board_view.h"
#include <stdio.h>

// 画子
void board_view_draw_stone(lv_obj_t *parent, int x, int y, int color) {
    if (!parent) return;

    lv_obj_t * stone = lv_obj_create(parent);
    lv_obj_set_size(stone, STONE_SIZE, STONE_SIZE);
    lv_obj_set_style_radius(stone, LV_RADIUS_CIRCLE, 0); 
    lv_obj_set_style_border_width(stone, 0, 0);          
    
    if (color == COLOR_BLACK) {
        lv_obj_set_style_bg_color(stone, lv_color_hex(0x000000), 0);
    } else {
        lv_obj_set_style_bg_color(stone, lv_color_hex(0xFFFFFF), 0);
    }

    // 计算坐标
    int pos_x = BOARD_PADDING + x * CELL_SIZE - STONE_SIZE / 2;
    int pos_y = BOARD_PADDING + y * CELL_SIZE - STONE_SIZE / 2;
    
    lv_obj_set_pos(stone, pos_x, pos_y);
    lv_obj_clear_flag(stone, LV_OBJ_FLAG_SCROLLABLE); 
}

// 清盘
void board_view_clear(lv_obj_t *parent) {
    if (!parent) return;
    lv_obj_clean(parent);
}

// 算坐标
int board_view_get_click_xy(lv_obj_t *board_obj, int *out_x, int *out_y) {
    if (!board_obj) return 0;

    lv_point_t p;
    lv_indev_get_point(lv_indev_get_act(), &p);

    lv_area_t coords;
    lv_obj_get_coords(board_obj, &coords);

    int rel_x = p.x - coords.x1;
    int rel_y = p.y - coords.y1;

    int gx = (rel_x - BOARD_PADDING + CELL_SIZE / 2) / CELL_SIZE;
    int gy = (rel_y - BOARD_PADDING + CELL_SIZE / 2) / CELL_SIZE;

    if (gx >= 0 && gx < BOARD_SIZE && gy >= 0 && gy < BOARD_SIZE) {
        *out_x = gx;
        *out_y = gy;
        return 1; 
    }
    return 0; 
}