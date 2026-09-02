#include "lvgl/lvgl.h"
#include "lvgl/demos/lv_demos.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>  
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>

// 全局变量
static const char* CORRECT_PASSWORD = "123456";
static lv_obj_t* ta_password = NULL;
static lv_obj_t* page_login = NULL;
static lv_obj_t* page_main = NULL;

// 主界面背景
static lv_obj_t* main_bg = NULL; 

// 相册相关
static const char* album_images[] = {
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

// 视频相关
static const char* video_cover_images[] = {
    "A:/WQ/picture/video1.bmp",
    "A:/WQ/picture/video2.bmp",
    "A:/WQ/picture/video3.bmp"
};
static const char* video_paths[] = {
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

// 三个图标容器
static lv_obj_t* icons[4] = {NULL, NULL, NULL, NULL};

// 井字棋游戏相关
static lv_obj_t* game_page = NULL;
static lv_obj_t* game_bg = NULL;
static lv_obj_t* game_return_btn = NULL;
static lv_obj_t* game_continue_btn = NULL;
static lv_obj_t* game_cells[3][3] = {NULL};
static int game_board[3][3] = {0};  // 0=空, 1=玩家1(圆点), 2=玩家2(叉号)
static int game_current_player = 1;  // 1=玩家1(圆点), 2=玩家2(叉号)
static int game_over = 0;
static lv_obj_t* game_result_label = NULL;

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
    if (is_connected) {
        lv_label_set_text(net_status_label, "Connected to server");
    } else {
        lv_label_set_text(net_status_label, "Disconnected");
    }
    
    // 确保文件列表标签被隐藏（避免残留显示）
    lv_label_set_text(net_file_list_label, "");
    lv_obj_add_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
}

static void net_client_init(void)
{
    if (sockfd > 0) {
        close(sockfd);
        sockfd = -1;
    }
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        lv_label_set_text(net_status_label, "Socket creation failed");
        return;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(IP);

    lv_label_set_text(net_status_label, "Connecting to server...");
    
    int ret = connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    if (ret == -1) {
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
    if (!is_connected || sockfd < 0) {
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
    if (!is_connected || sockfd < 0) {
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
    if (!is_connected || sockfd < 0) {
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
    if (!is_connected || sockfd < 0) {
        lv_label_set_text(net_status_label, "Not connected");
        return;
    }
    
    if (is_downloading) {
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
    if (!is_connected || sockfd < 0) {
        lv_label_set_text(net_status_label, "Not connected");
        return NULL;
    }
    
    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s%s", download_path, filename);
    
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
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
        if (new_progress != progress && new_progress % 10 == 0) {
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
    
    if (code == LV_EVENT_FOCUSED) {
        // 文本框获得焦点，显示键盘并绑定
        net_keyboard_target = ta;
        lv_keyboard_set_textarea(net_keyboard, ta);
        lv_obj_remove_flag(net_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(net_keyboard, -1);
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        // 文本框失去焦点，隐藏键盘
        if (net_keyboard_target == ta) {
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
    
    if (code == LV_EVENT_READY) {
        // 按下确认键，隐藏键盘
        lv_obj_add_flag(net_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(net_keyboard, NULL);
        net_keyboard_target = NULL;
    }
    else if (code == LV_EVENT_DEFOCUSED) {
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
    if (code == LV_EVENT_CLICKED) {
        if (!lv_obj_has_flag(net_keyboard, LV_OBJ_FLAG_HIDDEN)) {
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
    
    while (is_connected) {
        len = -1;
        type = -1;
        memset(head_buf, 0, sizeof(head_buf));
        memset(buf, 0, sizeof(buf));

        ret = my_read(confd, head_buf, 8);
        if (ret <= 0) {
            lv_label_set_text(net_status_label, "Disconnected from server");
            is_connected = 0;
            if (fd != -1) close(fd);
            break;
        }
        sscanf(head_buf, "%4d%4d", &len, &type);

        if (type == MSG_FILE_LIST) {
            ret = my_read(confd, buf, len);
            if (ret <= 0) {
                if (fd != -1) close(fd);
                is_connected = 0;
                break;
            }
            lv_label_set_text(net_file_list_label, buf);
            lv_obj_remove_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);
            lv_label_set_text(net_status_label, "File list received");
        }
        else if (type == MSG_VIEW_LOG) {
            ret = my_read(confd, buf, len);
            if (ret <= 0) {
                if (fd != -1) close(fd);
                is_connected = 0;
                break;
            }
            lv_label_set_text(net_file_list_label, buf);
            lv_obj_remove_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(net_status_label, "Log received");
        }
        else if (type == MSG_FILE_DOWNLOAD) {
            ret = my_read(confd, buf, len);
            if (ret <= 0) continue;
            
            char filename[256];
            strncpy(filename, buf, sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';
            
            if (strcmp(filename, "File not found") == 0) {
                lv_label_set_text(net_status_label, "File not found on server");
                is_downloading = 0;
                continue;
            }
            
            char file_path[512];
            snprintf(file_path, sizeof(file_path), "%s%s", download_path, filename);
            fd = open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if (fd == -1) {
                lv_label_set_text(net_status_label, "Failed to create file");
                is_downloading = 0;
                continue;
            }
            
            file_size = 0;
            total_received = 0;
            download_progress = 0;
        }
        else if (type == MSG_FILE_DATA) {
            if (len == 0) {
                lv_label_set_text(net_status_label, "Download completed");
                if (fd != -1) close(fd);
                fd = -1;
                is_downloading = 0;
                lv_bar_set_value(net_progress_bar, 100, LV_ANIM_ON);
                continue;
            }
            
            ret = my_read(confd, buf, len);
            if (ret <= 0) {
                if (fd != -1) close(fd);
                fd = -1;
                is_downloading = 0;
                continue;
            }
            
            if (fd != -1) {
                write(fd, buf, ret);
                total_received += ret;
                if (file_size == 0) {
                    file_size = len;
                }
                int new_progress = (int)((float)total_received / file_size * 100);
                if (new_progress != download_progress) {
                    download_progress = new_progress;
                    char status[256] = {0};
                    snprintf(status, sizeof(status), "Downloading: %d%%", download_progress);
                    lv_label_set_text(net_status_label, status);
                    lv_bar_set_value(net_progress_bar, download_progress, LV_ANIM_ON);
                }
            }
        }
        else if (type == MSG_FILE_UPLOAD) {
            ret = my_read(confd, buf, len);
            if (ret <= 0) {
                if (fd != -1) close(fd);
                is_connected = 0;
                break;
            }
            lv_label_set_text(net_status_label, buf);
        }
        else if (type == MSGQUIT) {
            ret = my_read(confd, buf, len);
            if (ret <= 0) {
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
    if (code == LV_EVENT_CLICKED) {

        net_client_init();
        
        if (is_connected) {
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
    if (code == LV_EVENT_CLICKED) {
        const char* msg = lv_textarea_get_text(net_textarea);
        if (strlen(msg) > 0) {
            net_send_message(msg);
            lv_textarea_set_text(net_textarea, "");
        }
    }
}

static void net_list_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
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
    if (code == LV_EVENT_CLICKED) {
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
    if (code == LV_EVENT_CLICKED) {
        const char* filename = lv_textarea_get_text(net_textarea);
        if (strlen(filename) > 0) {
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
    if (code == LV_EVENT_CLICKED) {
        const char* filename = lv_textarea_get_text(net_textarea);
        if (strlen(filename) > 0) {
            if (is_uploading) {
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
    if (code == LV_EVENT_CLICKED) {
        if (is_connected) {
            char quit_msg[64] = {0};
            snprintf(quit_msg, sizeof(quit_msg), "%s left the chat", username);
            net_send_message(quit_msg);

            lv_label_set_text(net_status_label, "Disconnected");

            is_connected = 0;
            if (sockfd > 0) {
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
        
        lv_obj_remove_flag(main_bg, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < 4; i++) {
            lv_obj_remove_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// 视频控制函数
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
        
        snprintf(cmd, sizeof(cmd), 
                 "mplayer64 -vo fbdev2 -slave -quiet -input file=/WQ/video/1.fifo -geometry 60:60 -zoom -x 900 -y 500 %s", actual_path);
        
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
        mplayer_fd = open("/WQ/video/1.fifo", O_WRONLY);
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
    if (mplayer_fd < 0) {
        mplayer_fd = open("/WQ/video/1.fifo", O_WRONLY);
        if (mplayer_fd < 0) {
            printf("Failed to open fifo\n");
            return;
        }
    }
    
    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "%s\n", cmd);
    write(mplayer_fd, buf, strlen(buf));
}

// 清空文本框
static void clear_textarea_timer_cb(lv_timer_t* t) 
{
    lv_obj_t* ta = (lv_obj_t*)t->user_data;
    lv_textarea_set_password_mode(ta, true);
    lv_textarea_set_text(ta, "");
    lv_textarea_set_max_length(ta, 6);
}

// 登录界面事件
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
            // 创建管道文件（mplayer控制用）
            mkfifo("/WQ/video/1.fifo", 0664);
            printf("FIFO created: /WQ/video/1.fifo\n");
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

// 相册功能
static void update_album_view(void)
{
    lv_image_set_src(album_view_image, album_images[current_image_index]);
    
    char page_text[16] = {0};
    sprintf(page_text, "%d/%d", current_image_index + 1, album_count);
    lv_label_set_text(album_page_label, page_text);
}

static void album_prev_image(void)
{
    if (current_image_index == 0) {
        current_image_index = album_count - 1;
    } else {
        current_image_index--;
    }
    update_album_view();
}

static void album_next_image(void)
{
    if (current_image_index == album_count - 1) {
        current_image_index = 0;
    } else {
        current_image_index++;
    }
    update_album_view();
}

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
        if (direction == 0) {
            album_prev_image();
        } else {
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

            lv_obj_remove_flag(main_bg, LV_OBJ_FLAG_HIDDEN);
            // 显示主界面三个图标
            for (int i = 0; i < 4; i++) 
            {
                lv_obj_remove_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

// 视频功能
static void update_video_view(void)
{
    // 显示视频封面
    lv_image_set_src(video_view_image, video_cover_images[current_video_index]);
}

static void video_prev(void)
{
    // 停止当前播放
    stop_mplayer();
    
    if (current_video_index == 0) {
        current_video_index = video_count - 1;
    } else {
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
    
    if (current_video_index == video_count - 1) {
        current_video_index = 0;
    } else {
        current_video_index++;
    }
    update_video_view();
    // 播放新视频
    start_mplayer(video_paths[current_video_index]);
}

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
        
        if (is_video_mode == 1) {
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

            lv_obj_remove_flag(main_bg, LV_OBJ_FLAG_HIDDEN);
            for (int i = 0; i < 4; i++) 
            {
                lv_obj_remove_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

static void video_pause_cb(lv_event_t* e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        send_mplayer_command("pause");
        // 切换播放状态显示
        if (mplayer_playing == 1) 
        {
            mplayer_playing = 2;  // 暂停
            printf("Video paused\n");
        } else if (mplayer_playing == 2) 
        {
            mplayer_playing = 1;  // 恢复
            printf("Video resumed\n");
        }
    }
}

// 井字棋游戏
// 井字棋游戏函数
static void game_init_board(void)
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            game_board[i][j] = 0;
        }
    }
    game_current_player = 1;
    game_over = 0;
}

static int game_check_winner(void)
{
    // 检查行
    for (int i = 0; i < 3; i++) {
        if (game_board[i][0] != 0 && game_board[i][0] == game_board[i][1] && game_board[i][1] == game_board[i][2]) {
            return game_board[i][0];
        }
    }
    // 检查列
    for (int j = 0; j < 3; j++) {
        if (game_board[0][j] != 0 && game_board[0][j] == game_board[1][j] && game_board[1][j] == game_board[2][j]) {
            return game_board[0][j];
        }
    }
    // 检查对角线
    if (game_board[0][0] != 0 && game_board[0][0] == game_board[1][1] && game_board[1][1] == game_board[2][2]) {
        return game_board[0][0];
    }
    if (game_board[0][2] != 0 && game_board[0][2] == game_board[1][1] && game_board[1][1] == game_board[2][0]) {
        return game_board[0][2];
    }
    return 0;
}

static int game_is_full(void)
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (game_board[i][j] == 0) return 0;
        }
    }
    return 1;
}

// 更新游戏界面
static void game_update_ui(void)
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
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
    if (!game_over) {
        char btn_text[32] = {0};
        sprintf(btn_text, "Player %d", game_current_player);
        lv_label_set_text(lv_obj_get_child(game_continue_btn, 0), btn_text);
        lv_obj_remove_flag(game_continue_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

// 自动重启游戏回调
static void game_auto_restart_cb(lv_timer_t* timer)
{
    // 重新初始化游戏
    game_init_board();
    
    // 显示所有单元格并清空内容
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            lv_obj_remove_flag(game_cells[i][j], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clean(game_cells[i][j]);
        }
    }
    
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
    if (winner != 0) {
        game_over = 1;
        const char* player_name = (winner == 1) ? "Player 1" : "Player 2";
        char result_text[64] = {0};
        sprintf(result_text, "%s Wins!", player_name);
        
        // 清屏显示胜利信息
        lv_label_set_text(game_result_label, result_text);
        lv_obj_remove_flag(game_result_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(game_result_label);
        
        // 隐藏网格和继续按钮
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                lv_obj_add_flag(game_cells[i][j], LV_OBJ_FLAG_HIDDEN);
            }
        }
        lv_obj_add_flag(game_continue_btn, LV_OBJ_FLAG_HIDDEN);
        
        // 自动重置游戏
        lv_timer_t* timer = lv_timer_create(game_auto_restart_cb, 1000, NULL);
        lv_timer_set_repeat_count(timer, 1);
        return;
    }
    
    // 检查平局
    if (game_is_full()) {
        game_over = 1;
        lv_label_set_text(game_result_label, "Draw!");
        lv_obj_remove_flag(game_result_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(game_result_label);
        
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                lv_obj_add_flag(game_cells[i][j], LV_OBJ_FLAG_HIDDEN);
            }
        }
        lv_obj_add_flag(game_continue_btn, LV_OBJ_FLAG_HIDDEN);
        
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
    if (code == LV_EVENT_CLICKED) {
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
    if (code == LV_EVENT_CLICKED) {
        lv_obj_add_flag(game_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(game_bg, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(game_return_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(game_continue_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(game_result_label, LV_OBJ_FLAG_HIDDEN);
        
        // 恢复主界面
        lv_obj_remove_flag(main_bg, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < 4; i++) {
            lv_obj_remove_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// 界面图标回调
static void icon_click_event_cb(lv_event_t* e) 
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) 
    {
        int index = (int)(intptr_t)lv_event_get_user_data(e);
        
        switch(index) 
        {
            case 0:  // 相册
               // 隐藏主界面图标
                for (int i = 0; i < 4; i++) 
                {
                    lv_obj_add_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
                }
                // 显示背景
                lv_obj_remove_flag(album_bg, LV_OBJ_FLAG_HIDDEN);
                // 显示返回按钮（网格模式）
                lv_obj_remove_flag(album_return_btn, LV_OBJ_FLAG_HIDDEN);
                // 显示网格容器
                lv_obj_remove_flag(album_grid_container, LV_OBJ_FLAG_HIDDEN);
                // 隐藏查看模式的部件
                lv_obj_add_flag(main_bg, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(album_view_image, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(album_page_label, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(album_left_btn, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(album_right_btn, LV_OBJ_FLAG_HIDDEN);
                is_album_mode = 0;
                break;
            case 1:  // 视频
                for (int i = 0; i < 4; i++) 
                {
                    lv_obj_add_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
                }
                lv_obj_remove_flag(video_bg, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(video_return_btn, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(video_grid_container, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(main_bg, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(video_view_image, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(video_left_btn, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(video_right_btn, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(video_pause_btn, LV_OBJ_FLAG_HIDDEN);
                is_video_mode = 0;
                break;
            case 2:  // 游戏
                for (int i = 0; i < 4; i++) 
                {
                    lv_obj_add_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
                }
                lv_obj_add_flag(main_bg, LV_OBJ_FLAG_HIDDEN);

                // 显示游戏页面
                lv_obj_remove_flag(game_page, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(game_bg, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(game_return_btn, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(game_continue_btn, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(game_result_label, LV_OBJ_FLAG_HIDDEN);

                // 初始化游戏
                game_init_board();
                // 显示所有单元格
                for (int i = 0; i < 3; i++) {
                    for (int j = 0; j < 3; j++) {
                    lv_obj_remove_flag(game_cells[i][j], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clean(game_cells[i][j]);
                    }
                }
                game_update_ui();
                break;
            case 3:  // 网络客户端
                for (int i = 0; i < 4; i++) 
                {
                    lv_obj_add_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
                }
                lv_obj_add_flag(main_bg, LV_OBJ_FLAG_HIDDEN);

                // 重置网络页面状态
                lv_label_set_text(net_status_label, "Disconnected");
                lv_label_set_text(net_file_list_label, "");
                lv_obj_add_flag(net_file_list_label, LV_OBJ_FLAG_HIDDEN);
                lv_bar_set_value(net_progress_bar, 0, LV_ANIM_OFF);
                lv_textarea_set_text(net_textarea, "");
                
                // 如果之前连接过，重置连接状态
                if (is_connected && sockfd > 0) {
                    close(sockfd);
                    sockfd = -1;
                    is_connected = 0;
                }

                // 显示网络页面
                lv_obj_remove_flag(net_page, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(net_bg, LV_OBJ_FLAG_HIDDEN);
                lv_obj_remove_flag(net_return_btn, LV_OBJ_FLAG_HIDDEN);
                break;
            default:
                break;
        }
    }
}

// 创建UI组件
static void create_album_content(void)
{
    album_bg = lv_image_create(page_main);
    lv_image_set_src(album_bg, "A:/WQ/picture/background.bmp");
    lv_obj_set_align(album_bg, LV_ALIGN_CENTER);
    lv_obj_set_size(album_bg, 1024, 600);
    lv_obj_move_to_index(album_bg, 0);
    lv_obj_add_flag(album_bg, LV_OBJ_FLAG_HIDDEN);

    // 相册网格容器
    album_grid_container = lv_obj_create(page_main);
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
    
    album_view_image = lv_image_create(page_main);
    lv_obj_set_pos(album_view_image, 0, 0);
    lv_obj_set_size(album_view_image, 1024, 600);
    lv_obj_add_flag(album_view_image, LV_OBJ_FLAG_HIDDEN);
    
    album_page_label = lv_label_create(page_main);
    lv_obj_set_align(album_page_label, LV_ALIGN_TOP_MID);
    lv_obj_set_style_text_color(album_page_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(album_page_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_bg_color(album_page_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(album_page_label, 0, 0);
    lv_obj_set_style_pad_all(album_page_label, 8, 0);
    lv_obj_set_style_radius(album_page_label, 6, 0);
    lv_obj_add_flag(album_page_label, LV_OBJ_FLAG_HIDDEN);

    album_return_btn = lv_button_create(page_main);
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
    
    album_left_btn = lv_button_create(page_main);
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
    
    album_right_btn = lv_button_create(page_main);
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

static void create_video_content(void)
{
    video_bg = lv_image_create(page_main);
    lv_image_set_src(video_bg, "A:/WQ/picture/background.bmp");
    lv_obj_set_align(video_bg, LV_ALIGN_CENTER);
    lv_obj_set_size(video_bg, 1024, 600);
    lv_obj_move_to_index(video_bg, 0);
    lv_obj_add_flag(video_bg, LV_OBJ_FLAG_HIDDEN);
    
    // 返回按钮
    video_return_btn = lv_button_create(page_main);
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
    video_grid_container = lv_obj_create(page_main);
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
    video_view_image = lv_image_create(page_main);
    lv_obj_set_pos(video_view_image, 0, 0);
    lv_obj_set_size(video_view_image, 1024, 600);
    lv_obj_add_flag(video_view_image, LV_OBJ_FLAG_HIDDEN);

    // 暂停按键
    video_pause_btn = lv_button_create(page_main);
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
    video_left_btn = lv_button_create(page_main);
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
    video_right_btn = lv_button_create(page_main);
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

static void create_game_content(void)
{
    // 游戏页面
    game_page = lv_obj_create(page_main);
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
    game_return_btn = lv_button_create(page_main);
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

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
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

// 创建网络客户端页面
static void create_network_content(void)
{
    net_page = lv_obj_create(page_main);
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
    net_return_btn = lv_button_create(page_main);
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
    
    icon_info_t icon_data[] = {
        {"A:/WQ/picture/album.bmp", "album", 50, 50},
        {"A:/WQ/picture/video.bmp", "video", 300, 50},
        {"A:/WQ/picture/game.bmp", "game", 550, 50},
        {"A:/WQ/picture/transfer.bmp", "transfer", 800, 50}
    };
    
    for (int i = 0; i < 4; i++) 
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

    create_album_content();
    create_video_content();
    create_game_content();
    create_network_content();
}

// 主函数
void myinterface(void) 
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
    
    create_main_page();
    
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

int main(void)
{
    lv_init();

    lv_display_t * disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, "/dev/fb0");

    lv_indev_t * indev = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event6");

    myinterface();

    while(1) {
        lv_timer_handler();
        usleep(5000);
    }

    return 0;
}