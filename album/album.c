#include "album.h"
#include "../main/main.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

// 相册图片
static const char* album_images[] = 
{
    "A:/WQ/picture/A.bmp",
    "A:/WQ/picture/B.bmp",
    "A:/WQ/picture/C.bmp",
    "A:/WQ/picture/D.bmp"
};
static const int album_count = 4;
static int current_image_index = 0;
static int is_album_mode = 0;

// 相册子控件
static lv_obj_t* album_grid_container = NULL;
static lv_obj_t* album_view_image = NULL;
static lv_obj_t* album_page_label = NULL;
static lv_obj_t* album_left_btn = NULL;
static lv_obj_t* album_right_btn = NULL;
static lv_obj_t* album_return_btn = NULL;
static lv_obj_t* album_bg = NULL;

// ==================== 相册显示逻辑 ====================
static void update_album_view(void)
{
    lv_image_set_src(album_view_image, album_images[current_image_index]);

    char page_text[16] = {0};
    sprintf(page_text, "%d/%d", current_image_index + 1, album_count);
    lv_label_set_text(album_page_label, page_text);
}

static void album_prev_image(void)
{
    if (current_image_index == 0) 
    {
        current_image_index = album_count - 1;
    } 
    else 
    {
        current_image_index--;
    }
    update_album_view();
}

static void album_next_image(void)
{
    if (current_image_index == album_count - 1) 
    {
        current_image_index = 0;
    } 
    else 
    {
        current_image_index++;
    }
    update_album_view();
}

// ==================== 事件回调 ====================
static void album_thumb_click_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        int index = (int)(intptr_t)lv_event_get_user_data(e);
        current_image_index = index;
        is_album_mode = 1;

        // 隐藏网格容器
        lv_obj_add_flag(album_grid_container, LV_OBJ_FLAG_HIDDEN);

        // 显示查看模式部件
        lv_obj_remove_flag(album_view_image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(album_page_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(album_left_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(album_right_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(album_return_btn, LV_OBJ_FLAG_HIDDEN);

        update_album_view();
    }
}

static void album_nav_btn_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        int direction = (int)(intptr_t)lv_event_get_user_data(e);
        if (direction == 0) 
        {
            album_prev_image();
        } 
        else 
        {
            album_next_image();
        }
    }
}

static void album_return_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        if (is_album_mode == 1)
        {
            // 查看模式返回网格模式
            is_album_mode = 0;

            // 显示网格容器
            lv_obj_remove_flag(album_grid_container, LV_OBJ_FLAG_HIDDEN);

            // 隐藏查看模式部件
            lv_obj_add_flag(album_view_image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(album_page_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(album_left_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(album_right_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(album_return_btn, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            // 网格模式返回主界面
            // 隐藏相册所有内容
            lv_obj_add_flag(album_grid_container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(album_view_image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(album_page_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(album_left_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(album_right_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(album_return_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(album_bg, LV_OBJ_FLAG_HIDDEN);

            // 恢复主界面
            app_main_page_show();
        }
    }
}

// ==================== 对外接口 ====================
void album_show(void)
{
    // 隐藏主界面(背景 + 四个图标)
    app_main_page_hide();

    // 显示背景
    lv_obj_remove_flag(album_bg, LV_OBJ_FLAG_HIDDEN);
    // 显示返回按钮（网格模式）
    lv_obj_remove_flag(album_return_btn, LV_OBJ_FLAG_HIDDEN);
    // 显示网格容器
    lv_obj_remove_flag(album_grid_container, LV_OBJ_FLAG_HIDDEN);
    // 隐藏查看模式的部件
    lv_obj_add_flag(album_view_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(album_page_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(album_left_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(album_right_btn, LV_OBJ_FLAG_HIDDEN);

    is_album_mode = 0;
}

void album_create(lv_obj_t* parent)
{
    album_bg = lv_image_create(parent);
    lv_image_set_src(album_bg, "A:/WQ/picture/background.bmp");
    lv_obj_set_align(album_bg, LV_ALIGN_CENTER);
    lv_obj_set_size(album_bg, 1024, 600);
    lv_obj_move_to_index(album_bg, 0);
    lv_obj_add_flag(album_bg, LV_OBJ_FLAG_HIDDEN);

    // 相册网格容器
    album_grid_container = lv_obj_create(parent);
    lv_obj_set_pos(album_grid_container, 30, 40);
    lv_obj_set_size(album_grid_container, 1000, 500);
    lv_obj_set_style_bg_opa(album_grid_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(album_grid_container, 0, 0);
    lv_obj_set_style_pad_all(album_grid_container, 10, 0);
    lv_obj_add_flag(album_grid_container, LV_OBJ_FLAG_HIDDEN);

    //清除滚动条
    lv_obj_remove_flag(album_grid_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(album_grid_container, LV_SCROLLBAR_MODE_OFF);

    int cols = 3;
    int spacing = 20;
    int container_width = 1024;
    int container_height = 600;

    int img_width = (container_width - spacing * 3 - 80) / cols;
    int img_height = (container_height - spacing * 2 - 80) / 2;

    if (img_height > img_width * 3 / 4)
    {
        img_height = img_width * 3 / 4;
    }

    for (int i = 0; i < album_count; i++)
    {
        int row = i / cols;
        int col = i % cols;

        lv_obj_t* container = lv_obj_create(album_grid_container);
        int x = 10 + col * (img_width + spacing);
        int y = 10 + row * (img_height + spacing + 40);
        lv_obj_set_pos(container, x, y);
        lv_obj_set_size(container, img_width, img_height + 40);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_bg_opa(container, 0, 0);

        //禁用滑动,清除滚动条
        lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

        lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(container, album_thumb_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* img = lv_image_create(container);
        lv_image_set_src(img, album_images[i]);
        lv_obj_set_pos(img, 0, 0);
        lv_obj_set_size(img, img_width, img_height);
        lv_obj_set_align(img, LV_ALIGN_CENTER);
    }

    album_view_image = lv_image_create(parent);
    lv_obj_set_pos(album_view_image, 0, 0);
    lv_obj_set_size(album_view_image, 1024, 600);
    lv_obj_add_flag(album_view_image, LV_OBJ_FLAG_HIDDEN);

    album_page_label = lv_label_create(parent);
    lv_obj_set_align(album_page_label, LV_ALIGN_TOP_MID);
    lv_obj_set_style_text_color(album_page_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(album_page_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_bg_color(album_page_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(album_page_label, 0, 0);
    lv_obj_set_style_pad_all(album_page_label, 8, 0);
    lv_obj_set_style_radius(album_page_label, 6, 0);
    lv_obj_add_flag(album_page_label, LV_OBJ_FLAG_HIDDEN);

    album_return_btn = lv_button_create(parent);
    lv_obj_set_pos(album_return_btn, 0, 0);
    lv_obj_set_size(album_return_btn, 40, 40);
    lv_obj_set_style_border_width(album_return_btn, 0, 0);
    lv_obj_add_flag(album_return_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(album_return_btn, album_return_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(album_return_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* return_img = lv_image_create(album_return_btn);
    lv_image_set_src(return_img, "A:/WQ/picture/return.bmp");
    lv_obj_set_align(return_img, LV_ALIGN_CENTER);
    lv_obj_set_size(return_img, 40, 40);

    album_left_btn = lv_button_create(parent);
    lv_obj_set_align(album_left_btn, LV_ALIGN_LEFT_MID);
    lv_obj_set_size(album_left_btn, 30, 60);
    lv_obj_set_style_bg_opa(album_left_btn, 0, 0);
    lv_obj_set_style_border_width(album_left_btn, 0, 0);
    lv_obj_add_flag(album_left_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(album_left_btn, album_nav_btn_cb, LV_EVENT_CLICKED, (void*)0);
    lv_obj_add_flag(album_left_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* left_img = lv_image_create(album_left_btn);
    lv_image_set_src(left_img, "A:/WQ/picture/last.bmp");
    lv_obj_set_align(left_img, LV_ALIGN_CENTER);
    lv_obj_set_size(left_img, 30, 60);

    album_right_btn = lv_button_create(parent);
    lv_obj_set_align(album_right_btn, LV_ALIGN_RIGHT_MID);
    lv_obj_set_size(album_right_btn, 30, 60);
    lv_obj_set_style_bg_opa(album_right_btn, 0, 0);
    lv_obj_set_style_border_width(album_right_btn, 0, 0);
    lv_obj_add_flag(album_right_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(album_right_btn, album_nav_btn_cb, LV_EVENT_CLICKED, (void*)1);
    lv_obj_add_flag(album_right_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* right_img = lv_image_create(album_right_btn);
    lv_image_set_src(right_img, "A:/WQ/picture/next.bmp");
    lv_obj_set_align(right_img, LV_ALIGN_CENTER);
    lv_obj_set_size(right_img, 60, 60);
}
