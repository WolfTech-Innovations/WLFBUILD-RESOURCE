#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <signal.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <limits.h>
#include <termios.h>

// Android-inspired UI constants
#define STATUS_BAR_HEIGHT 30
#define NAV_BAR_HEIGHT 50
#define APP_ICON_SIZE 80
#define APP_ICON_SPACING 20
#define APP_NAME_HEIGHT 20
#define NOTIFICATION_HEIGHT 60
#define QUICK_SETTINGS_HEIGHT 200
#define WINDOW_RADIUS 8

// Colors (Android-inspired Material Design colors)
#define COLOR_PRIMARY 0x3F51B5       // Indigo 500
#define COLOR_PRIMARY_DARK 0x303F9F  // Indigo 700
#define COLOR_ACCENT 0xFF4081        // Pink A200
#define COLOR_BACKGROUND 0xFAFAFA    // Light gray
#define COLOR_CARD 0xFFFFFF          // White
#define COLOR_TEXT_PRIMARY 0x212121  // Nearly black
#define COLOR_TEXT_SECONDARY 0x757575// Dark gray
#define COLOR_DIVIDER 0xDDDDDD       // Light gray
#define COLOR_STATUS_BAR 0x000000    // Black with 80% opacity
#define COLOR_NAV_BAR 0x000000       // Black

// Additional UI colors
#define COLOR_GREEN 0x4CAF50
#define COLOR_RED 0xF44336
#define COLOR_BLUE 0x2196F3
#define COLOR_YELLOW 0xFFEB3B
#define COLOR_TRANSPARENT 0x00000000

// Maximum number of apps to display
#define MAX_APPS 36
#define MAX_APP_NAME_LENGTH 64

// Maximum number of notification items
#define MAX_NOTIFICATIONS 10

// Maximum number of commands to display
#define MAX_COMMANDS 100
#define MAX_COMMAND_LENGTH 64

// Animation constants
#define ANIM_DURATION_MS 300
#define ANIM_FRAMES 30

// UI States
typedef enum {
    STATE_HOME,
    STATE_APP_DRAWER,
    STATE_NOTIFICATIONS,
    STATE_QUICK_SETTINGS,
    STATE_APP_RUNNING
} UIState;

// Notification structure
typedef struct {
    char title[64];
    char message[256];
    unsigned int color;
    int is_read;
    time_t timestamp;
} Notification;

// App structure
typedef struct {
    char name[MAX_APP_NAME_LENGTH];
    char exec_path[PATH_MAX];
    unsigned int icon_color;  // Placeholder for icon
    char category[32];
    int is_system_app;
} App;

// Window structure
typedef struct {
    int id;
    int x, y, width, height;
    char *title;
    int is_focused;
    int is_maximized;
    unsigned char *buffer; // Window's own buffer
    int zorder;            // Z-order for layering
} ManagedWindow;

// Session manager structure
typedef struct {
    int session_active;
    ManagedWindow windows[100];
    int window_count;
} SessionManager;

// Font data structure - simple bitmap font
typedef struct {
    unsigned char data[256][8]; // Simple 8x8 bitmap font
    int width;
    int height;
} Font;

// Animation structure
typedef struct {
    float progress;  // 0.0 to 1.0
    int is_active;
    int start_time;
    int duration;
    int type;        // 0=fade, 1=slide
    int direction;   // For slide: 0=left, 1=right, 2=up, 3=down
} Animation;

// Framebuffer data
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
int fb_fd;
unsigned char *fb_ptr;
int screen_width, screen_height;
int bytes_per_pixel;
int line_length;

// Double buffering
unsigned char *back_buffer = NULL;

// Input devices
int mouse_fd = -1;
int kbd_fd = -1;
int touch_fd = -1;

// UI state
UIState current_state = STATE_HOME;
Animation current_animation;
int notifications_open = 0;
int quick_settings_open = 0;
int app_drawer_open = 0;

// Mouse/touch state
int pointer_x = 0;
int pointer_y = 0;
int pointer_down = 0;
int pointer_was_down = 0;
int drag_start_x = 0;
int drag_start_y = 0;
int is_dragging = 0;

// Apps and commands
App app_list[MAX_APPS];
int app_count = 0;
int selected_app = -1;
char command_list[MAX_COMMANDS][MAX_COMMAND_LENGTH];
int command_count = 0;

// Notification system
Notification notifications[MAX_NOTIFICATIONS];
int notification_count = 0;

// Session manager
SessionManager session;

// Font
Font main_font;

// Function prototypes
void init_system();
void init_framebuffer();
void init_input_devices();
void init_font();
void init_apps();
void cleanup();

// Drawing functions
void clear_screen(unsigned int color);
void swap_buffers();
void draw_pixel(int x, int y, unsigned int color);
void draw_rect(int x, int y, int width, int height, unsigned int color);
void draw_rounded_rect(int x, int y, int width, int height, int radius, unsigned int color);
void draw_circle(int x, int y, int radius, unsigned int color);
void draw_char(int x, int y, char c, unsigned int color);
void draw_text(int x, int y, const char *text, unsigned int color);
void draw_text_centered(int x, int y, int width, const char *text, unsigned int color);
void draw_icon(int x, int y, int size, unsigned int color);
void draw_app_icon(int x, int y, const App *app);
void draw_status_bar();
void draw_navigation_bar();
void draw_home_screen();
void draw_app_drawer();
void draw_notifications();
void draw_quick_settings();
void draw_running_app();
void draw_ui();

// Animation functions
void start_animation(int type, int direction, int duration_ms);
void update_animation();
float ease_in_out_quad(float t);

// Event handling
void* input_thread(void *arg);
void handle_touch_event(int x, int y, int down);
void handle_key_event(int key_code, int down);
void launch_app(const App *app);
void handle_back_button();
void handle_home_button();
void handle_recents_button();

// System functions from WolfOS
void handle_sigint(int sig);
void display_splash();
void animate_loading();
void fancy_print(const char* text, unsigned int color1, unsigned int color2, int delay_ms);
void show_progress_bar(const char* text, int duration_ms);
void print_system_info();
int check_internet_connection();
void refresh_app_list();
void execute_selected_command(const char* command, const char* args);

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    // Just print a message and continue
    printf("\nOperation interrupted. K9 Android Desktop continues running...\n");
    signal(SIGINT, handle_sigint); // Re-register handler
}

int main() {
    // Set up signal handling
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    
    // Initialize the system
    init_system();
    
    // Main loop
    while (1) {
        update_animation();
        draw_ui();
        swap_buffers();
        usleep(16667); // ~60 FPS (1000000/60)
    }
    
    // Cleanup (never reached in this simple example)
    cleanup();
    return 0;
}

// Initialize the system
void init_system() {
    // Seed random number generator
    srand(time(NULL));
    
    // Display the splash screen (text-mode version for now)
    display_splash();
    
    // Show fancy loading animations
    animate_loading();
    
    // Initialize framebuffer
    init_framebuffer();
    
    // Initialize input devices
    init_input_devices();
    
    // Initialize font
    init_font();
    
    // Create back buffer for double buffering
    back_buffer = malloc(screen_width * screen_height * bytes_per_pixel);
    if (!back_buffer) {
        fprintf(stderr, "Failed to allocate back buffer\n");
        exit(1);
    }
    
    // Initialize apps
    init_apps();
    
    // Create session
    session.session_active = 1;
    session.window_count = 0;
    
    // Create input thread
    pthread_t input_tid;
    if (pthread_create(&input_tid, NULL, input_thread, NULL) != 0) {
        fprintf(stderr, "Failed to create input thread\n");
        cleanup();
        exit(1);
    }
    
    // Add a welcome notification
    if (notification_count < MAX_NOTIFICATIONS) {
        strcpy(notifications[notification_count].title, "Welcome");
        strcpy(notifications[notification_count].message, "K9 Android Desktop is ready");
        notifications[notification_count].color = COLOR_PRIMARY;
        notifications[notification_count].is_read = 0;
        notifications[notification_count].timestamp = time(NULL);
        notification_count++;
    }
    
    // Start with a welcome animation
    start_animation(0, 0, ANIM_DURATION_MS);
    
    // Print system info
    print_system_info();
    
    // Check internet connection
    check_internet_connection();
}

// Initialize the framebuffer
void init_framebuffer() {
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        perror("Error opening framebuffer device");
        exit(1);
    }

    // Get fixed screen information
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("Error reading fixed screen info");
        close(fb_fd);
        exit(1);
    }

    // Get variable screen information
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("Error reading variable screen info");
        close(fb_fd);
        exit(1);
    }

    screen_width = vinfo.xres;
    screen_height = vinfo.yres;
    bytes_per_pixel = vinfo.bits_per_pixel / 8;
    line_length = finfo.line_length;

    // Map the device to memory
    fb_ptr = (unsigned char *)mmap(0, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((long)fb_ptr == -1) {
        perror("Error mapping framebuffer to memory");
        close(fb_fd);
        exit(1);
    }

    printf("Framebuffer initialized: %dx%d, %d bpp\n", screen_width, screen_height, vinfo.bits_per_pixel);
}

// Initialize input devices
void init_input_devices() {
    // Try to open touch input first
    touch_fd = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
    
    // Fall back to mouse if touch not available
    if (touch_fd == -1) {
        mouse_fd = open("/dev/input/mice", O_RDONLY | O_NONBLOCK);
        if (mouse_fd == -1) {
            perror("Error opening mouse device");
            // Not fatal, continue without mouse
        }
    }

    // Open keyboard
    kbd_fd = open("/dev/input/event1", O_RDONLY | O_NONBLOCK);
    if (kbd_fd == -1) {
        // Try another keyboard device
        kbd_fd = open("/dev/input/event2", O_RDONLY | O_NONBLOCK);
        if (kbd_fd == -1) {
            perror("Error opening keyboard device");
            // Not fatal, continue without keyboard
        }
    }
}

// Load simple bitmap font
void init_font() {
    main_font.width = 8;
    main_font.height = 8;
    memset(main_font.data, 0, sizeof(main_font.data));
    
    // Define basic character set (simplified for example)
    // This would normally load from a font file
    
    // Define letter 'K'
    main_font.data['K'][0] = 0b10001000;
    main_font.data['K'][1] = 0b10010000;
    main_font.data['K'][2] = 0b10100000;
    main_font.data['K'][3] = 0b11000000;
    main_font.data['K'][4] = 0b10100000;
    main_font.data['K'][5] = 0b10010000;
    main_font.data['K'][6] = 0b10001000;
    main_font.data['K'][7] = 0b00000000;
    
    // Define digit '9'
    main_font.data['9'][0] = 0b01110000;
    main_font.data['9'][1] = 0b10001000;
    main_font.data['9'][2] = 0b10001000;
    main_font.data['9'][3] = 0b01111000;
    main_font.data['9'][4] = 0b00001000;
    main_font.data['9'][5] = 0b00010000;
    main_font.data['9'][6] = 0b01100000;
    main_font.data['9'][7] = 0b00000000;
    
    // Additional characters would be defined here
    // For simplicity, we're not defining the entire character set
}

// Initialize app list
void init_apps() {
    // Clear app list
    app_count = 0;
    
    // Add system apps
    strcpy(app_list[app_count].name, "Settings");
    strcpy(app_list[app_count].exec_path, "settings");
    app_list[app_count].icon_color = COLOR_BLUE;
    strcpy(app_list[app_count].category, "System");
    app_list[app_count].is_system_app = 1;
    app_count++;
    
    strcpy(app_list[app_count].name, "Calculator");
    strcpy(app_list[app_count].exec_path, "calculator");
    app_list[app_count].icon_color = COLOR_RED;
    strcpy(app_list[app_count].category, "Utility");
    app_list[app_count].is_system_app = 1;
    app_count++;
    
    strcpy(app_list[app_count].name, "Clock");
    strcpy(app_list[app_count].exec_path, "clock");
    app_list[app_count].icon_color = COLOR_GREEN;
    strcpy(app_list[app_count].category, "Utility");
    app_list[app_count].is_system_app = 1;
    app_count++;
    
    strcpy(app_list[app_count].name, "Files");
    strcpy(app_list[app_count].exec_path, "files");
    app_list[app_count].icon_color = COLOR_YELLOW;
    strcpy(app_list[app_count].category, "Utility");
    app_list[app_count].is_system_app = 1;
    app_count++;
    
    strcpy(app_list[app_count].name, "Terminal");
    strcpy(app_list[app_count].exec_path, "terminal");
    app_list[app_count].icon_color = COLOR_STATUS_BAR;
    strcpy(app_list[app_count].category, "System");
    app_list[app_count].is_system_app = 1;
    app_count++;
    
    // Add applications from /bin and /usr/bin
    refresh_app_list();
}

// Clean up resources
void cleanup() {
    // Free window resources
    for (int i = 0; i < session.window_count; i++) {
        if (session.windows[i].title) free(session.windows[i].title);
        if (session.windows[i].buffer) free(session.windows[i].buffer);
    }
    
    // Free back buffer
    if (back_buffer) free(back_buffer);
    
    // Unmap framebuffer
    munmap(fb_ptr, finfo.smem_len);
    close(fb_fd);
    
    // Close input devices
    if (mouse_fd >= 0) close(mouse_fd);
    if (kbd_fd >= 0) close(kbd_fd);
    if (touch_fd >= 0) close(touch_fd);
}

// Clear the screen with a specified color
void clear_screen(unsigned int color) {
    for (int y = 0; y < screen_height; y++) {
        for (int x = 0; x < screen_width; x++) {
            draw_pixel(x, y, color);
        }
    }
}

// Swap the back buffer to the screen
void swap_buffers() {
    memcpy(fb_ptr, back_buffer, screen_width * screen_height * bytes_per_pixel);
}

// Draw a single pixel (to back buffer)
void draw_pixel(int x, int y, unsigned int color) {
    if (x < 0 || x >= screen_width || y < 0 || y >= screen_height)
        return;

    long location = (x + vinfo.xoffset) * bytes_per_pixel + 
                   (y + vinfo.yoffset) * (screen_width * bytes_per_pixel);

    if (bytes_per_pixel == 4) {
        back_buffer[location] = color & 0xFF;         // Blue
        back_buffer[location + 1] = (color >> 8) & 0xFF;  // Green
        back_buffer[location + 2] = (color >> 16) & 0xFF; // Red
        back_buffer[location + 3] = (color >> 24) & 0xFF; // Alpha (ignored)
    } else if (bytes_per_pixel == 2) {
        // 16-bit color implementation
        unsigned short pixel = ((color & 0xF80000) >> 8) | 
                              ((color & 0x00FC00) >> 5) | 
                              ((color & 0x0000F8) >> 3);
        *((unsigned short*)(back_buffer + location)) = pixel;
    }
}

// Draw a filled rectangle
void draw_rect(int x, int y, int width, int height, unsigned int color) {
    for (int j = y; j < y + height && j < screen_height; j++) {
        for (int i = x; i < x + width && i < screen_width; i++) {
            if (i >= 0 && j >= 0) {
                draw_pixel(i, j, color);
            }
        }
    }
}

// Draw a character from the bitmap font
void draw_char(int x, int y, char c, unsigned int color) {
    for (int j = 0; j < main_font.height; j++) {
        unsigned char line = main_font.data[(unsigned char)c][j];
        for (int i = 0; i < main_font.width; i++) {
            if (line & (0x80 >> i)) {
                draw_pixel(x + i, y + j, color);
            }
        }
    }
}

// Draw text using the bitmap font
void draw_text(int x, int y, const char *text, unsigned int color) {
    int pos_x = x;
    while (*text) {
        draw_char(pos_x, y, *text, color);
        text++;
        pos_x += main_font.width;
    }
}

// Draw text centered within a width
void draw_text_centered(int x, int y, int width, const char *text, unsigned int color) {
    int text_width = strlen(text) * main_font.width;
    int start_x = x + (width - text_width) / 2;
    draw_text(start_x, y, text, color);
}

// Draw rounded rectangle
void draw_rounded_rect(int x, int y, int width, int height, int radius, unsigned int color) {
    // Draw the central rectangle
    draw_rect(x + radius, y, width - 2 * radius, height, color);
    draw_rect(x, y + radius, width, height - 2 * radius, color);
    
    // Draw the four corners
    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            if (i*i + j*j <= radius*radius) {
                // Top-left corner
                draw_pixel(x + radius + i, y + radius + j, color);
                // Top-right corner
                draw_pixel(x + width - radius + i, y + radius + j, color);
                // Bottom-left corner
                draw_pixel(x + radius + i, y + height - radius + j, color);
                // Bottom-right corner
                draw_pixel(x + width - radius + i, y + height - radius + j, color);
            }
        }
    }
}

// Draw a circle
void draw_circle(int x, int y, int radius, unsigned int color) {
    for (int i = -radius; i <= radius; i++) {
        for (int j = -radius; j <= radius; j++) {
            if (i*i + j*j <= radius*radius) {
                draw_pixel(x + i, y + j, color);
            }
        }
    }
}

// Draw a simple app icon
void draw_app_icon(int x, int y, const App *app) {
    // Draw a rounded rectangle for the icon background
    draw_rounded_rect(x, y, APP_ICON_SIZE, APP_ICON_SIZE, WINDOW_RADIUS, app->icon_color);
    
    // Draw the first letter of the app name as the icon
    char first_letter = app->name[0];
    draw_char(x + APP_ICON_SIZE/2 - main_font.width/2, 
              y + APP_ICON_SIZE/2 - main_font.height/2, 
              first_letter, COLOR_CARD);
    
    // Draw the app name below the icon
    draw_text_centered(x, y + APP_ICON_SIZE + 5, APP_ICON_SIZE, app->name, COLOR_TEXT_PRIMARY);
}

// Draw status bar (top of screen)
void draw_status_bar() {
    // Draw status bar background
    draw_rect(0, 0, screen_width, STATUS_BAR_HEIGHT, COLOR_STATUS_BAR);
    
    // Draw time in top-right corner
    char time_str[9];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(time_str, sizeof(time_str), "%H:%M", timeinfo);
    draw_text(screen_width - strlen(time_str) * main_font.width - 10, 
              (STATUS_BAR_HEIGHT - main_font.height) / 2, 
              time_str, COLOR_CARD);
    
    // Draw battery icon (simple rectangle)
    draw_rect(screen_width - 60, (STATUS_BAR_HEIGHT - 10) / 2, 20, 10, COLOR_CARD);
    draw_rect(screen_width - 40, (STATUS_BAR_HEIGHT - 6) / 2, 3, 6, COLOR_CARD);
    
    // Draw wifi icon (simple triangle)
    int wifi_x = screen_width - 90;
    int wifi_y = STATUS_BAR_HEIGHT / 2;
    draw_pixel(wifi_x, wifi_y - 5, COLOR_CARD);
    draw_pixel(wifi_x - 1, wifi_y - 4, COLOR_CARD);
    draw_pixel(wifi_x + 1, wifi_y - 4, COLOR_CARD);
    draw_pixel(wifi_x - 2, wifi_y - 3, COLOR_CARD);
    draw_pixel(wifi_x + 2, wifi_y - 3, COLOR_CARD);
    draw_pixel(wifi_x - 3, wifi_y - 2, COLOR_CARD);
    draw_pixel(wifi_x + 3, wifi_y - 2, COLOR_CARD);
    draw_pixel(wifi_x - 4, wifi_y - 1, COLOR_CARD);
    draw_pixel(wifi_x + 4, wifi_y - 1, COLOR_CARD);
    
    // Draw notification count if there are unread notifications
    int unread_count = 0;
    for (int i = 0; i < notification_count; i++) {
        if (!notifications[i].is_read) unread_count++;
    }
    
    if (unread_count > 0) {
        char count_str[4];
        snprintf(count_str, sizeof(count_str), "%d", unread_count);
        draw_circle(20, STATUS_BAR_HEIGHT / 2, 8, COLOR_RED);
        draw_text(20 - (strlen(count_str) * main_font.width) / 2, 
                 STATUS_BAR_HEIGHT / 2 - main_font.height / 2, 
                 count_str, COLOR_CARD);
    }
}

// Draw navigation bar (bottom of screen)
void draw_navigation_bar() {
    // Draw nav bar background
    draw_rect(0, screen_height - NAV_BAR_HEIGHT, screen_width, NAV_BAR_HEIGHT, COLOR_NAV_BAR);
    
    // Draw back button (triangle)
    int back_x = screen_width / 6;
    int back_y = screen_height - NAV_BAR_HEIGHT / 2;
    for (int i = 0; i < 10; i++) {
        draw_rect(back_x - i, back_y - i, 1, i * 2 + 1, COLOR_CARD);
    }
    
    // Draw home button (circle)
    int home_x = screen_width / 2;
    int home_y = screen_height - NAV_BAR_HEIGHT / 2;
    draw_circle(home_x, home_y, 10, COLOR_CARD);
    
    // Draw recents button (square)
    int recents_x = screen_width * 5 / 6;
    int recents_y = screen_height - NAV_BAR_HEIGHT / 2;
    draw_rect(recents_x - 10, recents_y - 10, 20, 20, COLOR_CARD);
}

// Draw the home screen
void draw_home_screen() {
    // Draw home screen background
    draw_rect(0, STATUS_BAR_HEIGHT, screen_width, 
              screen_height - STATUS_BAR_HEIGHT - NAV_BAR_HEIGHT, 
              COLOR_BACKGROUND);
    
    // Draw home screen wallpaper (a simple gradient)
    for (int y = STATUS_BAR_HEIGHT; y < screen_height - NAV_BAR_HEIGHT; y++) {
        unsigned int color = COLOR_PRIMARY_DARK - 
                            ((y - STATUS_BAR_HEIGHT) * 0x001000 / 
                            (screen_height - STATUS_BAR_HEIGHT - NAV_BAR_HEIGHT));
        draw_rect(0, y, screen_width, 1, color);
    }
    
    // Draw dock background
    draw_rounded_rect(10, screen_height - NAV_BAR_HEIGHT - 90, 
                     screen_width - 20, 80, 20, COLOR_CARD);
    
    // Draw dock apps (4 most used apps)
    int dock_app_count = app_count > 4 ? 4 : app_count;
    int dock_width = screen_width - 40;
    int app_spacing = dock_width / dock_app_count;
    
    for (int i = 0; i < dock_app_count; i++) {
        int app_x = 20 + i * app_spacing + (app_spacing - APP_ICON_SIZE) / 2;
        int app_y = screen_height - NAV_BAR_HEIGHT - 80;
        draw_app_icon(app_x, app_y, &app_list[i]);
    }
    
    // Draw app grid on home screen (2x2 grid)
    int grid_app_count = app_count > 8 ? 8 : app_count;
    int grid_app_per_row = 4;
    int grid_x_offset = (screen_width - (grid_app_per_row * (APP_ICON_SIZE + APP_ICON_SPACING))) / 2;
    int grid_y_offset = STATUS_BAR_HEIGHT + 40;
    
    for (int i = 4; i < grid_app_count; i++) {
        int row = (i - 4) / grid_app_per_row;
        int col = (i - 4) % grid_app_per_row;
        int app_x = grid_x_offset + col * (APP_ICON_SIZE + APP_ICON_SPACING);
        int app_y = grid_y_offset + row * (APP_ICON_SIZE + APP_ICON_SPACING + APP_NAME_HEIGHT);
        draw_app_icon(app_x, app_y, &app_list[i]);
    }
    
    // Draw K9 branding on home screen
    draw_text_centered(0, screen_height - NAV_BAR_HEIGHT - 150, 
                      screen_width, "K9 ANDROID DESKTOP", COLOR_CARD);
}

// Draw the app drawer
void draw_app_drawer() {
    // Draw app drawer background (semi-transparent overlay)
    draw_rect(0, STATUS_BAR_HEIGHT, screen_width, 
             screen_height - STATUS_BAR_HEIGHT - NAV_BAR_HEIGHT, 
             COLOR_STATUS_BAR);
    
    // Draw app drawer header
    draw_rect(0, STATUS_BAR_HEIGHT, screen_width, 40, COLOR_PRIMARY);
    draw_text_centered(0, STATUS_BAR_HEIGHT + 16, 
                     screen_width, "All Apps", COLOR_CARD);
    
    // Draw app grid
    int grid_app_per_row = 4;
    int grid_x_offset = (screen_width - (grid_app_per_row * (APP_ICON_SIZE + APP_ICON_SPACING))) / 2;
    int grid_y_offset = STATUS_BAR_HEIGHT + 80;
    
    for (int i = 0; i < app_count; i++) {
        int row = i / grid_app_per_row;
        int col = i % grid_app_per_row;
        int app_x = grid_x_offset + col * (APP_ICON_SIZE + APP_ICON_SPACING);
        int app_y = grid_y_offset + row * (APP_ICON_SIZE + APP_ICON_SPACING + APP_NAME_HEIGHT);
        
        // If the app would go off screen, stop drawing
        if (app_y + APP_ICON_SIZE > screen_height - NAV_BAR_HEIGHT)
            break;
            
        draw_app_icon(app_x, app_y, &app_list[i]);
        
        // If this is the selected app, draw a highlight
        if (i == selected_app) {
            draw_rounded_rect(app_x - 5, app_y - 5, 
                             APP_ICON_SIZE + 10, APP_ICON_SIZE + APP_NAME_HEIGHT + 10, 
                             WINDOW_RADIUS, COLOR_ACCENT);
        }
    }
}

// Draw notifications panel
void draw_notifications() {
    // Handle animation
    int panel_height = screen_height - STATUS_BAR_HEIGHT - NAV_BAR_HEIGHT;
    int y_offset = 0;
    if (current_animation.is_active) {
        float t = current_animation.progress;
        float ease = ease_in_out_quad(t);
        if (current_animation.direction == 2) { // Sliding down
            y_offset = (int)((1.0 - ease) * panel_height);
        } else if (current_animation.direction == 3) { // Sliding up
            y_offset = (int)(ease * panel_height);
        }
    }
    
    // Draw notifications panel background
    draw_rect(0, STATUS_BAR_HEIGHT - y_offset, screen_width, panel_height, COLOR_CARD);
    
    // Draw notifications header
    draw_rect(0, STATUS_BAR_HEIGHT - y_offset, screen_width, 40, COLOR_PRIMARY);
    draw_text_centered(0, STATUS_BAR_HEIGHT + 16 - y_offset, screen_width, "Notifications", COLOR_CARD);
    
    // Draw notifications
    int notif_y = STATUS_BAR_HEIGHT + 50 - y_offset;
    for (int i = 0; i < notification_count; i++) {
        // Draw notification card
        draw_rounded_rect(10, notif_y, screen_width - 20, NOTIFICATION_HEIGHT, WINDOW_RADIUS, COLOR_BACKGROUND);
        
        // Draw colored bar on left to indicate notification type
        draw_rect(10, notif_y, 5, NOTIFICATION_HEIGHT, notifications[i].color);
        
        // Draw notification title
        draw_text(20, notif_y + 10, notifications[i].title, COLOR_TEXT_PRIMARY);
        
        // Draw notification message (truncated if needed)
        char msg_copy[64]; // Truncated version for display
        strncpy(msg_copy, notifications[i].message, 63);
        msg_copy[63] = '\0';
        draw_text(20, notif_y + 30, msg_copy, COLOR_TEXT_SECONDARY);
        
        // Draw notification time
        char time_str[20];
        struct tm *timeinfo = localtime(&notifications[i].timestamp);
        strftime(time_str, sizeof(time_str), "%H:%M", timeinfo);
        draw_text(screen_width - 70, notif_y + 10, time_str, COLOR_TEXT_SECONDARY);
        
        // Draw clear button
        draw_circle(screen_width - 25, notif_y + NOTIFICATION_HEIGHT/2, 10, COLOR_RED);
        draw_text(screen_width - 29, notif_y + NOTIFICATION_HEIGHT/2 - 4, "X", COLOR_CARD);
        
        notif_y += NOTIFICATION_HEIGHT + 5;
        
        // If we've drawn too many notifications, stop
        if (notif_y > screen_height - NAV_BAR_HEIGHT - 10)
            break;
    }
    
    // Draw "Clear all" button at bottom
    if (notification_count > 0) {
        draw_rounded_rect(screen_width/2 - 50, screen_height - NAV_BAR_HEIGHT - 50 - y_offset, 
                         100, 40, WINDOW_RADIUS, COLOR_PRIMARY);
        draw_text_centered(screen_width/2 - 50, screen_height - NAV_BAR_HEIGHT - 35 - y_offset, 
                         100, "Clear All", COLOR_CARD);
    } else {
        draw_text_centered(0, screen_height/2 - NAV_BAR_HEIGHT/2 - y_offset, 
                         screen_width, "No notifications", COLOR_TEXT_SECONDARY);
    }
}

// Draw quick settings panel
void draw_quick_settings() {
    // Handle animation
    int panel_height = QUICK_SETTINGS_HEIGHT;
    int y_offset = 0;
    if (current_animation.is_active) {
        float t = current_animation.progress;
        float ease = ease_in_out_quad(t);
        if (current_animation.direction == 2) { // Sliding down
            y_offset = (int)((1.0 - ease) * panel_height);
        } else if (current_animation.direction == 3) { // Sliding up
            y_offset = (int)(ease * panel_height);
        }
    }
    
    // Draw quick settings panel background
    draw_rect(0, STATUS_BAR_HEIGHT - y_offset, screen_width, panel_height, COLOR_CARD);
    
    // Draw quick settings grid (2x3)
    int button_width = (screen_width - 60) / 3;
    int button_height = (QUICK_SETTINGS_HEIGHT - 80) / 2;
    
    // WiFi toggle
    draw_rounded_rect(20, STATUS_BAR_HEIGHT + 20 - y_offset, 
                     button_width, button_height, WINDOW_RADIUS, COLOR_BLUE);
    draw_text_centered(20, STATUS_BAR_HEIGHT + button_height/2 - 4 - y_offset, 
                     button_width, "WiFi", COLOR_CARD);
    
    // Bluetooth toggle
    draw_rounded_rect(30 + button_width, STATUS_BAR_HEIGHT + 20 - y_offset, 
                     button_width, button_height, WINDOW_RADIUS, COLOR_PRIMARY);
    draw_text_centered(30 + button_width, STATUS_BAR_HEIGHT + button_height/2 - 4 - y_offset, 
                     button_width, "BT", COLOR_CARD);
    
    // Airplane mode toggle
    draw_rounded_rect(40 + 2*button_width, STATUS_BAR_HEIGHT + 20 - y_offset, 
                     button_width, button_height, WINDOW_RADIUS, COLOR_TEXT_SECONDARY);
    draw_text_centered(40 + 2*button_width, STATUS_BAR_HEIGHT + button_height/2 - 4 - y_offset, 
                     button_width, "Flight", COLOR_CARD);
    
    // Flashlight toggle
    draw_rounded_rect(20, STATUS_BAR_HEIGHT + 30 + button_height - y_offset, 
                     button_width, button_height, WINDOW_RADIUS, COLOR_YELLOW);
    draw_text_centered(20, STATUS_BAR_HEIGHT + 30 + button_height + button_height/2 - 4 - y_offset, 
                     button_width, "Light", COLOR_TEXT_PRIMARY);
    
    // Auto-rotate toggle
    draw_rounded_rect(30 + button_width, STATUS_BAR_HEIGHT + 30 + button_height - y_offset, 
                     button_width, button_height, WINDOW_RADIUS, COLOR_GREEN);
    draw_text_centered(30 + button_width, STATUS_BAR_HEIGHT + 30 + button_height + button_height/2 - 4 - y_offset, 
                     button_width, "Rotate", COLOR_CARD);
    
    // Battery saver toggle
    draw_rounded_rect(40 + 2*button_width, STATUS_BAR_HEIGHT + 30 + button_height - y_offset, 
                     button_width, button_height, WINDOW_RADIUS, COLOR_RED);
    draw_text_centered(40 + 2*button_width, STATUS_BAR_HEIGHT + 30 + button_height + button_height/2 - 4 - y_offset, 
                     button_width, "Saver", COLOR_CARD);
}

// Render UI for running app
void draw_running_app() {
    // Draw app window
    draw_rect(0, STATUS_BAR_HEIGHT, screen_width, 
             screen_height - STATUS_BAR_HEIGHT - NAV_BAR_HEIGHT, 
             COLOR_BACKGROUND);
    
    if (session.window_count > 0) {
        // Find focused window
        int focused_idx = -1;
        for (int i = 0; i < session.window_count; i++) {
            if (session.windows[i].is_focused) {
                focused_idx = i;
                break;
            }
        }
        
        if (focused_idx >= 0) {
            // Draw window title bar
            draw_rect(0, STATUS_BAR_HEIGHT, screen_width, 30, COLOR_PRIMARY);
            draw_text(10, STATUS_BAR_HEIGHT + 11, session.windows[focused_idx].title, COLOR_CARD);
            
            // Draw close button
            draw_rect(screen_width - 30, STATUS_BAR_HEIGHT, 30, 30, COLOR_RED);
            draw_text(screen_width - 20, STATUS_BAR_HEIGHT + 11, "X", COLOR_CARD);
            
            // Draw app content (placeholder)
            draw_rect(0, STATUS_BAR_HEIGHT + 30, screen_width, 
                     screen_height - STATUS_BAR_HEIGHT - NAV_BAR_HEIGHT - 30, 
                     COLOR_CARD);
            
            draw_text_centered(0, screen_height/2 - 50, screen_width, 
                           "K9", COLOR_PRIMARY);
            draw_text_centered(0, screen_height/2, screen_width, 
                           "App running...", COLOR_TEXT_PRIMARY);
        }
    }
}

// Draw the complete UI based on current state
void draw_ui() {
    // Clear screen
    clear_screen(COLOR_BACKGROUND);
    
    // Draw main UI based on state
    switch (current_state) {
        case STATE_HOME:
            draw_home_screen();
            break;
        case STATE_APP_DRAWER:
            draw_home_screen(); // Draw home screen as background
            draw_app_drawer();
            break;
        case STATE_NOTIFICATIONS:
            draw_home_screen(); // Draw home screen as background
            draw_notifications();
            break;
        case STATE_QUICK_SETTINGS:
            draw_home_screen(); // Draw home screen as background
            draw_quick_settings();
            break;
        case STATE_APP_RUNNING:
            draw_running_app();
            break;
    }
    
    // Always draw status and navigation bars on top
    draw_status_bar();
    draw_navigation_bar();
    
    // Draw pointer for debugging
    if (pointer_down) {
        draw_circle(pointer_x, pointer_y, 5, COLOR_ACCENT);
    } else {
        draw_circle(pointer_x, pointer_y, 3, COLOR_PRIMARY);
    }
}

// Animation easing function
float ease_in_out_quad(float t) {
    return t < 0.5 ? 2 * t * t : 1 - pow(-2 * t + 2, 2) / 2;
}

// Start an animation
void start_animation(int type, int direction, int duration_ms) {
    current_animation.is_active = 1;
    current_animation.progress = 0.0;
    current_animation.start_time = (int)time(NULL) * 1000;
    current_animation.duration = duration_ms;
    current_animation.type = type;
    current_animation.direction = direction;
}

// Update animation progress
void update_animation() {
    if (!current_animation.is_active)
        return;
    
    int current_time = (int)time(NULL) * 1000;
    int elapsed = current_time - current_animation.start_time;
    
    if (elapsed >= current_animation.duration) {
        current_animation.is_active = 0;
        current_animation.progress = 1.0;
    } else {
        current_animation.progress = (float)elapsed / current_animation.duration;
    }
}

// Input thread
void* input_thread(void *arg) {
    unsigned char buffer[3];
    struct input_event ev;
    int bytes_read;
    
    while (1) {
        // Try to read from mouse if available
        if (mouse_fd >= 0) {
            bytes_read = read(mouse_fd, buffer, sizeof(buffer));
            if (bytes_read == 3) {
                int btn_left = buffer[0] & 0x1;
                int dx = (char)buffer[1];
                int dy = (char)buffer[2];
                
                pointer_was_down = pointer_down;
                pointer_down = btn_left;
                pointer_x += dx;
                pointer_y += dy;
                
                // Keep pointer within screen bounds
                if (pointer_x < 0) pointer_x = 0;
                if (pointer_x >= screen_width) pointer_x = screen_width - 1;
                if (pointer_y < 0) pointer_y = 0;
                if (pointer_y >= screen_height) pointer_y = screen_height - 1;
                
                // Handle mouse click events
                if (pointer_down && !pointer_was_down) {
                    handle_touch_event(pointer_x, pointer_y, 1);
                } else if (!pointer_down && pointer_was_down) {
                    handle_touch_event(pointer_x, pointer_y, 0);
                }
            }
        }
        
        // Try to read from touch if available
        if (touch_fd >= 0) {
            bytes_read = read(touch_fd, &ev, sizeof(struct input_event));
            if (bytes_read == sizeof(struct input_event)) {
                if (ev.type == EV_ABS) {
                    if (ev.code == ABS_X)
                        pointer_x = ev.value;
                    else if (ev.code == ABS_Y)
                        pointer_y = ev.value;
                } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                    pointer_was_down = pointer_down;
                    pointer_down = ev.value;
                    
                    if (pointer_down && !pointer_was_down) {
                        handle_touch_event(pointer_x, pointer_y, 1);
                    } else if (!pointer_down && pointer_was_down) {
                        handle_touch_event(pointer_x, pointer_y, 0);
                    }
                }
            }
        }
        
        // Try to read from keyboard if available
        if (kbd_fd >= 0) {
            bytes_read = read(kbd_fd, &ev, sizeof(struct input_event));
            if (bytes_read == sizeof(struct input_event)) {
                if (ev.type == EV_KEY) {
                    handle_key_event(ev.code, ev.value);
                }
            }
        }
        
        usleep(5000); // Poll every 5ms
    }
    
    return NULL;
}

// Handle touch/click events
void handle_touch_event(int x, int y, int down) {
    if (down) {
        // Touch down - register start of potential drag
        drag_start_x = x;
        drag_start_y = y;
        is_dragging = 0;
        
        // Check nav bar button presses
        if (y >= screen_height - NAV_BAR_HEIGHT) {
            int nav_btn_width = screen_width / 3;
            if (x < nav_btn_width) {
                // Back button pressed
                handle_back_button();
                return;
            } else if (x >= nav_btn_width && x < nav_btn_width * 2) {
                // Home button pressed
                handle_home_button();
                return;
            } else {
                // Recents button pressed
                handle_recents_button();
                return;
            }
        }
        
        // Check status bar
        if (y < STATUS_BAR_HEIGHT) {
            // Check for notification pull-down
            if (x < screen_width / 2) {
                if (!notifications_open) {
                    notifications_open = 1;
                    current_state = STATE_NOTIFICATIONS;
                    start_animation(1, 2, ANIM_DURATION_MS);
                } else {
                    notifications_open = 0;
                    current_state = STATE_HOME;
                    start_animation(1, 3, ANIM_DURATION_MS);
                }
            } else {
                // Quick settings
                if (!quick_settings_open) {
                    quick_settings_open = 1;
                    current_state = STATE_QUICK_SETTINGS;
                    start_animation(1, 2, ANIM_DURATION_MS);
                } else {
                    quick_settings_open = 0;
                    current_state = STATE_HOME;
                    start_animation(1, 3, ANIM_DURATION_MS);
                }
            }
            return;
        }
        
        // Check for app drawer button
        if (current_state == STATE_HOME && y > screen_height - NAV_BAR_HEIGHT - 90 && y < screen_height - NAV_BAR_HEIGHT) {
            app_drawer_open = 1;
            current_state = STATE_APP_DRAWER;
            start_animation(0, 0, ANIM_DURATION_MS);
            return;
        }
        
        // Check for app selection in app drawer
        if (current_state == STATE_APP_DRAWER) {
            int grid_app_per_row = 4;
            int grid_x_offset = (screen_width - (grid_app_per_row * (APP_ICON_SIZE + APP_ICON_SPACING))) / 2;
            int grid_y_offset = STATUS_BAR_HEIGHT + 80;
            
            for (int i = 0; i < app_count; i++) {
                int row = i / grid_app_per_row;
                int col = i % grid_app_per_row;
                int app_x = grid_x_offset + col * (APP_ICON_SIZE + APP_ICON_SPACING);
                int app_y = grid_y_offset + row * (APP_ICON_SIZE + APP_ICON_SPACING + APP_NAME_HEIGHT);
                
                if (x >= app_x && x < app_x + APP_ICON_SIZE &&
                    y >= app_y && y < app_y + APP_ICON_SIZE) {
                    selected_app = i;
                    return;
                }
            }
        }
        
        // Handle notification interactions
        if (current_state == STATE_NOTIFICATIONS) {
            // Check for notification clear buttons
            int notif_y = STATUS_BAR_HEIGHT + 50;
            for (int i = 0; i < notification_count; i++) {
                if (x >= screen_width - 35 && x <= screen_width - 15 &&
                    y >= notif_y + NOTIFICATION_HEIGHT/2 - 10 && 
                    y <= notif_y + NOTIFICATION_HEIGHT/2 + 10) {
                    // Remove this notification
                    for (int j = i; j < notification_count - 1; j++) {
                        notifications[j] = notifications[j+1];
                    }
                    notification_count--;
                    return;
                }
                notif_y += NOTIFICATION_HEIGHT + 5;
            }
            
            // Check clear all button
            if (notification_count > 0 && 
                x >= screen_width/2 - 50 && x <= screen_width/2 + 50 &&
                y >= screen_height - NAV_BAR_HEIGHT - 50 && y <= screen_height - NAV_BAR_HEIGHT - 10) {
                notification_count = 0;
                return;
            }
        }
        
        // Handle quick settings interactions
        if (current_state == STATE_QUICK_SETTINGS) {
            int button_width = (screen_width - 60) / 3;
            int button_height = (QUICK_SETTINGS_HEIGHT - 80) / 2;
            
            // Handle toggle buttons (just visual feedback for now)
            // WiFi toggle
            if (x >= 20 && x <= 20 + button_width &&
                y >= STATUS_BAR_HEIGHT + 20 && y <= STATUS_BAR_HEIGHT + 20 + button_height) {
                // Toggle WiFi (just visual for now)
                return;
            }
            
            // Other toggle buttons would be handled similarly
        }
        
    } else {
        // Touch up - check if dragging
        int dx = x - drag_start_x;
        int dy = y - drag_start_y;
        
        if (abs(dx) > 50 || abs(dy) > 50) {
            // This was a drag
            is_dragging = 1;
            
            // Handle vertical swipes
            if (abs(dy) > abs(dx)) {
                if (dy < -50) {
                    // Swipe up
                    if (current_state == STATE_HOME || current_state == STATE_APP_RUNNING) {
                        // Open app drawer
                        app_drawer_open = 1;
                        current_state = STATE_APP_DRAWER;
                        start_animation(0, 3, ANIM_DURATION_MS);
                    }
                } else if (dy > 50) {
                    // Swipe down
                    if (current_state == STATE_HOME || current_state == STATE_APP_RUNNING) {
                        // Open notifications
                        notifications_open = 1;
                        current_state = STATE_NOTIFICATIONS;
                        start_animation(1, 2, ANIM_DURATION_MS);
                    }
                }
            }
            // Handle horizontal swipes as needed
        }
        
        // If not dragging and in app drawer, check for app launch
        if (!is_dragging && current_state == STATE_APP_DRAWER && selected_app >= 0) {
            launch_app(&app_list[selected_app]);
            selected_app = -1;
            app_drawer_open = 0;
            current_state = STATE_APP_RUNNING;
        }
    }
}

// Handle keyboard events
void handle_key_event(int key_code, int down) {
    if (!down) return; // Only handle key press, not release
    
    switch (key_code) {
        case KEY_ESC:
            handle_back_button();
            break;
        case KEY_HOME:
            handle_home_button();
            break;
        case KEY_TAB:
            handle_recents_button();
            break;
        // Add more keyboard shortcuts
    }
}

// Handle back button press
void handle_back_button() {
    switch (current_state) {
        case STATE_APP_DRAWER:
            app_drawer_open = 0;
            current_state = STATE_HOME;
            start_animation(0, 0, ANIM_DURATION_MS);
            break;
        case STATE_NOTIFICATIONS:
            notifications_open = 0;
            current_state = STATE_HOME;
            start_animation(1, 3, ANIM_DURATION_MS);
            break;
        case STATE_QUICK_SETTINGS:
            quick_settings_open = 0;
            current_state = STATE_HOME;
            start_animation(1, 3, ANIM_DURATION_MS);
            break;
        case STATE_APP_RUNNING:
            current_state = STATE_HOME;
            // Close the running app
            for (int i = 0; i < session.window_count; i++) {
                if (session.windows[i].is_focused) {
                    // Close window
                    if (session.windows[i].title) free(session.windows[i].title);
                    if (session.windows[i].buffer) free(session.windows[i].buffer);
                    // Shift other windows up
                    for (int j = i; j < session.window_count - 1; j++) {
                        session.windows[j] = session.windows[j+1];
                    }
                    session.window_count--;
                    break;
                }
            }
            break;
        default:
            break;
    }
}

// Handle home button press
void handle_home_button() {
    current_state = STATE_HOME;
    app_drawer_open = 0;
    notifications_open = 0;
    quick_settings_open = 0;
}

// Handle recents button press
void handle_recents_button() {
    // Toggle app drawer for now
    if (current_state != STATE_APP_DRAWER) {
        app_drawer_open = 1;
        current_state = STATE_APP_DRAWER;
    } else {
        app_drawer_open = 0;
        current_state = STATE_HOME;
    }
}

// Launch app
void launch_app(const App *app) {
    // Create a window for the app
    if (session.window_count < 100) {
        ManagedWindow win;
        win.id = session.window_count;
        win.x = 0;
        win.y = STATUS_BAR_HEIGHT + 30;
        win.width = screen_width;
        win.height = screen_height - STATUS_BAR_HEIGHT - NAV_BAR_HEIGHT - 30;
        win.title = strdup(app->name);
        win.is_focused = 1;
        win.is_maximized = 1;
        win.zorder = session.window_count;
        win.buffer = malloc(win.width * win.height * bytes_per_pixel);
        
        // Set any existing window as unfocused
        for (int i = 0; i < session.window_count; i++) {
            session.windows[i].is_focused = 0;
        }
        
        session.windows[session.window_count++] = win;
    }
    
    // Add a notification about the launched app
    if (notification_count < MAX_NOTIFICATIONS) {
        sprintf(notifications[notification_count].title, "App Launched");
        sprintf(notifications[notification_count].message, "K9 launched %s", app->name);
        notifications[notification_count].color = COLOR_GREEN;
        notifications[notification_count].is_read = 0;
        notifications[notification_count].timestamp = time(NULL);
        notification_count++;
    }
}

// Display splash screen (text-based version)
void display_splash() {
    system("clear");
    printf("\n\n");
    printf("    ██╗  ██╗ █████╗    \n");
    printf("    ██║ ██╔╝██╔══██╗   \n");
    printf("    █████╔╝ ╚██████║   \n");
    printf("    ██╔═██╗  ╚═══██║   \n");
    printf("    ██║  ██╗ █████╔╝   \n");
    printf("    ╚═╝  ╚═╝ ╚════╝    \n");
    printf("\n");
    printf("    K9 System Loading...\n");
    printf("\n\n");
    
    // Wait a bit for effect
    usleep(500000);
}

// Show animated loading sequence
void animate_loading() {
    printf("Initializing systems ");
    for (int i = 0; i < 20; i++) {
        printf(".");
        fflush(stdout);
        usleep(100000);
    }
    printf(" Done!\n");
    
    // Show progress bar
    show_progress_bar("Loading modules", 1500000);
    
    printf("Starting K9 desktop environment...\n");
    usleep(500000);
}

// Display a fancy text message with gradient colors
void fancy_print(const char* text, unsigned int color1, unsigned int color2, int delay_ms) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        // Calculate color blend
        float t = (float)i / len;
        unsigned int r1 = (color1 >> 16) & 0xFF;
        unsigned int g1 = (color1 >> 8) & 0xFF;
        unsigned int b1 = color1 & 0xFF;
        unsigned int r2 = (color2 >> 16) & 0xFF;
        unsigned int g2 = (color2 >> 8) & 0xFF;
        unsigned int b2 = color2 & 0xFF;
        
        unsigned int r = r1 + (r2 - r1) * t;
        unsigned int g = g1 + (g2 - g1) * t;
        unsigned int b = b1 + (b2 - b1) * t;
        
        // Print character with color
        printf("\033[38;2;%u;%u;%um%c\033[0m", r, g, b, text[i]);
        fflush(stdout);
        usleep(delay_ms);
    }
    printf("\n");
}

// Show progress bar with text
void show_progress_bar(const char* text, int duration_ms) {
    const int bar_width = 40;
    printf("%s: [", text);
    for (int i = 0; i <= bar_width; i++) {
        printf("\r%s: [", text);
        for (int j = 0; j < i; j++) printf("=");
        for (int j = i; j < bar_width; j++) printf(" ");
        printf("] %d%%", (i * 100) / bar_width);
        fflush(stdout);
        usleep(duration_ms / bar_width);
    }
    printf("\n");
}

// Print system information
void print_system_info() {
    printf("K9 System Information:\n");
    printf("- Display: %dx%d, %d bpp\n", screen_width, screen_height, vinfo.bits_per_pixel);
    printf("- Version: 1.0 Beta\n");
    printf("- Available apps: %d\n", app_count);
    
    // Get system memory info
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo) {
        char line[256];
        unsigned long mem_total = 0, mem_free = 0;
        
        while (fgets(line, sizeof(line), meminfo)) {
            if (strncmp(line, "MemTotal:", 9) == 0)
                sscanf(line, "MemTotal: %lu", &mem_total);
            else if (strncmp(line, "MemFree:", 8) == 0)
                sscanf(line, "MemFree: %lu", &mem_free);
        }
        fclose(meminfo);
        
        if (mem_total > 0)
            printf("- Memory: %lu MB total, %lu MB free\n", mem_total/1024, mem_free/1024);
    }
    
    // CPU info
    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo) {
        char line[256];
        int cpu_count = 0;
        char cpu_model[256] = "Unknown";
        
        while (fgets(line, sizeof(line), cpuinfo)) {
            if (strncmp(line, "processor", 9) == 0)
                cpu_count++;
            else if (strncmp(line, "model name", 10) == 0) {
                char *p = strchr(line, ':');
                if (p && cpu_count == 1) { // Only store first CPU model
                    strncpy(cpu_model, p+2, sizeof(cpu_model)-1);
                    // Remove newline if present
                    char *nl = strchr(cpu_model, '\n');
                    if (nl) *nl = '\0';
                }
            }
        }
        fclose(cpuinfo);
        
        printf("- CPU: %s (%d cores)\n", cpu_model, cpu_count);
    }
}

// Check internet connection
int check_internet_connection() {
    // Simplified connectivity check
    FILE *ping = popen("ping -c 1 -W 1 8.8.8.8 > /dev/null 2>&1; echo $?", "r");
    if (!ping) return 0;
    
    char result[10];
    if (fgets(result, sizeof(result), ping) != NULL) {
        pclose(ping);
        return (result[0] == '0');
    }
    
    pclose(ping);
    return 0;
}

// Refresh app list by scanning /bin and /usr/bin
void refresh_app_list() {
    // Keep system apps (first 5)
    int sys_apps = app_count > 5 ? 5 : app_count;
    app_count = sys_apps;
    
    // Directories to scan
    const char *dirs[] = {"/bin", "/usr/bin"};
    
    // Get list of common executables in path
    for (int d = 0; d < 2 && app_count < MAX_APPS; d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL && app_count < MAX_APPS) {
            // Skip hidden files and directories
            if (entry->d_name[0] == '.') continue;
            
            // Check if file is executable
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "%s/%s", dirs[d], entry->d_name);
            
            struct stat st;
            if (stat(path, &st) == 0 && (st.st_mode & S_IXUSR)) {
                // Check if already in list
                int duplicate = 0;
                for (int i = 0; i < app_count; i++) {
                    if (strcmp(app_list[i].name, entry->d_name) == 0) {
                        duplicate = 1;
                        break;
                    }
                }
                
                if (!duplicate) {
                    // Add to app list
                    strncpy(app_list[app_count].name, entry->d_name, MAX_APP_NAME_LENGTH-1);
                    app_list[app_count].name[MAX_APP_NAME_LENGTH-1] = '\0';
                    
                    // Capitalize first letter for display
                    if (app_list[app_count].name[0] >= 'a' && app_list[app_count].name[0] <= 'z')
                        app_list[app_count].name[0] -= 32;
                    
                    strncpy(app_list[app_count].exec_path, path, PATH_MAX-1);
                    app_list[app_count].exec_path[PATH_MAX-1] = '\0';
                    
                    // Determine icon color based on name hash
                    unsigned int hash = 0;
                    for (int i = 0; app_list[app_count].name[i]; i++) {
                        hash = hash * 31 + app_list[app_count].name[i];
                    }
                    
                    // Use hash to select color from a small set
                    unsigned int colors[] = {COLOR_BLUE, COLOR_RED, COLOR_GREEN, COLOR_YELLOW, COLOR_PRIMARY};
                    app_list[app_count].icon_color = colors[hash % 5];
                    
                    strcpy(app_list[app_count].category, "Application");
                    app_list[app_count].is_system_app = 0;
                    
                    app_count++;
                }
            }
        }
        
        closedir(dir);
    }
    
    printf("Found %d applications\n", app_count);
}

// Execute a command
void execute_selected_command(const char* command, const char* args) {
    // Fork process to execute command
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        if (args) {
            execlp(command, command, args, NULL);
        } else {
            execlp(command, command, NULL);
        }
        
        // If exec fails
        perror("Command execution failed");
        exit(1);
    } else if (pid < 0) {
        // Fork failed
        perror("Failed to fork process");
    }
    
    // Parent continues, no need to wait
}

// Main entry point - implemented earlier
// int main() { ... }
