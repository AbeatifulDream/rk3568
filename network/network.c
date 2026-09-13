#include "network.h"
#include "../main/main.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// ==================== 网络客户端相关 ====================
static int sockfd = -1;
static unsigned short port = 10086;
static char* IP = "192.168.31.12";
static char username[20] = {"Guest"};
static char download_path[256] = "./downloads/";
static long file_size = 0;
static long total_received = 0;
static int download_progress = 0;
static int is_connected = 0;
static int is_downloading = 0;
static char current_download_filename[256] = {0};

// 网络消息类型
enum MSGTYPE
{
    MSGDATA,
    MSGQUIT,
    MSG_FILE_LIST,
    MSG_FILE_DOWNLOAD,
    MSG_FILE_UPLOAD,
    MSG_FILE_DATA,
    MSG_VIEW_LOG
};

// 网络客户端页面控件
static lv_obj_t* net_page = NULL;
static lv_obj_t* net_bg = NULL;
static lv_obj_t* net_return_btn = NULL;
static lv_obj_t* net_status_label = NULL;
static lv_obj_t* net_textarea = NULL;
static lv_obj_t* net_send_btn = NULL;
static lv_obj_t* net_list_btn = NULL;
static lv_obj_t* net_log_btn = NULL;
static lv_obj_t* net_download_btn = NULL;
static lv_obj_t* net_upload_btn = NULL;
static lv_obj_t* net_progress_bar = NULL;
static lv_obj_t* net_progress_label = NULL;
static lv_obj_t* net_file_list_label = NULL;
static lv_obj_t* net_connect_btn = NULL;
static lv_obj_t* net_username_ta = NULL;
static lv_obj_t* net_ip_ta = NULL;
static lv_obj_t* net_port_ta = NULL;
static pthread_t upload_thread = 0;
static int is_uploading = 0;

// 键盘相关
static lv_obj_t* net_keyboard = NULL;
static lv_obj_t* net_keyboard_target = NULL;

// ==================== 网络函数 ====================
static int my_read(int fd, char* buf, int len)
{
    int readed_bytes = 0;
    int ret = -1;
    while (1)
    {
        ret = read(fd, buf + readed_bytes, len - readed_bytes);
        if (ret == -1)
            return -1;
        if (ret == 0)
            return readed_bytes;
        readed_bytes += ret;
        if (readed_bytes == len)
            return readed_bytes;
    }
}

// 上传完成后的清理函数
static void upload_complete_reset(lv_timer_t* timer)
{
    // 重置进度条
    lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);

    // 恢复状态信息
    if (is_connected) 
    {
        lv_label_set_text(net_status_label, "Connected to server");
    } 
    else 
    {
        lv_label_set_text(net_status_label, "Disconnected");
    }

    // 确保文件列表标签被隐藏（避免残留显示）
    lv_label_set_text(net_file_list_label, "");
    lv_obj_add_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
}

static void net_client_init(void)
{
    if (sockfd > 0) 
    {
        close(sockfd);
        sockfd = -1;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) 
    {
        lv_label_set_text(net_status_label, "Socket creation failed");
        return;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(IP);

    lv_label_set_text(net_status_label, "Connecting to server...");

    int ret = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) 
    {
        lv_label_set_text(net_status_label, "Connection failed");
        close(sockfd);
        sockfd = -1;
        return;
    }

    mkdir(download_path, 0755);
    is_connected = 1;
    lv_label_set_text(net_status_label, "Connected to server");
}

static void net_send_message(const char* msg)
{
    if (!is_connected || sockfd < 0) 
    {
        lv_label_set_text(net_status_label, "Not connected");
        return;
    }

    int type = MSGDATA;
    char send_buf[1024] = {0};
    snprintf(send_buf, sizeof(send_buf), "%s:%s", username, msg);
    int len = strlen(send_buf);
    char header[16] = {0};
    snprintf(header, sizeof(header), "%4d%4d", len, type);
    write(sockfd, header, 8);
    write(sockfd, send_buf, len);
}

static void net_get_file_list(void)
{
    if (!is_connected || sockfd < 0) 
    {
        lv_label_set_text(net_status_label, "Not connected");
        return;
    }

    int type = MSG_FILE_LIST;
    char send_buf[16] = {0};
    snprintf(send_buf, sizeof(send_buf), "%4d%4d", 0, type);
    write(sockfd, send_buf, 8);
    lv_label_set_text(net_status_label, "Requesting file list...");
}

static void net_view_log(void)
{
    if (!is_connected || sockfd < 0) 
    {
        lv_label_set_text(net_status_label, "Not connected");
        return;
    }

    int type = MSG_VIEW_LOG;
    char send_buf[16] = {0};
    snprintf(send_buf, sizeof(send_buf), "%4d%4d", 0, type);
    write(sockfd, send_buf, 8);
    lv_label_set_text(net_status_label, "Requesting chat log...");
}

static void net_download_file(const char* filename)
{
    if (!is_connected || sockfd < 0) 
    {
        lv_label_set_text(net_status_label, "Not connected");
        return;
    }

    if (is_downloading) 
    {
        lv_label_set_text(net_status_label, "Download in progress...");
        return;
    }

    strncpy(current_download_filename, filename, sizeof(current_download_filename) - 1);
    is_downloading = 1;
    download_progress = 0;
    file_size = 0;
    total_received = 0;

    int type = MSG_FILE_DOWNLOAD;
    int len = strlen(filename);
    char send_buf[512] = {0};
    snprintf(send_buf, sizeof(send_buf), "%4d%4d%s", len, type, filename);
    write(sockfd, send_buf, len + 8);

    char status[256] = {0};
    snprintf(status, sizeof(status), "Downloading: %s 0%%", filename);
    lv_label_set_text(net_status_label, status);
}

static void* upload_file_thread(void* arg)
{
    pthread_detach(pthread_self());
    const char* filename = (const char*)arg;

    // 实际的的上传逻辑
    if (!is_connected || sockfd < 0) 
    {
        lv_label_set_text(net_status_label, "Not connected");
        return NULL;
    }

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s%s", download_path, filename);

    int fd = open(file_path, O_RDONLY);
    if (fd == -1) 
    {
        char msg[256] = {0};
        snprintf(msg, sizeof(msg), "File not found: %s", filename);
        lv_label_set_text(net_status_label, msg);
        lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);
        is_uploading = 0;
        return NULL;
    }

    struct stat st;
    fstat(fd, &st);
    long file_size = st.st_size;

    char status[256] = {0};
    snprintf(status, sizeof(status), "Uploading: %s", filename);
    lv_label_set_text(net_status_label, status);
    lv_obj_remove_flag(net_progress_bar, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);

    int type = MSG_FILE_UPLOAD;
    char info[512] = {0};
    snprintf(info, sizeof(info), "%s %ld", filename, file_size);
    int info_len = strlen(info);
    char send_buf[1024] = {0};
    snprintf(send_buf, sizeof(send_buf), "%4d%4d%s", info_len, type, info);
    write(sockfd, send_buf, info_len + 8);

    char readbuf[992] = {0};
    int ret;
    int progress = 0;
    long total_sent = 0;

    while (1)
    {
        memset(readbuf, 0, sizeof(readbuf));
        ret = read(fd, readbuf, sizeof(readbuf));
        if (ret <= 0)
            break;

        write(sockfd, readbuf, ret);
        total_sent += ret;

        int new_progress = (int)((float)total_sent / file_size * 100);
        if (new_progress != progress && new_progress % 10 == 0) 
        {
            progress = new_progress;
            char prog_str[64] = {0};
            snprintf(prog_str, sizeof(prog_str), "Upload progress: %d%%", progress);
            lv_label_set_text(net_status_label, prog_str);
            lv_bar_set_value(net_progress_bar, progress, LV_ANIM_ON);
        }
    }

    close(fd);
    lv_label_set_text(net_status_label, "Upload completed");
    lv_bar_set_value(net_progress_bar, 100, LV_ANIM_ON);

    // 2秒后自动重置UI
    lv_timer_t* timer = lv_timer_create(upload_complete_reset, 2000, NULL);
    lv_timer_set_repeat_count(timer, 1);

    is_uploading = 0;
    return NULL;
}

// 文本框焦点事件回调
static void net_textarea_focus_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* ta = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED) 
    {
        // 文本框获得焦点，显示键盘并绑定
        net_keyboard_target = ta;
        lv_keyboard_set_textarea(net_keyboard, ta);
        lv_obj_remove_flag(net_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(net_keyboard, -1);
    }
    else if (code == LV_EVENT_DEFOCUSED) 
    {
        // 文本框失去焦点，隐藏键盘
        if (net_keyboard_target == ta) 
        {
            net_keyboard_target = NULL;
            lv_keyboard_set_textarea(net_keyboard, NULL);
            lv_obj_add_flag(net_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// 键盘事件回调
static void net_keyboard_event_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY) 
    {
        // 按下确认键，隐藏键盘
        lv_obj_add_flag(net_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(net_keyboard, NULL);
        net_keyboard_target = NULL;
    }
    else if (code == LV_EVENT_DEFOCUSED) 
    {
        // 键盘失去焦点，隐藏键盘
        lv_obj_add_flag(net_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(net_keyboard, NULL);
        net_keyboard_target = NULL;
    }
}

// 点击空白区域隐藏键盘
static void net_page_click_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        if (!lv_obj_has_flag(net_keyboard, LV_OBJ_FLAG_HIDDEN)) 
        {
            lv_obj_add_flag(net_keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(net_keyboard, NULL);
            net_keyboard_target = NULL;
            // 移除所有文本框的焦点
            lv_obj_clear_state(net_username_ta, LV_STATE_FOCUSED);
            lv_obj_clear_state(net_ip_ta, LV_STATE_FOCUSED);
            lv_obj_clear_state(net_port_ta, LV_STATE_FOCUSED);
            lv_obj_clear_state(net_textarea, LV_STATE_FOCUSED);
        }
    }
}

// ==================== 网络接收线程 ====================
static void* net_recv_thread(void* arg)
{
    pthread_detach(pthread_self());
    int confd = sockfd;
    int len = -1, type = -1;
    char head_buf[13] = {0};
    char buf[4096] = {0};
    int ret;
    int fd = -1;

    while (is_connected) 
    {
        len = -1;
        type = -1;
        memset(head_buf, 0, sizeof(head_buf));
        memset(buf, 0, sizeof(buf));

        ret = my_read(confd, head_buf, 8);
        if (ret <= 0) 
        {
            lv_label_set_text(net_status_label, "Disconnected from server");
            is_connected = 0;
            if (fd != -1) close(fd);
            break;
        }
        sscanf(head_buf, "%4d%4d", &len, &type);

        if (type == MSG_FILE_LIST) 
        {
            ret = my_read(confd, buf, len);
            if (ret <= 0) 
            {
                if (fd != -1) close(fd);
                is_connected = 0;
                break;
            }
            lv_label_set_text(net_file_list_label, buf);
            lv_obj_remove_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);
            lv_label_set_text(net_status_label, "File list received");
        }
        else if (type == MSG_VIEW_LOG) 
        {
            ret = my_read(confd, buf, len);
            if (ret <= 0) 
            {
                if (fd != -1) close(fd);
                is_connected = 0;
                break;
            }
            lv_label_set_text(net_file_list_label, buf);
            lv_obj_remove_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(net_status_label, "Log received");
        }
        else if (type == MSG_FILE_DOWNLOAD) 
        {
            ret = my_read(confd, buf, len);
            if (ret <= 0) continue;

            char filename[256];
            strncpy(filename, buf, sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';

            if (strcmp(filename, "File not found") == 0) 
            {
                lv_label_set_text(net_status_label, "File not found on server");
                is_downloading = 0;
                continue;
            }

            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s%s", download_path, filename);
            fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if (fd == -1) 
            {
                lv_label_set_text(net_status_label, "Failed to create file");
                is_downloading = 0;
                continue;
            }

            file_size = 0;
            total_received = 0;
            download_progress = 0;
        }
        else if (type == MSG_FILE_DATA) 
        {
            if (len == 0) 
            {
                lv_label_set_text(net_status_label, "Download completed");
                if (fd != -1) close(fd);
                fd = -1;
                is_downloading = 0;
                lv_bar_set_value(net_progress_bar, 100, LV_ANIM_ON);
                continue;
            }

            ret = my_read(confd, buf, len);
            if (ret <= 0) 
            {
                if (fd != -1) close(fd);
                fd = -1;
                is_downloading = 0;
                continue;
            }

            if (fd != -1) 
            {
                write(fd, buf, ret);
                total_received += ret;
                if (file_size == 0) 
                {
                    file_size = len;
                }
                int new_progress = (int)((float)total_received / file_size * 100);
                if (new_progress != download_progress) 
                {
                    download_progress = new_progress;
                    char status[256] = {0};
                    snprintf(status, sizeof(status), "Downloading: %d%%", download_progress);
                    lv_label_set_text(net_status_label, status);
                    lv_bar_set_value(net_progress_bar, download_progress, LV_ANIM_ON);
                }
            }
        }
        else if (type == MSG_FILE_UPLOAD) 
        {
            ret = my_read(confd, buf, len);
            if (ret <= 0) 
            {
                if (fd != -1) close(fd);
                is_connected = 0;
                break;
            }
            lv_label_set_text(net_status_label, buf);
        }
        else if (type == MSGQUIT) 
        {
            ret = my_read(confd, buf, len);
            if (ret <= 0) 
            {
                close(confd);
                is_connected = 0;
                break;
            }
            lv_label_set_text(net_status_label, "Server closed connection");
            if (fd != -1) close(fd);
            is_connected = 0;
            break;
        }
    }

    close(confd);
    sockfd = -1;
    return NULL;
}

// ==================== 网络页面回调 ====================
static void net_connect_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        net_client_init();

        if (is_connected) 
        {
            pthread_t tid;
            pthread_create(&tid, NULL, net_recv_thread, NULL);

            char join_msg[64] = {0};
            snprintf(join_msg, sizeof(join_msg), "%s entered the chat", username);
            net_send_message(join_msg);
        }
    }
}

static void net_send_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        const char* msg = lv_textarea_get_text(net_textarea);
        if (strlen(msg) > 0) 
        {
            net_send_message(msg);
            lv_textarea_set_text(net_textarea, "");
        }
    }
}

static void net_list_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        // 清理之前的内容
        lv_label_set_text(net_file_list_label, "");
        lv_obj_remove_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
        // 重置进度条
        lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);
        // 显示加载状态
        lv_label_set_text(net_status_label, "Requesting file list...");
        net_get_file_list();
    }
}

static void net_log_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        // 清理之前的内容
        lv_label_set_text(net_file_list_label, "");
        lv_obj_remove_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
        // 重置进度条
        lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);
        lv_label_set_text(net_status_label, "Requesting chat log...");
        net_view_log();
    }
}

static void net_download_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        const char* filename = lv_textarea_get_text(net_textarea);
        if (strlen(filename) > 0) 
        {
            // 清空并隐藏文件列表区域
            lv_label_set_text(net_file_list_label, "");
            lv_obj_add_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
            // 重置进度条
            lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);
            // 确保进度条可见
            lv_obj_remove_flag(net_progress_bar, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(net_status_label, "Downloading...");
            net_download_file(filename);
            lv_textarea_set_text(net_textarea, "");
        }
    }
}

static void net_upload_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        const char* filename = lv_textarea_get_text(net_textarea);
        if (strlen(filename) > 0) 
        {
            if (is_uploading) 
            {
                lv_label_set_text(net_status_label, "Upload already in progress...");
                return;
            }

            // 清空并隐藏文件列表区域
            lv_label_set_text(net_file_list_label, "");
            lv_obj_add_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
            // 重置进度条
            lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);
            // 确保进度条可见
            lv_obj_remove_flag(net_progress_bar, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(net_status_label, "Uploading...");

            // 创建上传线程
            char* filename_copy = malloc(strlen(filename) + 1);
            strcpy(filename_copy, filename);
            is_uploading = 1;
            pthread_create(&upload_thread, NULL, upload_file_thread, filename_copy);

            lv_textarea_set_text(net_textarea, "");
        }
    }
}

static void net_return_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        if (is_connected) 
        {
            char quit_msg[64] = {0};
            snprintf(quit_msg, sizeof(quit_msg), "%s left the chat", username);
            net_send_message(quit_msg);

            lv_label_set_text(net_status_label, "Disconnected");

            is_connected = 0;
            if (sockfd > 0) 
            {
                close(sockfd);
                sockfd = -1;
            }
        }

        // 清空文件列表/日志显示区域
        lv_label_set_text(net_file_list_label, "");
        lv_obj_add_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);

        // 重置进度条
        lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);

        // 清空输入框
        lv_textarea_set_text(net_textarea, "");

        lv_obj_add_flag(net_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(net_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(net_return_btn, LV_OBJ_FLAG_HIDDEN);

        // 恢复主界面
        app_main_page_show();
    }
}

// ==================== 对外接口 ====================
void network_show(void)
{
    // 隐藏主界面(背景 + 四个图标)
    app_main_page_hide();

    // 重置网络页面状态
    lv_label_set_text(net_status_label, "Disconnected");
    lv_label_set_text(net_file_list_label, "");
    lv_obj_add_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);
    lv_textarea_set_text(net_textarea, "");

    // 如果之前连接过，重置连接状态
    if (is_connected && sockfd > 0) 
    {
        close(sockfd);
        sockfd = -1;
        is_connected = 0;
    }

    // 显示网络页面
    lv_obj_remove_flag(net_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(net_bg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(net_return_btn, LV_OBJ_FLAG_HIDDEN);
}

void network_create(lv_obj_t* parent)
{
    net_page = lv_obj_create(parent);
    lv_obj_set_size(net_page, 1024, 600);
    lv_obj_set_align(net_page, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(net_page, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(net_page, 0, 0);
    lv_obj_add_flag(net_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(net_page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(net_page, LV_SCROLLBAR_MODE_OFF);

    // 背景
    net_bg = lv_obj_create(net_page);
    lv_obj_set_size(net_bg, 1024, 600);
    lv_obj_set_align(net_bg, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(net_bg, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_bg_opa(net_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(net_bg, 0, 0);
    lv_obj_move_to_index(net_bg, 0);
    lv_obj_add_flag(net_bg, LV_OBJ_FLAG_HIDDEN);

    // 背景可点击，用于隐藏键盘
    lv_obj_add_flag(net_bg, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(net_bg, net_page_click_cb, LV_EVENT_CLICKED, NULL);

    // 返回按钮
    net_return_btn = lv_button_create(parent);
    lv_obj_set_pos(net_return_btn, 0, 0);
    lv_obj_set_size(net_return_btn, 40, 40);
    lv_obj_set_style_border_width(net_return_btn, 0, 0);
    lv_obj_add_flag(net_return_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(net_return_btn, net_return_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(net_return_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* return_img = lv_image_create(net_return_btn);
    lv_image_set_src(return_img, "A:/WQ/picture/return.bmp");
    lv_obj_set_align(return_img, LV_ALIGN_CENTER);
    lv_obj_set_size(return_img, 40, 40);

    // 状态标签
    net_status_label = lv_label_create(net_page);
    lv_obj_set_pos(net_status_label, 50, 10);
    lv_obj_set_style_text_color(net_status_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(net_status_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(net_status_label, "Disconnected");

    // 连接按钮
    net_connect_btn = lv_button_create(net_page);
    lv_obj_set_pos(net_connect_btn, 350, 10);
    lv_obj_set_size(net_connect_btn, 120, 35);
    lv_obj_set_style_bg_color(net_connect_btn, lv_color_hex(0x4CAF50), 0);
    lv_obj_add_event_cb(net_connect_btn, net_connect_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* connect_label = lv_label_create(net_connect_btn);
    lv_label_set_text(connect_label, "Connect");
    lv_obj_set_align(connect_label, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(connect_label, lv_color_hex(0xFFFFFF), 0);

    // 操作按钮行
    net_list_btn = lv_button_create(net_page);
    lv_obj_set_pos(net_list_btn, 350, 55);
    lv_obj_set_size(net_list_btn, 85, 32);
    lv_obj_add_event_cb(net_list_btn, net_list_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* list_label = lv_label_create(net_list_btn);
    lv_label_set_text(list_label, "List");
    lv_obj_set_align(list_label, LV_ALIGN_CENTER);

    net_log_btn = lv_button_create(net_page);
    lv_obj_set_pos(net_log_btn, 445, 55);
    lv_obj_set_size(net_log_btn, 85, 32);
    lv_obj_add_event_cb(net_log_btn, net_log_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* log_label = lv_label_create(net_log_btn);
    lv_label_set_text(log_label, "Log");
    lv_obj_set_align(log_label, LV_ALIGN_CENTER);

    net_download_btn = lv_button_create(net_page);
    lv_obj_set_pos(net_download_btn, 540, 55);
    lv_obj_set_size(net_download_btn, 85, 32);
    lv_obj_add_event_cb(net_download_btn, net_download_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* download_label = lv_label_create(net_download_btn);
    lv_label_set_text(download_label, "Download");
    lv_obj_set_align(download_label, LV_ALIGN_CENTER);

    net_upload_btn = lv_button_create(net_page);
    lv_obj_set_pos(net_upload_btn, 635, 55);
    lv_obj_set_size(net_upload_btn, 85, 32);
    lv_obj_add_event_cb(net_upload_btn, net_upload_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* upload_label = lv_label_create(net_upload_btn);
    lv_label_set_text(upload_label, "Upload");
    lv_obj_set_align(upload_label, LV_ALIGN_CENTER);

    // 消息输入框
    net_textarea = lv_textarea_create(net_page);
    lv_obj_set_pos(net_textarea, 350, 97);
    lv_obj_set_size(net_textarea, 370, 36);
    lv_textarea_set_placeholder_text(net_textarea, "Enter message or filename...");
    lv_textarea_set_text(net_textarea, "");
    lv_obj_remove_flag(net_textarea, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(net_textarea, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_event_cb(net_textarea, net_textarea_focus_cb, LV_EVENT_ALL, NULL);

    // 发送按钮
    net_send_btn = lv_button_create(net_page);
    lv_obj_set_pos(net_send_btn, 730, 97);
    lv_obj_set_size(net_send_btn, 70, 36);
    lv_obj_add_event_cb(net_send_btn, net_send_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* send_label = lv_label_create(net_send_btn);
    lv_label_set_text(send_label, "Send");
    lv_obj_set_align(send_label, LV_ALIGN_CENTER);

    // 进度条
    net_progress_bar = lv_bar_create(net_page);
    lv_obj_set_pos(net_progress_bar, 350, 143);
    lv_obj_set_size(net_progress_bar, 450, 16);
    lv_bar_set_range(net_progress_bar, 0, 100);
    lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_remove_flag(net_progress_bar, LV_OBJ_FLAG_HIDDEN);

    // 文件列表/日志显示区域
    net_file_list_label = lv_label_create(net_page);
    lv_obj_set_pos(net_file_list_label, 350, 170);
    lv_obj_set_size(net_file_list_label, 500, 360);
    lv_obj_set_style_bg_color(net_file_list_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(net_file_list_label, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(net_file_list_label, 1, 0);
    lv_obj_set_style_border_color(net_file_list_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_radius(net_file_list_label, 4, 0);
    lv_obj_set_style_text_color(net_file_list_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(net_file_list_label, &lv_font_montserrat_16, 0);
    lv_obj_add_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(net_file_list_label, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(net_file_list_label, LV_SCROLLBAR_MODE_ACTIVE);
    lv_label_set_long_mode(net_file_list_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(net_file_list_label, LV_TEXT_ALIGN_LEFT, 0);

    // 创建键盘
    net_keyboard = lv_keyboard_create(lv_layer_top());
    lv_obj_set_align(net_keyboard, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_size(net_keyboard, 1024, 200);
    lv_obj_add_flag(net_keyboard, LV_OBJ_FLAG_HIDDEN);

    // 键盘事件
    lv_obj_add_event_cb(net_keyboard, net_keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(net_keyboard, net_keyboard_event_cb, LV_EVENT_DEFOCUSED, NULL);
}
