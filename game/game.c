#include "game.h"
#include "../main/main.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// 井字棋游戏相关
static lv_obj_t* game_page = NULL;
static lv_obj_t* game_bg = NULL;
static lv_obj_t* game_return_btn = NULL;
static lv_obj_t* game_continue_btn = NULL;
static lv_obj_t* game_cells[3][3] = {NULL};
static int game_board[3][3] = {0};   // 0=空, 1=玩家1(圆点), 2=玩家2(叉号)
static int game_current_player = 1;  // 1=玩家1(圆点), 2=玩家2(叉号)
static int game_over = 0;
static lv_obj_t* game_result_label = NULL;

// ==================== 游戏逻辑 ====================
static void game_init_board(void)
{
    for (int i = 0; i < 3; i++) 
    {
        for (int j = 0; j < 3; j++) 
        {
            game_board[i][j] = 0;
        }
    }
    game_current_player = 1;
    game_over = 0;
}

static int game_check_winner(void)
{
    // 检查行
    for (int i = 0; i < 3; i++) 
    {
        if (game_board[i][0] != 0 && game_board[i][0] == game_board[i][1] && game_board[i][1] == game_board[i][2]) 
        {
            return game_board[i][0];
        }
    }
    // 检查列
    for (int j = 0; j < 3; j++) 
    {
        if (game_board[0][j] != 0 && game_board[0][j] == game_board[1][j] && game_board[1][j] == game_board[2][j]) 
        {
            return game_board[0][j];
        }
    }
    // 检查对角线
    if (game_board[0][0] != 0 && game_board[0][0] == game_board[1][1] && game_board[1][1] == game_board[2][2]) 
    {
        return game_board[0][0];
    }
    if (game_board[0][2] != 0 && game_board[0][2] == game_board[1][1] && game_board[1][1] == game_board[2][0]) 
    {
        return game_board[0][2];
    }
    return 0;
}

static int game_is_full(void)
{
    for (int i = 0; i < 3; i++) 
    {
        for (int j = 0; j < 3; j++) 
        {
            if (game_board[i][j] == 0) return 0;
        }
    }
    return 1;
}

// 更新游戏界面
static void game_update_ui(void)
{
    for (int i = 0; i < 3; i++) 
    {
        for (int j = 0; j < 3; j++) 
        {
            lv_obj_t* cell = game_cells[i][j];
            // 清除子控件
            lv_obj_clean(cell);

            if (game_board[i][j] == 1)
            {
                // 玩家1：黑圆点
                lv_obj_t* dot = lv_obj_create(cell);
                lv_obj_set_size(dot, 60, 60);
                lv_obj_set_align(dot, LV_ALIGN_CENTER);
                lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
                lv_obj_set_style_bg_color(dot, lv_color_hex(0x000000), 0);
                lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(dot, 0, 0);
            }
            else if (game_board[i][j] == 2)
            {
                // 玩家2： X
                lv_obj_t* label = lv_label_create(cell);
                lv_label_set_text(label, "X");
                lv_obj_set_align(label, LV_ALIGN_CENTER);
                lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
                lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
            }
        }
    }

    // 更新继续按钮文本
    if (!game_over) 
    {
        char btn_text[32] = {0};
        sprintf(btn_text, "Player %d", game_current_player);
        lv_label_set_text(lv_obj_get_child(game_continue_btn, 0), btn_text);
        lv_obj_remove_flag(game_continue_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

// 显示所有单元格并清空内容
static void game_clear_cells(void)
{
    for (int i = 0; i < 3; i++) 
    {
        for (int j = 0; j < 3; j++) 
        {
            lv_obj_remove_flag(game_cells[i][j], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clean(game_cells[i][j]);
        }
    }
}

// 隐藏网格与继续按钮
static void game_hide_cells(void)
{
    for (int i = 0; i < 3; i++) 
    {
        for (int j = 0; j < 3; j++) 
        {
            lv_obj_add_flag(game_cells[i][j], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_add_flag(game_continue_btn, LV_OBJ_FLAG_HIDDEN);
}

// ==================== 事件回调 ====================
// 自动重启游戏回调
static void game_auto_restart_cb(lv_timer_t* timer)
{
    // 重新初始化游戏
    game_init_board();

    // 显示所有单元格并清空内容
    game_clear_cells();

    // 隐藏结果标签
    lv_obj_add_flag(game_result_label, LV_OBJ_FLAG_HIDDEN);

    // 显示继续按钮
    lv_obj_remove_flag(game_continue_btn, LV_OBJ_FLAG_HIDDEN);

    // 更新界面
    game_update_ui();
}

// 游戏单元格点击回调
static void game_cell_click_cb(lv_event_t* e)
{
    if (game_over) return;

    int* pos = (int*)lv_event_get_user_data(e);
    int row = pos[0];
    int col = pos[1];

    // 位置已有棋子
    if (game_board[row][col] != 0) return;

    // 下棋
    game_board[row][col] = game_current_player;
    game_update_ui();

    // 检查胜利
    int winner = game_check_winner();
    if (winner != 0) 
    {
        game_over = 1;
        const char* player_name = (winner == 1) ? "Player 1" : "Player 2";
        char result_text[64] = {0};
        sprintf(result_text, "%s Wins!", player_name);

        // 清屏显示胜利信息
        lv_label_set_text(game_result_label, result_text);
        lv_obj_remove_flag(game_result_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(game_result_label);

        // 隐藏网格和继续按钮
        game_hide_cells();

        // 自动重置游戏
        lv_timer_t* timer = lv_timer_create(game_auto_restart_cb, 1000, NULL);
        lv_timer_set_repeat_count(timer, 1);
        return;
    }

    // 检查平局
    if (game_is_full()) 
    {
        game_over = 1;
        lv_label_set_text(game_result_label, "Draw!");
        lv_obj_remove_flag(game_result_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(game_result_label);

        game_hide_cells();

        // 1秒后自动重置游戏
        lv_timer_t* timer = lv_timer_create(game_auto_restart_cb, 1500, NULL);
        lv_timer_set_repeat_count(timer, 1);
        return;
    }
}

// 继续按钮回调
static void game_continue_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        if (game_over) return;

        // 切换玩家
        game_current_player = (game_current_player == 1) ? 2 : 1;
        game_update_ui();
    }
}

// 游戏返回回调
static void game_return_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        lv_obj_add_flag(game_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(game_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(game_return_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(game_continue_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(game_result_label, LV_OBJ_FLAG_HIDDEN);

        // 恢复主界面
        app_main_page_show();
    }
}

// ==================== 对外接口 ====================
void game_show(void)
{
    // 隐藏主界面(背景 + 四个图标)
    app_main_page_hide();

    // 显示游戏页面
    lv_obj_remove_flag(game_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(game_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(game_return_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(game_continue_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(game_result_label, LV_OBJ_FLAG_HIDDEN);

    // 初始化游戏
    game_init_board();
    // 显示所有单元格
    game_clear_cells();
    game_update_ui();
}

void game_create(lv_obj_t* parent)
{
    // 游戏页面
    game_page = lv_obj_create(parent);
    lv_obj_set_size(game_page, 1024, 600);
    lv_obj_set_align(game_page, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(game_page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(game_page, 0, 0);
    lv_obj_add_flag(game_page, LV_OBJ_FLAG_HIDDEN);

    lv_obj_remove_flag(game_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(game_page, LV_SCROLLBAR_MODE_OFF);

    // 游戏背景
    game_bg = lv_obj_create(game_page);
    lv_obj_set_size(game_bg, 1024, 600);
    lv_obj_set_align(game_bg, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(game_bg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(game_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(game_bg, 0, 0);
    lv_obj_move_to_index(game_bg, 0);

    // 返回按钮
    game_return_btn = lv_button_create(parent);
    lv_obj_set_pos(game_return_btn, 0, 0);
    lv_obj_set_size(game_return_btn, 40, 40);
    lv_obj_set_style_border_width(game_return_btn, 0, 0);
    lv_obj_add_flag(game_return_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(game_return_btn, game_return_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(game_return_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* return_img = lv_image_create(game_return_btn);
    lv_image_set_src(return_img, "A:/WQ/picture/return.bmp");
    lv_obj_set_align(return_img, LV_ALIGN_CENTER);
    lv_obj_set_size(return_img, 40, 40);

    // 继续按钮
    game_continue_btn = lv_button_create(game_page);
    lv_obj_set_align(game_continue_btn, LV_ALIGN_RIGHT_MID);
    lv_obj_set_pos(game_continue_btn, -30, 0);
    lv_obj_set_size(game_continue_btn, 180, 50);

    static lv_style_t style_continue;
    lv_style_init(&style_continue);
    lv_style_set_bg_color(&style_continue, lv_color_hex(0xFFFFFF));
    lv_style_set_bg_opa(&style_continue, LV_OPA_30);
    lv_style_set_border_width(&style_continue, 2);
    lv_style_set_border_color(&style_continue, lv_color_hex(0x000000));
    lv_style_set_radius(&style_continue, 8);
    lv_obj_add_style(game_continue_btn, &style_continue, LV_STATE_DEFAULT);

    lv_obj_t* continue_label = lv_label_create(game_continue_btn);
    lv_label_set_text(continue_label, "continue: Player 1");
    lv_obj_set_align(continue_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(continue_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(continue_label, &lv_font_montserrat_18, 0);
    lv_obj_add_event_cb(game_continue_btn, game_continue_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(game_continue_btn, LV_OBJ_FLAG_HIDDEN);

    // 九宫格容器
    lv_obj_t* grid_container = lv_obj_create(game_page);
    lv_obj_set_size(grid_container, 300, 300);
    lv_obj_set_align(grid_container, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(grid_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid_container, 0, 0);

    lv_obj_remove_flag(grid_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(grid_container, LV_SCROLLBAR_MODE_OFF);

    // 创建9个单元格
    enum _lv_align_t locate[9] = {
        LV_ALIGN_TOP_LEFT, LV_ALIGN_TOP_MID, LV_ALIGN_TOP_RIGHT,
        LV_ALIGN_LEFT_MID, LV_ALIGN_CENTER, LV_ALIGN_RIGHT_MID,
        LV_ALIGN_BOTTOM_LEFT, LV_ALIGN_BOTTOM_MID, LV_ALIGN_BOTTOM_RIGHT
    };

    for (int i = 0; i < 3; i++) 
    {
        for (int j = 0; j < 3; j++) 
        {
            int idx = i * 3 + j;
            game_cells[i][j] = lv_obj_create(grid_container);
            lv_obj_set_size(game_cells[i][j], 100, 100);
            lv_obj_set_align(game_cells[i][j], locate[idx]);
            lv_obj_set_style_bg_color(game_cells[i][j], lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_bg_opa(game_cells[i][j], LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(game_cells[i][j], 2, 0);
            lv_obj_set_style_border_color(game_cells[i][j], lv_color_hex(0x000000), 0);
            lv_obj_set_style_radius(game_cells[i][j], 0, 0);
            lv_obj_add_flag(game_cells[i][j], LV_OBJ_FLAG_CLICKABLE);

            int* pos = malloc(sizeof(int) * 2);
            pos[0] = i;
            pos[1] = j;
            lv_obj_add_event_cb(game_cells[i][j], game_cell_click_cb, LV_EVENT_CLICKED, pos);

            lv_obj_remove_flag(game_cells[i][j], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_scrollbar_mode(game_cells[i][j], LV_SCROLLBAR_MODE_OFF);
        }
    }

    // 结果标签（初始隐藏）
    game_result_label = lv_label_create(game_page);
    lv_obj_set_align(game_result_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(game_result_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(game_result_label, &lv_font_montserrat_30, 0);
    lv_obj_set_style_bg_color(game_result_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(game_result_label, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(game_result_label, 20, 0);
    lv_obj_set_style_radius(game_result_label, 10, 0);
    lv_obj_add_flag(game_result_label, LV_OBJ_FLAG_HIDDEN);
}
