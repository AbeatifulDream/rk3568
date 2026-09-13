#include "video.h"
#include "../main/main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

// mplayer 控制管道
#define VIDEO_FIFO_PATH  "/WQ/video/1.fifo"
// mplayer 播放命令行
#define MPLAYER_CMD      "mplayer64 -vo fbdev2 -slave -quiet -input file=" VIDEO_FIFO_PATH \
                         " -geometry 60:60 -zoom -x 900 -y 500 %s"

// 视频封面/视频文件
static const char* video_cover_images[] = 
{
    "A:/WQ/picture/video1.bmp",
    "A:/WQ/picture/video2.bmp",
    "A:/WQ/picture/video3.bmp"
};
static const char* video_paths[] = 
{
    "A:/WQ/video/1.mp4",
    "A:/WQ/video/2.mp4",
    "A:/WQ/video/3.mp4"
};
static const int video_count = 3;
static int current_video_index = 0;
static int is_video_mode = 0;

// 视频子控件
static lv_obj_t* video_grid_container = NULL;
static lv_obj_t* video_view_image = NULL;
static lv_obj_t* video_pause_btn = NULL;
static lv_obj_t* video_left_btn = NULL;
static lv_obj_t* video_right_btn = NULL;
static lv_obj_t* video_return_btn = NULL;
static lv_obj_t* video_bg = NULL;

// 视频控制相关
static pid_t mplayer_pid = -1;
static int mplayer_fd = -1;
static int mplayer_playing = 0;  // 0=停止, 1=播放中, 2=暂停

// ==================== mplayer 控制 ====================
// 创建 mplayer 控制管道
static void video_fifo_init(void)
{
    if (mkfifo(VIDEO_FIFO_PATH, 0664) == 0)
    {
        printf("FIFO created: %s\n", VIDEO_FIFO_PATH);
    }
}

// 启动mplayer播放视频
static void start_mplayer(const char* video_path)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        // 子进程：执行mplayer
        char cmd[256] = {0};
        const char* actual_path = video_path;
        if (strncmp(video_path, "A:", 2) == 0)
        {
            actual_path = video_path + 2;
        }

        snprintf(cmd, sizeof(cmd), MPLAYER_CMD, actual_path);

        printf("Playing: %s\n", cmd);
        system(cmd);
        exit(0);
    }
    else if (pid > 0)
    {
        mplayer_pid = pid;
        mplayer_playing = 1;
        printf("mplayer started, PID: %d, video: %s\n", mplayer_pid, video_path);
        usleep(200000);
        mplayer_fd = open(VIDEO_FIFO_PATH, O_WRONLY);
        if (mplayer_fd < 0)
        {
            perror("open fifo error");
        }
        else
        {
            printf("mplayer fifo opened\n");
        }
    }
}

// 停止mplayer
static void stop_mplayer(void)
{
    if (mplayer_fd > 0)
    {
        const char* cmd = "quit\n";
        write(mplayer_fd, cmd, strlen(cmd));
        close(mplayer_fd);
        mplayer_fd = -1;
    }

    if (mplayer_pid > 0)
    {
        system("killall -9 mplayer64");
        wait(NULL);
        mplayer_pid = -1;
        mplayer_playing = 0;
        printf("mplayer stopped\n");
    }
}

// 发送命令到mplayer (暂停/继续)
static void send_mplayer_command(const char* cmd)
{
    if (mplayer_fd < 0) 
    {
        mplayer_fd = open(VIDEO_FIFO_PATH, O_WRONLY);
        if (mplayer_fd < 0) 
        {
            printf("Failed to open fifo\n");
            return;
        }
    }

    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "%s\n", cmd);
    write(mplayer_fd, buf, strlen(buf));
}

// ==================== 视频显示逻辑 ====================
static void update_video_view(void)
{
    // 显示视频封面
    lv_image_set_src(video_view_image, video_cover_images[current_video_index]);
}

static void video_prev(void)
{
    // 停止当前播放
    stop_mplayer();

    if (current_video_index == 0) 
    {
        current_video_index = video_count - 1;
    } 
    else 
    {
        current_video_index--;
    }
    update_video_view();
    // 播放新视频
    start_mplayer(video_paths[current_video_index]);
}

static void video_next(void)
{
    // 停止当前播放
    stop_mplayer();

    if (current_video_index == video_count - 1) 
    {
        current_video_index = 0;
    } 
    else 
    {
        current_video_index++;
    }
    update_video_view();
    // 播放新视频
    start_mplayer(video_paths[current_video_index]);
}

// ==================== 事件回调 ====================
static void video_thumb_click_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        int index = (int)(intptr_t)lv_event_get_user_data(e);
        current_video_index = index;
        is_video_mode = 1;

        lv_obj_add_flag(video_grid_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(video_view_image, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(video_left_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(video_right_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(video_pause_btn, LV_OBJ_FLAG_HIDDEN);

        update_video_view();
        // 启动mplayer播放视频
        start_mplayer(video_paths[current_video_index]);
    }
}

static void video_nav_btn_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        int direction = (int)(intptr_t)lv_event_get_user_data(e);
        if (direction == 0)
        {
            video_prev();
        }
        else
        {
            video_next();
        }
    }
}

static void video_return_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED)
    {
        // 停止播放
        stop_mplayer();

        if (is_video_mode == 1) 
        {
            // 返回视频网格
            is_video_mode = 0;
            lv_obj_remove_flag(video_grid_container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(video_view_image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(video_left_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(video_right_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(video_pause_btn, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            // 返回主界面
            lv_obj_add_flag(video_grid_container, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(video_view_image, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(video_left_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(video_right_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(video_pause_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(video_return_btn, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(video_bg, LV_OBJ_FLAG_HIDDEN);

            // 恢复主界面
            app_main_page_show();
        }
    }
}

static void video_pause_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        send_mplayer_command("pause");
        // 切换播放状态显示
        if (mplayer_playing == 1)
        {
            mplayer_playing = 2;  // 暂停
            printf("Video paused\n");
        } 
        else if (mplayer_playing == 2)
        {
            mplayer_playing = 1;  // 恢复
            printf("Video resumed\n");
        }
    }
}

// ==================== 对外接口 ====================
void video_show(void)
{
    // 隐藏主界面(背景 + 四个图标)
    app_main_page_hide();

    // 显示背景与控件
    lv_obj_remove_flag(video_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(video_return_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(video_grid_container, LV_OBJ_FLAG_HIDDEN);
    // 隐藏查看模式的部件
    lv_obj_add_flag(video_view_image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(video_left_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(video_right_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(video_pause_btn, LV_OBJ_FLAG_HIDDEN);

    is_video_mode = 0;
}

void video_create(lv_obj_t* parent)
{
    // 创建 mplayer 控制管道
    video_fifo_init();

    video_bg = lv_image_create(parent);
    lv_image_set_src(video_bg, "A:/WQ/picture/background.bmp");
    lv_obj_set_align(video_bg, LV_ALIGN_CENTER);
    lv_obj_set_size(video_bg, 1024, 600);
    lv_obj_move_to_index(video_bg, 0);
    lv_obj_add_flag(video_bg, LV_OBJ_FLAG_HIDDEN);

    // 返回按钮
    video_return_btn = lv_button_create(parent);
    lv_obj_set_pos(video_return_btn, 0, 0);
    lv_obj_set_size(video_return_btn, 40, 40);
    lv_obj_set_style_border_width(video_return_btn, 0, 0);
    lv_obj_add_flag(video_return_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(video_return_btn, video_return_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(video_return_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* return_img = lv_image_create(video_return_btn);
    lv_image_set_src(return_img, "A:/WQ/picture/return.bmp");
    lv_obj_set_align(return_img, LV_ALIGN_CENTER);
    lv_obj_set_size(return_img, 40, 40);

    // 视频网格容器
    video_grid_container = lv_obj_create(parent);
    lv_obj_set_pos(video_grid_container, 30, 40);
    lv_obj_set_size(video_grid_container, 1000, 500);
    lv_obj_set_style_bg_opa(video_grid_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(video_grid_container, 0, 0);
    lv_obj_set_style_pad_all(video_grid_container, 10, 0);
    lv_obj_add_flag(video_grid_container, LV_OBJ_FLAG_HIDDEN);

    lv_obj_remove_flag(video_grid_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(video_grid_container, LV_SCROLLBAR_MODE_OFF);

    int cols = 2;
    int spacing = 20;
    int container_width = 1024;
    int container_height = 540;

    int img_width = (container_width - spacing * 3 - 80) / cols;
    int img_height = (container_height - spacing * 2 - 80) / 2;

    if (img_height > img_width * 3 / 4)
    {
        img_height = img_width * 3 / 4;
    }

    for (int i = 0; i < video_count; i++)
    {
        int row = i / cols;
        int col = i % cols;

        lv_obj_t* container = lv_obj_create(video_grid_container);
        int x = 10 + col * (img_width + spacing);
        int y = 10 + row * (img_height + spacing + 40);
        lv_obj_set_pos(container, x, y);
        lv_obj_set_size(container, img_width, img_height + 40);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_bg_opa(container, 0, 0);
        // 绑定事件
        lv_obj_add_flag(container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(container, video_thumb_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        // 禁用滑动,清除滚动条
        lv_obj_remove_flag(container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(container, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t* img = lv_image_create(container);
        lv_image_set_src(img, video_cover_images[i]);
        lv_obj_set_pos(img, 0, 0);
        lv_obj_set_size(img, img_width, img_height);
        lv_obj_set_align(img, LV_ALIGN_CENTER);
    }

    // 查看模式：显示视频封面
    video_view_image = lv_image_create(parent);
    lv_obj_set_pos(video_view_image, 0, 0);
    lv_obj_set_size(video_view_image, 1024, 600);
    lv_obj_add_flag(video_view_image, LV_OBJ_FLAG_HIDDEN);

    // 暂停按键
    video_pause_btn = lv_button_create(parent);
    lv_obj_set_align(video_pause_btn, LV_ALIGN_TOP_MID);
    lv_obj_set_size(video_pause_btn, 60, 30);
    lv_obj_set_style_bg_opa(video_pause_btn, 0, 0);
    lv_obj_set_style_border_width(video_pause_btn, 0, 0);
    lv_obj_add_flag(video_pause_btn, LV_OBJ_FLAG_CLICKABLE);
    // 绑定事件
    lv_obj_add_event_cb(video_pause_btn, video_pause_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(video_pause_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* pause_img = lv_image_create(video_pause_btn);
    lv_image_set_src(pause_img, "A:/WQ/picture/pause.bmp");
    lv_obj_set_align(pause_img, LV_ALIGN_CENTER);
    lv_obj_set_size(pause_img, 60, 30);

    // 左箭头（上一首）
    video_left_btn = lv_button_create(parent);
    lv_obj_set_align(video_left_btn, LV_ALIGN_LEFT_MID);
    lv_obj_set_size(video_left_btn, 30, 60);
    lv_obj_set_style_bg_opa(video_left_btn, 0, 0);
    lv_obj_set_style_border_width(video_left_btn, 0, 0);
    lv_obj_add_flag(video_left_btn, LV_OBJ_FLAG_CLICKABLE);
    // 绑定事件
    lv_obj_add_event_cb(video_left_btn, video_nav_btn_cb, LV_EVENT_CLICKED, (void*)0);
    lv_obj_add_flag(video_left_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* left_img = lv_image_create(video_left_btn);
    lv_image_set_src(left_img, "A:/WQ/picture/last.bmp");
    lv_obj_set_align(left_img, LV_ALIGN_CENTER);
    lv_obj_set_size(left_img, 30, 60);

    // 右箭头（下一首）
    video_right_btn = lv_button_create(parent);
    lv_obj_set_align(video_right_btn, LV_ALIGN_RIGHT_MID);
    lv_obj_set_size(video_right_btn, 30, 60);
    lv_obj_set_style_bg_opa(video_right_btn, 0, 0);
    lv_obj_set_style_border_width(video_right_btn, 0, 0);
    lv_obj_add_flag(video_right_btn, LV_OBJ_FLAG_CLICKABLE);
    // 绑定事件
    lv_obj_add_event_cb(video_right_btn, video_nav_btn_cb, LV_EVENT_CLICKED, (void*)1);
    lv_obj_add_flag(video_right_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* right_img = lv_image_create(video_right_btn);
    lv_image_set_src(right_img, "A:/WQ/picture/next.bmp");
    lv_obj_set_align(right_img, LV_ALIGN_CENTER);
    lv_obj_set_size(right_img, 60, 60);
}
