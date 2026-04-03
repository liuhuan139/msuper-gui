#include <gtk/gtk.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <mutex>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <unistd.h>

namespace fs = std::filesystem;

// 状态枚举
enum class Status {
    READY,
    WORKING,
    SUCCESS,
    ERROR
};

class MSuperGUI {
private:
    GtkWidget *window;
    GtkWidget *main_vbox;
    GtkWidget *content_vbox;
    GtkWidget *title_label;
    GtkWidget *file_chooser_button;
    GtkWidget *file_chooser_box;
    GtkWidget *process_button;
    GtkWidget *status_label;
    GtkWidget *partition_combo;
    GtkWidget *partition_combo_box;
    GtkWidget *partition_label;
    GtkWidget *mount_button;
    GtkWidget *unmount_button;
    GtkWidget *button_box;
    GtkWidget *menubar;
    GtkWidget *statusbar_box;
    GtkWidget *status_light;
    GtkWidget *status_light_event_box;

    std::string input_img;
    std::string ext4_img;
    std::string unpack_dir;
    std::string mount_dir;
    std::vector<std::string> partition_images;

    Status current_status;
    guint breathe_timeout_id;
    bool breathe_state;

    // 工作目录
    const std::string IMAGES_DIR = std::string(getenv("HOME")) + "/.local/images";
    const std::string MOUNT_DIR = IMAGES_DIR + "/mount";
    const std::string UNPACK_DIR = IMAGES_DIR + "/unpacked";

    // 日志文件
    const std::string LOG_FILE = "/tmp/msuper-gui.log";
    std::mutex log_mutex;

    // 工具路径（会在构造函数中设置）
    std::string SIMG2IMG;
    std::string LPUNPACK;

public:
    MSuperGUI() : current_status(Status::READY), breathe_timeout_id(0), breathe_state(false) {
        // 获取程序所在目录
        std::string program_dir = get_program_directory();
        SIMG2IMG = program_dir + "/simg2img";
        LPUNPACK = program_dir + "/lpunpack";

        // 初始化GTK
        gtk_init(NULL, NULL);

        // 创建主窗口
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window), "Android Super Image Tool");
        gtk_window_set_default_size(GTK_WINDOW(window), 700, 550);
        gtk_container_set_border_width(GTK_CONTAINER(window), 20);

        // 连接信号
        g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), this);

        // 创建主垂直布局
        main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(window), main_vbox);

        // 创建菜单栏
        create_menu_bar();

        // 创建内容区域（带居中对齐）
        content_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
        gtk_widget_set_halign(content_vbox, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(content_vbox, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(main_vbox), content_vbox, TRUE, TRUE, 0);
        gtk_widget_set_size_request(content_vbox, 500, -1);

        // 创建大标题
        create_title();

        // 创建文件选择区域
        create_file_chooser_section();

        // 创建处理按钮
        create_process_button();

        // 创建分区选择区域
        create_partition_section();

        // 创建挂载/卸载按钮区域
        create_mount_buttons();

        // 创建状态标签
        create_status_label();

        // 创建底部状态栏（颜色灯）
        create_status_light();

        // 显示所有控件
        gtk_widget_show_all(window);

        // 创建工作目录
        create_working_dirs();

        // 初始化日志
        log_message("===== 程序启动 =====");
        log_message("工作目录: " + IMAGES_DIR);
        log_message("挂载点: " + MOUNT_DIR);
        log_message("解包目录: " + UNPACK_DIR);
        log_message("日志文件: " + LOG_FILE);
        log_message("simg2img路径: " + SIMG2IMG);
        log_message("lpunpack路径: " + LPUNPACK);

        // 设置初始状态
        set_status(Status::READY);
    }

    void run() {
        gtk_main();
    }

private:
    // 创建大标题
    void create_title() {
        title_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(title_label), "<span font_desc='Sans Bold 24'>Android Super Image Tool</span>");
        gtk_box_pack_start(GTK_BOX(content_vbox), title_label, FALSE, FALSE, 10);

        // 添加分隔线
        GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_box_pack_start(GTK_BOX(content_vbox), separator, FALSE, FALSE, 20);
    }

    // 创建文件选择区域
    void create_file_chooser_section() {
        file_chooser_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_box_pack_start(GTK_BOX(content_vbox), file_chooser_box, FALSE, FALSE, 0);

        GtkWidget *label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), "<b>选择 Super 镜像文件:</b>");
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(file_chooser_box), label, FALSE, FALSE, 0);

        file_chooser_button = gtk_file_chooser_button_new("选择 Super.img 文件", GTK_FILE_CHOOSER_ACTION_OPEN);
        gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(file_chooser_button), create_file_filter());
        gtk_widget_set_size_request(file_chooser_button, 400, -1);
        gtk_box_pack_start(GTK_BOX(file_chooser_box), file_chooser_button, FALSE, FALSE, 0);
    }

    // 创建处理按钮
    void create_process_button() {
        process_button = gtk_button_new_with_label("处理镜像");
        gtk_widget_set_size_request(process_button, 200, 40);
        gtk_widget_set_halign(process_button, GTK_ALIGN_CENTER);
        gtk_box_pack_start(GTK_BOX(content_vbox), process_button, FALSE, FALSE, 10);
        g_signal_connect(process_button, "clicked", G_CALLBACK(on_process_clicked), this);
    }

    // 创建分区选择区域
    void create_partition_section() {
        partition_combo_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_box_pack_start(GTK_BOX(content_vbox), partition_combo_box, FALSE, FALSE, 0);

        partition_label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(partition_label), "<b>选择分区:</b>");
        gtk_widget_set_halign(partition_label, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(partition_combo_box), partition_label, FALSE, FALSE, 0);

        partition_combo = gtk_combo_box_text_new();
        gtk_widget_set_size_request(partition_combo, 400, -1);
        gtk_box_pack_start(GTK_BOX(partition_combo_box), partition_combo, FALSE, FALSE, 0);
    }

    // 创建挂载/卸载按钮区域
    void create_mount_buttons() {
        button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_CENTER);
        gtk_box_set_spacing(GTK_BOX(button_box), 20);
        gtk_box_pack_start(GTK_BOX(content_vbox), button_box, FALSE, FALSE, 10);

        mount_button = gtk_button_new_with_label("挂载所选分区");
        gtk_widget_set_size_request(mount_button, 150, 35);
        gtk_container_add(GTK_CONTAINER(button_box), mount_button);
        g_signal_connect(mount_button, "clicked", G_CALLBACK(on_mount_clicked), this);

        unmount_button = gtk_button_new_with_label("卸载分区");
        gtk_widget_set_size_request(unmount_button, 150, 35);
        gtk_container_add(GTK_CONTAINER(button_box), unmount_button);
        g_signal_connect(unmount_button, "clicked", G_CALLBACK(on_unmount_clicked), this);
    }

    // 创建状态标签
    void create_status_label() {
        status_label = gtk_label_new("就绪");
        gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);
        gtk_widget_set_size_request(status_label, 450, -1);
        gtk_box_pack_start(GTK_BOX(content_vbox), status_label, FALSE, FALSE, 10);
    }

    // 创建底部状态灯
    void create_status_light() {
        statusbar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_set_halign(statusbar_box, GTK_ALIGN_END);
        gtk_container_set_border_width(GTK_CONTAINER(statusbar_box), 5);
        gtk_box_pack_end(GTK_BOX(main_vbox), statusbar_box, FALSE, FALSE, 0);

        // 创建一个事件框来容纳 DrawingArea
        status_light_event_box = gtk_event_box_new();
        gtk_widget_set_size_request(status_light_event_box, 20, 20);

        // 创建绘图区域
        status_light = gtk_drawing_area_new();
        gtk_widget_set_size_request(status_light, 20, 20);
        g_signal_connect(status_light, "draw", G_CALLBACK(on_status_light_draw), this);

        gtk_container_add(GTK_CONTAINER(status_light_event_box), status_light);
        gtk_box_pack_start(GTK_BOX(statusbar_box), status_light_event_box, FALSE, FALSE, 0);
    }

    // 设置状态颜色
    void set_status(Status status) {
        current_status = status;

        // 停止之前的呼吸动画
        if (breathe_timeout_id > 0) {
            g_source_remove(breathe_timeout_id);
            breathe_timeout_id = 0;
        }

        breathe_state = false;

        // 根据状态设置动画
        if (status == Status::WORKING) {
            // 工作状态：呼吸动画
            breathe_timeout_id = g_timeout_add(500, (GSourceFunc)breathe_callback, this);
        }

        // 重绘
        gtk_widget_queue_draw(status_light);
    }

    // 呼吸动画回调
    static gboolean breathe_callback(gpointer data) {
        MSuperGUI *gui = static_cast<MSuperGUI*>(data);
        gui->breathe_state = !gui->breathe_state;
        gtk_widget_queue_draw(gui->status_light);
        return G_SOURCE_CONTINUE;
    }

    // 绘制状态灯
    static gboolean on_status_light_draw(GtkWidget *widget, cairo_t *cr, gpointer data) {
        MSuperGUI *gui = static_cast<MSuperGUI*>(data);

        GtkAllocation allocation;
        gtk_widget_get_allocation(widget, &allocation);
        int width = allocation.width;
        int height = allocation.height;
        int radius = std::min(width, height) / 2 - 2;
        int center_x = width / 2;
        int center_y = height / 2;

        // 绘制圆形
        cairo_arc(cr, center_x, center_y, radius, 0, 2 * G_PI);

        // 设置颜色
        double alpha = 1.0;
        switch (gui->current_status) {
            case Status::READY:
                cairo_set_source_rgba(cr, 0.5, 0.5, 0.5, 1.0); // 灰色
                break;
            case Status::WORKING:
                alpha = gui->breathe_state ? 1.0 : 0.3;
                cairo_set_source_rgba(cr, 1.0, 0.84, 0.0, alpha); // 黄色，呼吸
                break;
            case Status::SUCCESS:
                cairo_set_source_rgba(cr, 0.0, 0.8, 0.0, 1.0); // 绿色
                break;
            case Status::ERROR:
                cairo_set_source_rgba(cr, 0.9, 0.1, 0.1, 1.0); // 红色
                break;
        }

        cairo_fill_preserve(cr);

        // 绘制边框
        cairo_set_source_rgba(cr, 0.3, 0.3, 0.3, 1.0);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);

        return FALSE;
    }

    // 创建菜单栏
    void create_menu_bar() {
        menubar = gtk_menu_bar_new();

        // 文件菜单
        GtkWidget *file_menu = gtk_menu_new();
        GtkWidget *file_menu_item = gtk_menu_item_new_with_label("文件");

        GtkWidget *open_item = gtk_menu_item_new_with_label("打开");
        GtkWidget *exit_item = gtk_menu_item_new_with_label("退出");

        gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), exit_item);

        gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), file_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_menu_item);

        // 工具菜单
        GtkWidget *tool_menu = gtk_menu_new();
        GtkWidget *tool_menu_item = gtk_menu_item_new_with_label("工具");

        GtkWidget *mount_item = gtk_menu_item_new_with_label("挂载分区");
        GtkWidget *unmount_item = gtk_menu_item_new_with_label("卸载分区");
        GtkWidget *clear_cache_item = gtk_menu_item_new_with_label("清除缓存");

        gtk_menu_shell_append(GTK_MENU_SHELL(tool_menu), mount_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(tool_menu), unmount_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(tool_menu), clear_cache_item);

        gtk_menu_item_set_submenu(GTK_MENU_ITEM(tool_menu_item), tool_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(menubar), tool_menu_item);

        // 帮助菜单
        GtkWidget *help_menu = gtk_menu_new();
        GtkWidget *help_menu_item = gtk_menu_item_new_with_label("帮助");

        GtkWidget *instructions_item = gtk_menu_item_new_with_label("操作说明");
        GtkWidget *about_item = gtk_menu_item_new_with_label("关于");

        gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), instructions_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_item);

        gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_menu_item), help_menu);
        gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_menu_item);

        // 添加到布局
        gtk_box_pack_start(GTK_BOX(main_vbox), menubar, FALSE, FALSE, 0);

        // 连接信号
        g_signal_connect(open_item, "activate", G_CALLBACK(on_open_clicked), this);
        g_signal_connect(exit_item, "activate", G_CALLBACK(gtk_main_quit), NULL);
        g_signal_connect(mount_item, "activate", G_CALLBACK(on_mount_clicked), this);
        g_signal_connect(unmount_item, "activate", G_CALLBACK(on_unmount_clicked), this);
        g_signal_connect(clear_cache_item, "activate", G_CALLBACK(on_clear_cache_clicked), this);
        g_signal_connect(instructions_item, "activate", G_CALLBACK(on_instructions_clicked), this);
        g_signal_connect(about_item, "activate", G_CALLBACK(on_about_clicked), NULL);
    }

    // 显示文件选择对话框
    void show_file_chooser_dialog() {
        GtkWidget *dialog = gtk_file_chooser_dialog_new(
            "选择 Super 镜像文件",
            GTK_WINDOW(window),
            GTK_FILE_CHOOSER_ACTION_OPEN,
            "_取消", GTK_RESPONSE_CANCEL,
            "_打开", GTK_RESPONSE_ACCEPT,
            NULL
        );

        gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(dialog), create_file_filter());

        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
            char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
            if (filename) {
                gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_chooser_button), filename);
                g_free(filename);
            }
        }

        gtk_widget_destroy(dialog);
    }

    // 创建文件过滤器
    GtkFileFilter* create_file_filter() {
        GtkFileFilter *filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, "Android Super Image files (*.img)");
        gtk_file_filter_add_pattern(filter, "*.img");
        return filter;
    }

    // 创建工作目录（如果不存在）
    void ensure_directory_exists(const std::string &dir) {
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
            log_message("创建目录: " + dir);
        }
    }

    // 获取程序所在目录
    std::string get_program_directory() {
        char exe_path[1024];
        ssize_t count = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (count != -1) {
            exe_path[count] = '\0';
            fs::path path(exe_path);
            return path.parent_path().string();
        }
        // 如果失败，返回当前目录
        return ".";
    }

    // 创建所有工作目录
    void create_working_dirs() {
        ensure_directory_exists(IMAGES_DIR);
        ensure_directory_exists(MOUNT_DIR);
        ensure_directory_exists(UNPACK_DIR);
    }

    // 获取当前时间字符串
    std::string get_current_time() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_time;
        localtime_r(&now_time, &local_time);

        std::ostringstream oss;
        oss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    // 写日志
    void log_message(const std::string &message) {
        std::lock_guard<std::mutex> lock(log_mutex);
        std::ofstream log_file(LOG_FILE, std::ios::app);
        if (log_file.is_open()) {
            log_file << "[" << get_current_time() << "] " << message << std::endl;
            log_file.close();
        }
    }

    // 执行命令并获取输出
    std::string execute_command(const std::string &command) {
        char buffer[128];
        std::string result = "";
        FILE *pipe = popen(command.c_str(), "r");
        if (!pipe) return "错误: 无法执行命令";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        return result;
    }

    // 检查挂载点是否已挂载
    bool is_mounted() {
        std::string command = "mountpoint \"" + MOUNT_DIR + "\"";
        FILE *pipe = popen(command.c_str(), "r");
        int exit_code = pclose(pipe);
        return (exit_code == 0);
    }

    // 在主线程中更新状态
    void update_status_in_main(const std::string &status, Status light_status = Status::WORKING) {
        auto *data = new std::tuple<MSuperGUI*, std::string, Status>(this, status, light_status);
        g_idle_add((GSourceFunc)update_status_callback, data);
    }

    // 更新状态标签
    void update_status(const std::string &status, Status light_status) {
        gtk_label_set_text(GTK_LABEL(status_label), status.c_str());
        set_status(light_status);
    }

    // 重置所有控件到初始状态
    void reset_controls() {
        // 重置文件选择器
        gtk_file_chooser_unselect_all(GTK_FILE_CHOOSER(file_chooser_button));

        // 清空分区列表
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(partition_combo));
        partition_images.clear();

        // 重置状态标签
        gtk_label_set_text(GTK_LABEL(status_label), "就绪");
        set_status(Status::READY);

        // 清空文件名缓存
        input_img.clear();
        ext4_img.clear();
    }

    // 清除缓存（子线程）
    void clear_cache_thread() {
        log_message("开始清除缓存");
        update_status_in_main("正在清除缓存...", Status::WORKING);

        // 确保工作目录存在
        ensure_directory_exists(IMAGES_DIR);
        ensure_directory_exists(MOUNT_DIR);
        ensure_directory_exists(UNPACK_DIR);

        // 清除 IMAGES_DIR 目录下的所有文件和子目录内容
        if (fs::exists(IMAGES_DIR)) {
            for (const auto &entry : fs::directory_iterator(IMAGES_DIR)) {
                if (entry.path() != MOUNT_DIR && entry.path() != UNPACK_DIR) {
                    // 不是 mount 和 unpacked 目录，直接删除
                    fs::remove_all(entry.path());
                    log_message("删除: " + entry.path().string());
                }
            }
        }

        // 清除 UNPACK_DIR 目录下的所有内容
        if (fs::exists(UNPACK_DIR)) {
            for (const auto &entry : fs::directory_iterator(UNPACK_DIR)) {
                fs::remove_all(entry.path());
                log_message("删除: " + entry.path().string());
            }
        }

        // 再次确保目录存在
        ensure_directory_exists(IMAGES_DIR);
        ensure_directory_exists(MOUNT_DIR);
        ensure_directory_exists(UNPACK_DIR);

        // 在主线程中重置控件
        g_idle_add((GSourceFunc)reset_controls_callback, this);

        log_message("缓存清除完成");
        update_status_in_main("缓存清除完成", Status::SUCCESS);
    }

    // 处理镜像文件（子线程）
    void process_image_thread() {
        log_message("开始处理镜像");
        update_status_in_main("正在处理镜像...", Status::WORKING);

        // 确保工作目录存在
        ensure_directory_exists(IMAGES_DIR);
        ensure_directory_exists(MOUNT_DIR);
        ensure_directory_exists(UNPACK_DIR);

        // 获取选择的文件
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_chooser_button));
        if (!filename) {
            log_message("错误: 未选择文件");
            update_status_in_main("错误: 请选择一个文件", Status::ERROR);
            return;
        }
        input_img = filename;
        g_free(filename);

        if (input_img.empty()) {
            log_message("错误: 文件路径为空");
            update_status_in_main("错误: 请选择一个文件", Status::ERROR);
            return;
        }

        log_message("选择的文件: " + input_img);
        update_status_in_main("正在处理镜像...", Status::WORKING);

        // 获取文件名
        fs::path img_path(input_img);
        std::string img_filename = img_path.filename().string();
        ext4_img = IMAGES_DIR + "/" + img_filename + "_ext4";

        // 步骤 1: 使用 simg2img 转换
        update_status_in_main("步骤 1: 转换镜像...", Status::WORKING);
        if (!fs::exists(ext4_img)) {
            log_message("转换镜像: " + input_img + " -> " + ext4_img);
            std::string command = SIMG2IMG + " \"" + input_img + "\" \"" + ext4_img + "\"";
            execute_command(command);
            // 检查转换后的文件是否存在
            if (!fs::exists(ext4_img)) {
                log_message("错误: simg2img 转换失败");
                update_status_in_main("错误: simg2img 转换失败", Status::ERROR);
                return;
            }
            log_message("转换完成");
        } else {
            log_message("镜像已存在，跳过转换: " + ext4_img);
            update_status_in_main("步骤 1: 镜像已存在，跳过转换", Status::WORKING);
        }

        // 步骤 2: 使用 lpunpack 解包
        update_status_in_main("步骤 2: 解包镜像...", Status::WORKING);
        log_message("解包镜像到: " + UNPACK_DIR);
        // 清空旧的解包目录
        for (const auto &entry : fs::directory_iterator(UNPACK_DIR)) {
            fs::remove_all(entry.path());
        }

        std::string command = LPUNPACK + " \"" + ext4_img + "\" \"" + UNPACK_DIR + "\"";
        execute_command(command);
        log_message("解包完成");

        // 步骤 3: 列出可用分区
        update_status_in_main("步骤 3: 列出可用分区...", Status::WORKING);
        partition_images.clear();

        // 在主线程中更新组合框
        g_idle_add((GSourceFunc)update_partition_combo_callback, this);

        log_message("镜像处理完成");
        update_status_in_main("处理完成，请选择要挂载的分区", Status::SUCCESS);
    }

    // 挂载所选分区（子线程）
    void mount_partition_thread() {
        log_message("开始挂载分区");
        ensure_directory_exists(MOUNT_DIR);

        int active_index = gtk_combo_box_get_active(GTK_COMBO_BOX(partition_combo));
        if (active_index < 0 || active_index >= (int)partition_images.size()) {
            log_message("错误: 未选择分区");
            update_status_in_main("错误: 请选择一个分区", Status::ERROR);
            return;
        }

        std::string selected_img = partition_images[active_index];
        fs::path img_path(selected_img);
        std::string selected_name = img_path.filename().string();

        log_message("选择的分区: " + selected_name);
        update_status_in_main("正在挂载分区: " + selected_name, Status::WORKING);

        // 检查挂载点是否已挂载
        if (is_mounted()) {
            log_message("卸载已有挂载");
            update_status_in_main("卸载已有挂载...", Status::WORKING);
            std::string unmount_cmd = "pkexec umount \"" + MOUNT_DIR + "\"";
            execute_command(unmount_cmd);
            // 等待一下
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // 使用 pkexec 挂载分区（会弹出密码对话框）
        std::string mount_command = "pkexec mount -o ro,loop \"" + selected_img + "\" \"" + MOUNT_DIR + "\"";
        log_message("执行挂载命令");
        update_status_in_main("执行挂载命令，请在弹出的对话框中输入密码...", Status::WORKING);

        execute_command(mount_command);

        // 检查是否挂载成功
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        if (is_mounted()) {
            log_message("挂载成功，打开挂载目录: " + MOUNT_DIR);
            update_status_in_main("挂载成功！正在打开挂载目录...", Status::SUCCESS);

            // 打开挂载目录
            execute_command("xdg-open \"" + MOUNT_DIR + "\"");

            update_status_in_main("完成！挂载目录: " + MOUNT_DIR, Status::SUCCESS);
        } else {
            log_message("错误: 挂载失败");
            update_status_in_main("错误: 挂载失败。可能需要输入密码。", Status::ERROR);
        }
    }

    // 卸载分区（子线程）
    void unmount_partition_thread() {
        log_message("开始卸载分区");
        update_status_in_main("正在卸载分区...", Status::WORKING);

        // 检查挂载点是否已挂载
        if (is_mounted()) {
            // 已挂载，需要卸载
            log_message("执行卸载命令");
            std::string unmount_cmd = "pkexec umount \"" + MOUNT_DIR + "\"";
            execute_command(unmount_cmd);

            // 等待一下再检查
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            if (!is_mounted()) {
                log_message("卸载成功");
                update_status_in_main("卸载成功", Status::SUCCESS);
            } else {
                log_message("错误: 卸载失败");
                update_status_in_main("错误: 卸载失败", Status::ERROR);
            }
        } else {
            log_message("挂载点未挂载");
            update_status_in_main("挂载点未挂载", Status::READY);
        }
    }

    // 回调函数
    static void on_destroy(GtkWidget *widget, gpointer data) {
        (void)widget;
        MSuperGUI *gui = static_cast<MSuperGUI*>(data);
        gui->log_message("===== 程序退出 =====");
        if (gui->breathe_timeout_id > 0) {
            g_source_remove(gui->breathe_timeout_id);
            gui->breathe_timeout_id = 0;
        }
        gtk_main_quit();
    }

    static void on_process_clicked(GtkWidget *widget, gpointer data) {
        (void)widget;
        MSuperGUI *gui = static_cast<MSuperGUI*>(data);
        // 在子线程中处理镜像
        std::thread t(&MSuperGUI::process_image_thread, gui);
        t.detach();
    }

    static void on_mount_clicked(GtkWidget *widget, gpointer data) {
        (void)widget;
        MSuperGUI *gui = static_cast<MSuperGUI*>(data);
        // 在子线程中挂载分区
        std::thread t(&MSuperGUI::mount_partition_thread, gui);
        t.detach();
    }

    static void on_unmount_clicked(GtkWidget *widget, gpointer data) {
        (void)widget;
        MSuperGUI *gui = static_cast<MSuperGUI*>(data);
        // 在子线程中卸载分区
        std::thread t(&MSuperGUI::unmount_partition_thread, gui);
        t.detach();
    }

    static void on_open_clicked(GtkWidget *widget, gpointer data) {
        (void)widget;
        MSuperGUI *gui = static_cast<MSuperGUI*>(data);
        gui->show_file_chooser_dialog();
    }

    static void on_clear_cache_clicked(GtkWidget *widget, gpointer data) {
        (void)widget;
        MSuperGUI *gui = static_cast<MSuperGUI*>(data);
        gui->log_message("用户点击清除缓存");

        // 首先检查分区是否已挂载
        if (gui->is_mounted()) {
            gui->log_message("检测到分区已挂载，提示用户确认");
            // 弹出警告对话框
            GtkWidget *dialog = gtk_message_dialog_new(
                GTK_WINDOW(gui->window),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_YES_NO,
                "警告：当前有分区已挂载！\n\n"
                "清除缓存前需要先卸载分区。\n"
                "是否继续卸载并清除缓存？"
            );
            gtk_window_set_title(GTK_WINDOW(dialog), "确认清除缓存");

            gint response = gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);

            if (response != GTK_RESPONSE_YES) {
                gui->log_message("用户取消清除缓存");
                // 用户取消
                return;
            }

            // 先卸载分区
            gui->log_message("先卸载分区");
            std::thread unmount_t(&MSuperGUI::unmount_partition_thread, gui);
            unmount_t.detach();

            // 等待卸载完成
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        } else {
            // 确认是否清除缓存
            GtkWidget *dialog = gtk_message_dialog_new(
                GTK_WINDOW(gui->window),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_QUESTION,
                GTK_BUTTONS_YES_NO,
                "确定要清除缓存吗？\n\n"
                "这将删除 ~/.local/images/ 目录下的所有文件：\n"
                "  • 转换后的镜像文件\n"
                "  • 解包的分区文件\n"
                "  • 其他所有缓存文件"
            );
            gtk_window_set_title(GTK_WINDOW(dialog), "确认清除缓存");

            gint response = gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);

            if (response != GTK_RESPONSE_YES) {
                gui->log_message("用户取消清除缓存");
                // 用户取消
                return;
            }
        }

        // 在子线程中清除缓存
        std::thread t(&MSuperGUI::clear_cache_thread, gui);
        t.detach();
    }

    // 回调函数：重置控件
    static gboolean reset_controls_callback(gpointer data) {
        MSuperGUI *gui = static_cast<MSuperGUI*>(data);
        gui->reset_controls();
        return FALSE;
    }

    static void on_instructions_clicked(GtkWidget *widget, gpointer data) {
        (void)widget;
        MSuperGUI *gui = static_cast<MSuperGUI*>(data);

        const char *instructions_text =
            "<b>操作说明</b>\n\n"
            "<b>步骤 1: 选择 Super 镜像文件</b>\n"
            "点击\"选择 Super 镜像文件\"按钮或使用菜单栏的\"文件 -> 打开\"，选择要处理的 super.img 文件。\n\n"
            "<b>步骤 2: 处理镜像</b>\n"
            "点击\"处理镜像\"按钮，程序会：\n"
            "  • 使用 simg2img 转换稀疏镜像\n"
            "  • 使用 lpunpack 解包出各个分区\n"
            "  • 在下拉列表中显示可用分区\n\n"
            "<b>步骤 3: 选择分区</b>\n"
            "从\"选择分区\"下拉列表中选择想要挂载的分区镜像。\n\n"
            "<b>步骤 4: 挂载分区</b>\n"
            "点击\"挂载所选分区\"按钮：\n"
            "  • 会弹出密码输入对话框\n"
            "  • 输入管理员密码确认\n"
            "  • 挂载成功后会自动打开文件管理器\n\n"
            "<b>卸载分区</b>\n"
            "使用完毕后，点击\"卸载分区\"按钮卸载已挂载的分区。\n\n"
            "<b>清除缓存</b>\n"
            "使用\"工具 -> 清除缓存\"可以清理：\n"
            "  • 转换后的镜像文件 (*_ext4)\n"
            "  • 解包的分区文件\n"
            "注意：清除缓存前会检查是否有分区已挂载，如有需要先卸载。\n\n"
            "<b>工作目录</b>\n"
            "  • 镜像处理: ~/.local/images/\n"
            "  • 解包目录: ~/.local/images/unpacked/\n"
            "  • 挂载点: ~/.local/images/mount/";

        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(gui->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            NULL
        );
        gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialog), instructions_text);
        gtk_window_set_title(GTK_WINDOW(dialog), "操作说明");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    static void on_about_clicked(GtkWidget *widget, gpointer data) {
        (void)widget;
        (void)data;
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
                                                 "Android Super Image Tool\nVersion 2.0\n\n用于处理Android Super Image文件");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }

    // 回调函数：更新状态
    static gboolean update_status_callback(gpointer data) {
        auto *tuple = static_cast<std::tuple<MSuperGUI*, std::string, Status>*>(data);
        MSuperGUI *gui = std::get<0>(*tuple);
        std::string status = std::get<1>(*tuple);
        Status light_status = std::get<2>(*tuple);
        gui->update_status(status, light_status);
        delete tuple;
        return FALSE;
    }

    // 回调函数：更新分区组合框
    static gboolean update_partition_combo_callback(gpointer data) {
        MSuperGUI *gui = static_cast<MSuperGUI*>(data);
        gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(gui->partition_combo));
        gui->partition_images.clear();

        for (const auto &entry : fs::directory_iterator(gui->UNPACK_DIR)) {
            if (entry.path().extension() == ".img") {
                std::string partition_name = entry.path().filename().string();
                gui->partition_images.push_back(entry.path().string());
                gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(gui->partition_combo), NULL, partition_name.c_str());
            }
        }

        if (!gui->partition_images.empty()) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(gui->partition_combo), 0);
        }

        return FALSE;
    }
};

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    MSuperGUI gui;
    gui.run();
    return 0;
}
