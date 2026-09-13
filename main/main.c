#include "main.h"

#include "../album/album.h"
#include "../video/video.h"
#include "../game/game.h"
#include "../network/network.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// 主界面图标个数
#define ICON_COUNT 4

// 登录密码
static const char* CORRECT_PASSWORD = "123456";

// 页面对象
static lv_obj_t* page_login = NULL;
static lv_obj_t* page_main = NULL;

// 登录页面控件
static lv_obj_t* ta_password = NULL;

// 主界面背景
static lv_obj_t* main_bg = NULL;

// 四个图标容器
static lv_obj_t* icons[ICON_COUNT] = {NULL, NULL, NULL, NULL};

// ==================== 主界面显示/隐藏 ====================
void app_main_page_hide(void)
{
    for (int i = 0; i < ICON_COUNT; i++)
    {
        lv_obj_add_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(main_bg, LV_OBJ_FLAG_HIDDEN);
}

void app_main_page_show(void)
{
    lv_obj_remove_flag(main_bg, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < ICON_COUNT; i++)
    {
        lv_obj_remove_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
    }
}

// ==================== 界面图标回调 ====================
static void icon_click_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        int index = (int)(intptr_t)lv_event_get_user_data(e);

        switch(index)
        {
            case 0:  // 相册
                album_show();
                break;
            case 1:  // 视频
                video_show();
                break;
            case 2:  // 游戏
                game_show();
                break;
            case 3:  // 网络客户端
                network_show();
                break;
            default:
                break;
        }
    }
}

// ==================== 登录界面事件 ====================
// 清空文本框
static void clear_textarea_timer_cb(lv_timer_t* t)
{
    lv_obj_t* ta = (lv_obj_t*)t->user_data;
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_text(ta, "");
    lv_textarea_set_max_length(ta, 6);
}

static void num_btn_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* btn = lv_event_get_target(e);
    lv_obj_t* ta = lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED)
    {
        lv_obj_t* label = lv_obj_get_child(btn, 0);
        const char* text = lv_label_get_text(label);

        if (text)
        {
            const char* current = lv_textarea_get_text(ta);
            if (strlen(current) < 6)
            {
                lv_textarea_add_text(ta, text);
            }
        }
    }
}

static void clear_btn_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        lv_obj_t* ta = (lv_obj_t*)lv_event_get_user_data(e);
        lv_textarea_set_text(ta, "");
    }
}

static void del_btn_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        lv_obj_t* ta = (lv_obj_t*)lv_event_get_user_data(e);
        char text[64] = {0};
        strcpy(text, lv_textarea_get_text(ta));
        int len = strlen(text);
        if (len > 0)
        {
            text[len - 1] = '\0';
            lv_textarea_set_text(ta, text);
        }
    }
}

static void confirm_btn_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        lv_obj_t* ta = (lv_obj_t*)lv_event_get_user_data(e);
        const char* input = lv_textarea_get_text(ta);

        if (strcmp(input, CORRECT_PASSWORD) == 0)
        {
            printf("Password correct!\n");
            lv_obj_add_flag(page_login, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(page_main, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            printf("Password error!\n");
            lv_textarea_set_password_mode(ta, false);
            lv_textarea_set_max_length(ta, 20);
            lv_textarea_set_text(ta, "Password Error!");
            lv_timer_t* timer = lv_timer_create(clear_textarea_timer_cb, 500, ta);
            lv_timer_set_repeat_count(timer, 1);
        }
    }
}

// ==================== 登录页面 ====================
static void create_login_page(void)
{
    // 创建登录页面
    page_login = lv_obj_create(lv_screen_active());
    lv_obj_set_size(page_login, 1024, 600);
    lv_obj_set_align(page_login, LV_ALIGN_CENTER);

    lv_obj_set_style_border_width(page_login, 0, 0);
    lv_obj_set_style_border_side(page_login, LV_BORDER_SIDE_NONE, 0);
    lv_obj_set_style_pad_all(page_login, 0, 0);
    lv_obj_set_style_bg_opa(page_login, LV_OPA_TRANSP, 0);

    lv_obj_t* image1 = lv_image_create(page_login);
    lv_image_set_src(image1, "A:/WQ/picture/1.bmp");
    lv_obj_set_align(image1, LV_ALIGN_CENTER);
    lv_obj_set_size(image1, 1024, 600);

    // 密码输入框
    ta_password = lv_textarea_create(page_login);
    lv_obj_set_pos(ta_password, 212, 75);
    lv_obj_set_size(ta_password, 600, 100);
    lv_textarea_set_placeholder_text(ta_password, "please enter a 6-digit password");
    lv_textarea_set_password_mode(ta_password, true);
    lv_textarea_set_password_show_time(ta_password, 500);
    lv_textarea_set_max_length(ta_password, 6);

    static lv_style_t style_ta;
    lv_style_init(&style_ta);
    lv_style_set_bg_color(&style_ta, lv_color_hex(0xFFFFFF));
    lv_style_set_bg_opa(&style_ta, LV_OPA_30);
    lv_style_set_border_width(&style_ta, 2);
    lv_style_set_border_color(&style_ta, lv_color_hex(0x000000));
    lv_style_set_radius(&style_ta, 8);
    lv_obj_add_style(ta_password, &style_ta, LV_STATE_DEFAULT);

    // 数字按键 1-9
    int numbers[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    for (int i = 0; i < 9; i++)
    {
        int row = i / 3;
        int col = i % 3;

        lv_obj_t* btn = lv_button_create(page_login);
        lv_obj_set_pos(btn, 362 + col * 100, 200 + row * 100);
        lv_obj_set_size(btn, 100, 100);

        static lv_style_t style_btn;
        lv_style_init(&style_btn);
        lv_style_set_bg_color(&style_btn, lv_color_hex(0xFFFFFF));
        lv_style_set_bg_opa(&style_btn, LV_OPA_30);
        lv_style_set_border_width(&style_btn, 2);
        lv_style_set_border_color(&style_btn, lv_color_hex(0x000000));
        lv_style_set_radius(&style_btn, 8);
        lv_obj_add_style(btn, &style_btn, LV_STATE_DEFAULT);

        lv_obj_t* label = lv_label_create(btn);
        char num_str[2] = {0};
        sprintf(num_str, "%d", numbers[i]);
        lv_label_set_text(label, num_str);
        lv_obj_set_align(label, LV_ALIGN_CENTER);

        static lv_style_t style_label;
        lv_style_init(&style_label);
        lv_style_set_text_font(&style_label, &lv_font_montserrat_30);
        lv_style_set_text_color(&style_label, lv_color_hex(0x000000));
        lv_obj_add_style(label, &style_label, LV_STATE_DEFAULT);

        lv_obj_add_event_cb(btn, num_btn_event_cb, LV_EVENT_CLICKED, ta_password);
    }

    // 按键0
    lv_obj_t* btn0 = lv_button_create(page_login);
    lv_obj_set_pos(btn0, 462, 500);
    lv_obj_set_size(btn0, 100, 100);

    static lv_style_t style_btn0;
    lv_style_init(&style_btn0);
    lv_style_set_bg_color(&style_btn0, lv_color_hex(0xFFFFFF));
    lv_style_set_bg_opa(&style_btn0, LV_OPA_30);
    lv_style_set_border_width(&style_btn0, 2);
    lv_style_set_border_color(&style_btn0, lv_color_hex(0x000000));
    lv_style_set_radius(&style_btn0, 8);
    lv_obj_add_style(btn0, &style_btn0, LV_STATE_DEFAULT);

    lv_obj_t* label0 = lv_label_create(btn0);
    lv_label_set_text(label0, "0");
    lv_obj_set_align(label0, LV_ALIGN_CENTER);

    static lv_style_t style_label0;
    lv_style_init(&style_label0);
    lv_style_set_text_font(&style_label0, &lv_font_montserrat_30);
    lv_style_set_text_color(&style_label0, lv_color_hex(0x000000));
    lv_obj_add_style(label0, &style_label0, LV_STATE_DEFAULT);

    lv_obj_add_event_cb(btn0, num_btn_event_cb, LV_EVENT_CLICKED, ta_password);

    // 清空按钮
    lv_obj_t* clear_btn = lv_button_create(page_login);
    lv_obj_set_pos(clear_btn, 212, 520);
    lv_obj_set_size(clear_btn, 100, 50);

    static lv_style_t style_clear;
    lv_style_init(&style_clear);
    lv_style_set_bg_color(&style_clear, lv_color_hex(0xFFFFFF));
    lv_style_set_bg_opa(&style_clear, LV_OPA_30);
    lv_style_set_radius(&style_clear, 8);
    lv_obj_add_style(clear_btn, &style_clear, LV_STATE_DEFAULT);

    lv_obj_t* clear_label = lv_label_create(clear_btn);
    lv_label_set_text(clear_label, "clear");
    lv_obj_set_align(clear_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(clear_label, lv_color_hex(0x000000), 0);
    lv_obj_add_event_cb(clear_btn, clear_btn_event_cb, LV_EVENT_CLICKED, ta_password);

    // 删除按钮
    lv_obj_t* del_btn = lv_button_create(page_login);
    lv_obj_set_pos(del_btn, 330, 520);
    lv_obj_set_size(del_btn, 100, 50);

    static lv_style_t style_del;
    lv_style_init(&style_del);
    lv_style_set_bg_color(&style_del, lv_color_hex(0xFFFFFF));
    lv_style_set_bg_opa(&style_del, LV_OPA_30);
    lv_style_set_radius(&style_del, 8);
    lv_obj_add_style(del_btn, &style_del, LV_STATE_DEFAULT);

    lv_obj_t* del_label = lv_label_create(del_btn);
    lv_label_set_text(del_label, "delete");
    lv_obj_set_align(del_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(del_label, lv_color_hex(0x000000), 0);
    lv_obj_add_event_cb(del_btn, del_btn_event_cb, LV_EVENT_CLICKED, ta_password);

    // 确认按钮
    lv_obj_t* confirm_btn = lv_button_create(page_login);
    lv_obj_set_pos(confirm_btn, 712, 520);
    lv_obj_set_size(confirm_btn, 100, 50);

    static lv_style_t style_confirm;
    lv_style_init(&style_confirm);
    lv_style_set_bg_color(&style_confirm, lv_color_hex(0xFFFFFF));
    lv_style_set_bg_opa(&style_confirm, LV_OPA_30);
    lv_style_set_radius(&style_confirm, 8);
    lv_obj_add_style(confirm_btn, &style_confirm, LV_STATE_DEFAULT);

    lv_obj_t* confirm_label = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_label, "confirm");
    lv_obj_set_align(confirm_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(confirm_label, lv_color_hex(0x000000), 0);
    lv_obj_add_event_cb(confirm_btn, confirm_btn_event_cb, LV_EVENT_CLICKED, ta_password);

    lv_obj_remove_flag(page_login, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(page_login, LV_SCROLLBAR_MODE_OFF);
}

// ==================== 主页面 ====================
static void create_main_page(void)
{
    page_main = lv_obj_create(lv_screen_active());
    lv_obj_set_size(page_main, 1024, 600);
    lv_obj_set_align(page_main, LV_ALIGN_CENTER);

    lv_obj_set_style_border_width(page_main, 0, 0);
    lv_obj_set_style_border_side(page_main, LV_BORDER_SIDE_NONE, 0);
    lv_obj_set_style_pad_all(page_main, 0, 0);
    lv_obj_set_style_bg_opa(page_main, LV_OPA_TRANSP, 0);

    main_bg = lv_image_create(page_main);
    lv_image_set_src(main_bg, "A:/WQ/picture/2.bmp");
    lv_obj_set_align(main_bg, LV_ALIGN_CENTER);
    lv_obj_set_size(main_bg, 1024, 600);
    lv_obj_move_to_index(main_bg, 0);

    lv_obj_add_flag(page_main, LV_OBJ_FLAG_HIDDEN);

    typedef struct
    {
        const char* img_path;
        const char* label_text;
        int x_pos;
        int y_pos;
    } icon_info_t;

    icon_info_t icon_data[] = 
    {
        {"A:/WQ/picture/album.bmp", "album", 50, 50},
        {"A:/WQ/picture/video.bmp", "video", 300, 50},
        {"A:/WQ/picture/game.bmp", "game", 550, 50},
        {"A:/WQ/picture/transfer.bmp", "transfer", 800, 50}
    };

    for (int i = 0; i < ICON_COUNT; i++)
    {
        icons[i] = lv_obj_create(page_main);
        lv_obj_set_pos(icons[i], icon_data[i].x_pos, icon_data[i].y_pos);
        lv_obj_set_size(icons[i], 160, 200);
        lv_obj_set_style_border_width(icons[i], 0, 0);
        lv_obj_set_style_bg_opa(icons[i], 0, 0);
        lv_obj_set_style_bg_color(icons[i], lv_color_hex(0x000000), LV_STATE_PRESSED);
        lv_obj_add_flag(icons[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(icons[i], icon_click_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* img = lv_image_create(icons[i]);
        lv_image_set_src(img, icon_data[i].img_path);
        lv_obj_set_size(img, 160, 160);
        lv_obj_set_align(img, LV_ALIGN_TOP_MID);

        lv_obj_t* label = lv_label_create(icons[i]);
        lv_label_set_text(label, icon_data[i].label_text);
        lv_obj_set_align(label, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_22, 0);
        lv_obj_set_style_bg_color(label, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(label, 0, 0);
        lv_obj_set_style_pad_all(label, 4, 0);
        lv_obj_set_style_radius(label, 4, 0);

        lv_obj_remove_flag(icons[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(icons[i], LV_SCROLLBAR_MODE_OFF);
    }

    lv_obj_remove_flag(page_main, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(page_main, LV_SCROLLBAR_MODE_OFF);

    // 创建四个功能模块的界面
    album_create(page_main);
    video_create(page_main);
    game_create(page_main);
    network_create(page_main);
}

// ==================== 界面总装 ====================
void myinterface(void)
{
    create_login_page();
    create_main_page();
}

// ==================== 主函数 ====================
int main(void)
{
    lv_init();

    lv_display_t * disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, "/dev/fb0");

    lv_indev_t * indev = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event6");

    myinterface();

    while(1) 
    {
        lv_timer_handler();
        usleep(5000);
    }

    return 0;
}
