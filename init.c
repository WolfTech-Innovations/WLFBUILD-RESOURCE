#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <signal.h>
#include <limits.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

// Color definitions
#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define BLACK   "\033[30m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define WHITE   "\033[37m"

// Maximum number of commands to display
#define MAX_COMMANDS 100
#define MAX_COMMAND_LENGTH 64

// Function prototypes
void handle_sigint(int sig);
void check_root_permissions();
void display_splash();
void animate_loading();
void fancy_print(const char* text, const char* color1, const char* color2, int delay_ms);
void show_progress_bar(const char* text, int duration_ms);
void print_system_info();
int check_internet_connection();
void refresh_command_list();
void draw_tui(int selected_index, int start_idx);
void execute_selected_command(const char* command, const char* args);
int get_terminal_width();
int get_terminal_height();

// Global variables
char command_list[MAX_COMMANDS][MAX_COMMAND_LENGTH];
int command_count = 0;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    fancy_print("\nOperation interrupted. WolfOS continues running...", RED, YELLOW, 10);
    signal(SIGINT, handle_sigint); // Re-register handler
}

int main() {
    // Seed random number generator
    srand(time(NULL));
    
    // Set up signal handling
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);
    
    // Check root permissions
    check_root_permissions();
    
    // Display the splash screen
    display_splash();
    
    // Show fancy loading animations
    animate_loading();
    fancy_print("Configuring base system", BLUE, MAGENTA, 15);
    show_progress_bar("Setting up minimal environment", 600);
    fancy_print("Hello $USER", BLUE, MAGENTA, 15);
    show_progress_bar("Welcome to WolfOS . . .", 600);
    
    // Print system specs
    print_system_info();
    
    // Check for internet connectivity
    int internet_available = check_internet_connection();
    if (!internet_available) {
        fancy_print("No internet connection available. Running in offline mode.", BLUE, YELLOW, 15);
    }
    
    // Final message
    printf("\n%s%s╔════════════════════════════════════════════════════════════╗%s\n", BOLD, BLUE, RESET);
    printf("%s%s║ %sWolfOS is ready                                           %s║%s\n", BOLD, BLUE, GREEN, BLUE, RESET);
    printf("%s%s╚══════════════════════════════════════════════════════════════╝%s\n\n", BOLD, BLUE, RESET);
    
    // Initial command list refresh
    refresh_command_list();
    
    // Main TUI loop
    int selected_index = 0;
    int start_idx = 0;
    char args[1024] = "";
    
    struct termios orig_termios, raw_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw_termios = orig_termios;
    raw_termios.c_lflag &= ~(ICANON | ECHO);
    
    while (1) {
        // Get terminal dimensions
        int term_height = get_terminal_height();
        int visible_commands = term_height - 10; // Reserve space for header and input
        
        if (visible_commands < 3) visible_commands = 3;
        
        // Clear screen
        printf("\033[2J\033[H");
        
        // Draw the TUI
        draw_tui(selected_index, start_idx);
        
        // Show command and args input
        printf("\n%s%sCommand:%s %s\n", BOLD, GREEN, RESET, command_list[selected_index]);
        printf("%s%sArguments:%s %s", BOLD, CYAN, RESET, args);
        printf("\n\n%s%s[↑/↓] Navigate  [Enter] Execute  [Tab] Edit args  [R] Refresh list  [Q] Quit to restart%s", BOLD, YELLOW, RESET);
        
        // Set terminal to raw mode for key capture
        tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios);
        
        // Get key press
        char key = getchar();
        
        // Restore terminal
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
        
        // Process key press
        switch (key) {
            case 'q':
            case 'Q':
                // Restart system (don't actually exit)
                fancy_print("Restarting WolfOS...", BLUE, CYAN, 15);
                refresh_command_list();
                selected_index = 0;
                start_idx = 0;
                memset(args, 0, sizeof(args));
                break;
                
            case 'r':
            case 'R':
                // Refresh command list
                fancy_print("Refreshing command list...", BLUE, CYAN, 15);
                refresh_command_list();
                selected_index = 0;
                start_idx = 0;
                break;
                
            case '\t':
                // Edit arguments
                printf("\n%s%sEnter arguments:%s ", BOLD, CYAN, RESET);
                tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
                fflush(stdin);
                if (fgets(args, sizeof(args), stdin) != NULL) {
                    args[strcspn(args, "\n")] = 0; // Remove newline
                }
                break;
                
            case '\n':
            case '\r':
                // Execute selected command
                execute_selected_command(command_list[selected_index], args);
                printf("\n%s%sPress any key to continue...%s", BOLD, YELLOW, RESET);
                tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios);
                getchar();
                break;
                
            case 65: // Up arrow
                selected_index--;
                if (selected_index < 0) selected_index = command_count - 1;
                
                // Adjust view if needed
                if (selected_index < start_idx) 
                    start_idx = selected_index;
                if (selected_index >= start_idx + visible_commands) 
                    start_idx = selected_index - visible_commands + 1;
                break;
                
            case 66: // Down arrow
                selected_index++;
                if (selected_index >= command_count) selected_index = 0;
                
                // Adjust view if needed
                if (selected_index < start_idx) 
                    start_idx = selected_index;
                if (selected_index >= start_idx + visible_commands) 
                    start_idx = selected_index - visible_commands + 1;
                break;
        }
    }
    
    // We should never reach here due to infinite loop
    return 0;
}

// Refresh the list of commands from /bin
void refresh_command_list() {
    command_count = 0;
    DIR *dir;
    struct dirent *entry;
    
    // Add some built-in commands first
    strcpy(command_list[command_count++], "refresh");
    strcpy(command_list[command_count++], "sysinfo");
    strcpy(command_list[command_count++], "clear");
    strcpy(command_list[command_count++], "restart");
    
    // Open /bin directory
    dir = opendir("/bin");
    if (dir != NULL) {
        // Iterate through all entries
        while ((entry = readdir(dir)) != NULL && command_count < MAX_COMMANDS) {
            // Skip . and .. entries
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            
            // Skip hidden files
            if (entry->d_name[0] == '.')
                continue;
                
            // Add to command list
            strncpy(command_list[command_count], entry->d_name, MAX_COMMAND_LENGTH - 1);
            command_list[command_count][MAX_COMMAND_LENGTH - 1] = '\0'; // Ensure null termination
            command_count++;
        }
        
        closedir(dir);
    }
    
    // Try /usr/bin also if we have space
    dir = opendir("/usr/bin");
    if (dir != NULL) {
        // Iterate through all entries
        while ((entry = readdir(dir)) != NULL && command_count < MAX_COMMANDS) {
            // Skip . and .. entries
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            
            // Skip hidden files
            if (entry->d_name[0] == '.')
                continue;
            
            // Check if command already exists in our list
            int exists = 0;
            for (int i = 0; i < command_count; i++) {
                if (strcmp(command_list[i], entry->d_name) == 0) {
                    exists = 1;
                    break;
                }
            }
            
            // Add to command list if not a duplicate
            if (!exists) {
                strncpy(command_list[command_count], entry->d_name, MAX_COMMAND_LENGTH - 1);
                command_list[command_count][MAX_COMMAND_LENGTH - 1] = '\0'; // Ensure null termination
                command_count++;
            }
        }
        
        closedir(dir);
    }
    
    // Sort the commands alphabetically
    for (int i = 0; i < command_count - 1; i++) {
        for (int j = i + 1; j < command_count; j++) {
            if (strcmp(command_list[i], command_list[j]) > 0) {
                char temp[MAX_COMMAND_LENGTH];
                strcpy(temp, command_list[i]);
                strcpy(command_list[i], command_list[j]);
                strcpy(command_list[j], temp);
            }
        }
    }
}

// Draw the TUI with command list
void draw_tui(int selected_index, int start_idx) {
    int term_width = get_terminal_width();
    int term_height = get_terminal_height();
    int visible_commands = term_height - 10; // Reserve space for header and input
    
    if (visible_commands < 3) visible_commands = 3;
    if (visible_commands > command_count) visible_commands = command_count;
    
    // Draw header
    char header[256];
    snprintf(header, sizeof(header), "WolfOS Command Interface - %d commands available", command_count);
    
    printf("%s%s", BOLD, BLUE);
    for (int i = 0; i < term_width; i++) printf("═");
    printf("%s\n", RESET);
    
    int padding = (int)((term_width + strlen(header)) / 2);
    printf("%s%s%*s%s%s\n", BOLD, GREEN, padding, header, RESET, BOLD);
    
    printf("%s", BLUE);
    for (int i = 0; i < term_width; i++) printf("═");
    printf("%s\n\n", RESET);
    
    // Show command list (paginated)
    int end_idx = start_idx + visible_commands;
    if (end_idx > command_count) end_idx = command_count;
    
    for (int i = start_idx; i < end_idx; i++) {
        if (i == selected_index) {
            printf("%s%s> %s%s%s\n", BOLD, GREEN, YELLOW, command_list[i], RESET);
        } else {
            printf("  %s\n", command_list[i]);
        }
    }
    
    // Show pagination info if needed
    if (command_count > visible_commands) {
        printf("\n%s%sShowing %d-%d of %d commands%s\n", 
               BOLD, BLUE, start_idx + 1, end_idx, command_count, RESET);
    }
}

// Execute the selected command with arguments
void execute_selected_command(const char* command, const char* args) {
    // Handle built-in commands
    if (strcmp(command, "refresh") == 0) {
        fancy_print("Refreshing command list...", BLUE, CYAN, 15);
        refresh_command_list();
        return;
    } else if (strcmp(command, "sysinfo") == 0) {
        print_system_info();
        return;
    } else if (strcmp(command, "clear") == 0) {
        // Just return, the screen will be cleared on next loop
        return;
    } else if (strcmp(command, "restart") == 0) {
        fancy_print("Restarting WolfOS...", BLUE, CYAN, 15);
        sleep(1);
        return;
    }
    
    // For external commands, fork and execute
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        
        // Prepare arguments
        char cmd_path[PATH_MAX];
        
        // Try to find the command in PATH
        if (access(command, X_OK) == 0) {
            // Command is accessible directly
            strncpy(cmd_path, command, sizeof(cmd_path) - 1);
        } else {
            // Try in /bin
            char bin_path[PATH_MAX];
            snprintf(bin_path, sizeof(bin_path), "/bin/%s", command);
            
            if (access(bin_path, X_OK) == 0) {
                // Command is in /bin
                strncpy(cmd_path, bin_path, sizeof(cmd_path) - 1);
            } else {
                // Try in /usr/bin
                char usr_bin_path[PATH_MAX];
                snprintf(usr_bin_path, sizeof(usr_bin_path), "/usr/bin/%s", command);
                
                if (access(usr_bin_path, X_OK) == 0) {
                    // Command is in /usr/bin
                    strncpy(cmd_path, usr_bin_path, sizeof(cmd_path) - 1);
                } else {
                    // Command not found
                    printf("%s%sCommand not found: %s%s\n", BOLD, RED, command, RESET);
                    exit(1);
                }
            }
        }
        
        // Build argument list
        char* argv[64];
        int argc = 0;
        
        // First argument is the command itself
        argv[argc++] = strdup(command);
        
        // Parse additional arguments if provided
        if (strlen(args) > 0) {
            char* args_copy = strdup(args);
            char* token = strtok(args_copy, " ");
            
            while (token != NULL && argc < 63) {
                argv[argc++] = token;
                token = strtok(NULL, " ");
            }
        }
        
        // Null-terminate the argument list
        argv[argc] = NULL;
        
        // Execute the command
        execv(cmd_path, argv);
        
        // If execv fails
        perror("Command execution failed");
        exit(1);
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0) {
                printf("\n%s%sCommand exited with code %d%s\n", BOLD, RED, exit_code, RESET);
            } else {
                printf("\n%s%sCommand completed successfully%s\n", BOLD, GREEN, RESET);
            }
        } else {
            printf("\n%s%sCommand terminated abnormally%s\n", BOLD, RED, RESET);
        }
    } else {
        // Fork failed
        perror("Fork failed");
    }
}

// Get terminal width
int get_terminal_width() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col > 0 ? w.ws_col : 80;
}

// Get terminal height
int get_terminal_height() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_row > 0 ? w.ws_row : 24;
}

// Placeholder for check_root_permissions
void check_root_permissions() {
    if (geteuid() == 0) {
        fancy_print("Running with root permissions", GREEN, YELLOW, 10);
    } else {
        fancy_print("Running with user permissions", YELLOW, GREEN, 10);
    }
}

// Placeholder for display_splash
void display_splash() {
    printf("\n\n%s%s", BOLD, BLUE);
    printf("██╗    ██╗ ██████╗ ██╗     ███████╗ ██████╗ ███████╗\n");
    printf("██║    ██║██╔═══██╗██║     ██╔════╝██╔═══██╗██╔════╝\n");
    printf("██║ █╗ ██║██║   ██║██║     █████╗  ██║   ██║███████╗\n");
    printf("██║███╗██║██║   ██║██║     ██╔══╝  ██║   ██║╚════██║\n");
    printf("╚███╔███╔╝╚██████╔╝███████╗██║     ╚██████╔╝███████║\n");
    printf(" ╚══╝╚══╝  ╚═════╝ ╚══════╝╚═╝      ╚═════╝ ╚══════╝\n");
    printf("%s\n", RESET);
    
    printf("%s%sA Minimalist Command Line Interface%s\n\n", BOLD, CYAN, RESET);
}

// Placeholder for animate_loading
void animate_loading() {
    const char *frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    int frame_count = 10;
    
    printf("%s%sInitializing core components ", BOLD, BLUE);
    for (int i = 0; i < 20; i++) {
        printf("%s%s%s", BOLD, YELLOW, frames[i % frame_count]);
        fflush(stdout);
        usleep(100000); // 100ms
        printf("\b");
    }
    printf("%s✓%s\n", GREEN, RESET);
}

// Placeholder for fancy_print
void fancy_print(const char* text, const char* color1, const char* color2, int delay_ms) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        const char* color = (i % 2 == 0) ? color1 : color2;
        printf("%s%s%c%s", BOLD, color, text[i], RESET);
        fflush(stdout);
        usleep(delay_ms * 1000);
    }
    printf("\n");
}

// Placeholder for show_progress_bar
void show_progress_bar(const char* text, int duration_ms) {
    int width = 40;
    int sleep_per_step = duration_ms / width;
    
    printf("%s%s%s [", BOLD, BLUE, text);
    for (int i = 0; i < width; i++) {
        printf("%s%s#%s", BOLD, CYAN, RESET);
        fflush(stdout);
        usleep(sleep_per_step * 1000);
    }
    printf("%s%s] Done!%s\n", BOLD, GREEN, RESET);
}

// Placeholder for print_system_info
void print_system_info() {
    char hostname[128];
    gethostname(hostname, sizeof(hostname));
    
    printf("\n%s%sSystem Information:%s\n", BOLD, BLUE, RESET);
    printf("%s%s▶ Hostname:%s %s\n", BOLD, GREEN, RESET, hostname);
    
    // Try to get kernel version
    FILE *fp = popen("uname -r", "r");
    if (fp != NULL) {
        char kernel[128];
        if (fgets(kernel, sizeof(kernel), fp) != NULL) {
            kernel[strcspn(kernel, "\n")] = 0;
            printf("%s%s▶ Kernel:%s %s\n", BOLD, GREEN, RESET, kernel);
        }
        pclose(fp);
    }
    
    // Try to get CPU info
    fp = popen("grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2", "r");
    if (fp != NULL) {
        char cpu[256];
        if (fgets(cpu, sizeof(cpu), fp) != NULL) {
            cpu[strcspn(cpu, "\n")] = 0;
            printf("%s%s▶ CPU:%s %s\n", BOLD, GREEN, RESET, cpu);
        }
        pclose(fp);
    }
    
    // Try to get memory info
    fp = popen("grep 'MemTotal' /proc/meminfo | awk '{print $2/1024\" MB\"}'", "r");
    if (fp != NULL) {
        char mem[64];
        if (fgets(mem, sizeof(mem), fp) != NULL) {
            mem[strcspn(mem, "\n")] = 0;
            printf("%s%s▶ Memory:%s %s\n", BOLD, GREEN, RESET, mem);
        }
        pclose(fp);
    }
    
    printf("%s%s▶ Time:%s %ld\n", BOLD, GREEN, RESET, time(NULL));
}

// Placeholder for check_internet_connection
int check_internet_connection() {
    // Try to ping a reliable host (Google's DNS)
    int result = system("ping -c 1 -W 1 8.8.8.8 > /dev/null 2>&1");
    if (result == 0) {
        printf("%s%s▶ Internet:%s Connected\n", BOLD, GREEN, RESET);
        return 1;
    } else {
        printf("%s%s▶ Internet:%s Not connected\n", BOLD, RED, RESET);
        return 0;
    }
}
