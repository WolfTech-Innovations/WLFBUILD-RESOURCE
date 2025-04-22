#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>

#define RESET "\033[0m"
#define BOLD "\033[1m"
#define DIM "\033[2m"
#define ITALIC "\033[3m"
#define UNDERLINE "\033[4m"
#define BLINK "\033[5m"
#define BLACK "\033[30m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"
#define BG_BLACK "\033[40m"
#define BG_RED "\033[41m"
#define BG_GREEN "\033[42m"
#define BG_YELLOW "\033[43m"
#define BG_BLUE "\033[44m"
#define BG_MAGENTA "\033[45m"
#define BG_CYAN "\033[46m"
#define BG_WHITE "\033[47m"

#define MOUNT_POINT "/mnt/wolfos_install"
#define LIVE_PATH "/run/initramfs/memory"
#define GRUB_CONFIG "/boot/grub/grub.cfg"
#define FSTAB_PATH "/etc/fstab"

const char *splash_art[] = {
    "██╗    ██╗ ██████╗ ██╗     ███████╗ ██████╗ ███████╗",
    "██║    ██║██╔═══██╗██║     ██╔════╝██╔═══██╗██╔════╝",
    "██║ █╗ ██║██║   ██║██║     █████╗  ██║   ██║███████╗",
    "██║███╗██║██║   ██║██║     ██╔══╝  ██║   ██║╚════██║",
    "╚███╔███╔╝╚██████╔╝███████╗██║     ╚██████╔╝███████║",
    " ╚══╝╚══╝  ╚═════╝ ╚══════╝╚═╝      ╚═════╝ ╚══════╝",
    NULL
};

// Function prototypes
void fancy_print(const char *text, const char *prefix_color, const char *text_color, int delay_ms);
void log_message(const char *message, const char *color);
void show_progress_bar(const char *task, int duration_ms);
void display_splash();
void handle_sigint(int sig);
void animate_loading();
void print_system_info();
int check_internet_connection();
void prompt_gui_installation();
int execute_command(const char *command);
int execute_command_quiet(const char *command);
int execute_command_with_output(const char *command, char *output, size_t output_size);
void install_to_disk();
void show_main_menu();
char* get_disk_info();
void create_disk_partitions(const char *disk);
void format_partitions(const char *disk);
void mount_partitions(const char *disk);
void copy_system_files();
void install_bootloader(const char *disk);
void configure_system(const char *disk);
void check_root_permissions();
void unmount_partitions();
void generate_fstab(const char *disk);
char* get_uuid(const char *partition);
int disk_has_valid_partitions(const char *disk);
int is_efi_system();
void install_efi_bootloader(const char *disk);
void install_legacy_bootloader(const char *disk);

// Function for fancy text effects
void fancy_print(const char *text, const char *prefix_color, const char *text_color, int delay_ms) {
    printf("%s", prefix_color);
    printf("[ WolfOS ] %s", text_color);
    
    for (size_t i = 0; i < strlen(text); i++) {
        printf("%c", text[i]);
        fflush(stdout);
        usleep(delay_ms * 1000);
    }
    printf("%s\n", RESET);
}

// Log system messages with timestamp and fancy formatting
void log_message(const char *message, const char *color) {
    time_t now;
    struct tm *time_info;
    char timestamp[20];
    
    time(&now);
    time_info = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", time_info);
    
    printf("%s[%s]%s %s%s%s\n", CYAN, timestamp, RESET, color, message, RESET);
}

// Display a progress bar
void show_progress_bar(const char *task, int duration_ms) {
    printf("%s %-30s [", BLUE, task);
    fflush(stdout);
    
    int steps = 20;
    for (int i = 0; i <= steps; i++) {
        printf("%s#", GREEN);
        fflush(stdout);
        usleep((duration_ms / steps) * 1000);
    }
    
    printf("%s] %sCOMPLETE%s\n", BLUE, GREEN, RESET);
}

// Display the splash screen with fancy effects
void display_splash() {
    printf("\033[2J\033[H"); // Clear screen and move cursor to top
    
    // Create a fancy border at the top
    printf("%s╔", YELLOW);
    for (int i = 0; i < 78; i++) printf("═");
    printf("╗%s\n", RESET);
    
    printf("%s║%s%s               -- WolfOS The Minimal Linux --               %s%s║%s\n", 
           YELLOW, RESET, CYAN, RESET, YELLOW, RESET);
    
    printf("%s╠", YELLOW);
    for (int i = 0; i < 78; i++) printf("═");
    printf("╣%s\n\n", RESET);
    
    // Display the splash art with color cycling
    const char *colors[] = {BLUE, MAGENTA, CYAN, GREEN, YELLOW};
    int color_count = 5;
    
    for (int i = 0; splash_art[i] != NULL; i++) {
        printf("%s%s%s%s\n", BOLD, colors[i % color_count], splash_art[i], RESET);
        usleep(100000); // 100ms delay between lines for effect
    }
    
    // Display version and tagline
    printf("\n%s%s      WolfOS v1.0 - 'Lightweight. Powerful. Essential.'%s\n", RESET, CYAN, RESET);
    printf("%s      © 2025 WolfTech Innovations - All Rights Reserved%s\n", BLUE, RESET);
    
    // Create a fancy border at the bottom
    printf("\n%s╚", YELLOW);
    for (int i = 0; i < 78; i++) printf("═");
    printf("╝%s\n\n", RESET);
}

void handle_sigint(int sig) {
    printf("\n%s%s[SECURITY]%s %sNice try, I ain't quitting!%s\n", BOLD, RED, RESET, YELLOW, RESET);
}

void animate_loading() {
    const char *frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    int frame_count = 10;
    
    printf("%sInitializing minimal environment", GREEN);
    fflush(stdout);
    fancy_print("Initializing minimal Linux environment", BLUE, GREEN, 15);
    show_progress_bar("Loading essential kernel modules", 800);
    execute_command("mkdir /sys/block/");
    execute_command("mkdir /sys/proc");
    execute_command("mkdir /sys/dev/block");
    execute_command("mkdir /sys/dev/proc");
    execute_command("echo FS Init Complete");
    
    for (int i = 0; i < 30; i++) {
        printf("%s%s%s", CYAN, frames[i % frame_count], RESET);
        fflush(stdout);
        usleep(80000); // 80ms delay
        printf("\b");
    }
    printf("%s [COMPLETE]%s\n", GREEN, RESET);
}

// Print system specs
void print_system_info() {
    fancy_print("System Information", BLUE, YELLOW, 5);
    printf("   %s▸ %sKernel:%s      PuppyLinux Kernel\n", GREEN, BLUE, WHITE);
    printf("   %s▸ %sMemory:%s      Optimized for low-resource environments\n", GREEN, BLUE, WHITE);
    printf("   %s▸ %sComponents:%s  Essential utilities only\n", GREEN, BLUE, WHITE);
    printf("   %s▸ %sDesktop:%s     Optional (can be installed)\n", GREEN, BLUE, WHITE);
    printf("   %s▸ %sPackages:%s    Core utilities, bash, vim, gcc\n", GREEN, BLUE, WHITE);
}

// Check if running as root
void check_root_permissions() {
    if (geteuid() != 0) {
        printf("%s%sERROR: This installer must be run as root.%s\n", BOLD, RED, RESET);
        printf("%sPlease restart with sudo or as the root user.%s\n", YELLOW, RESET);
        exit(1);
    }
}

// Check internet connectivity
int check_internet_connection() {
    struct sockaddr_in addr;
    int sockfd;
    
    fancy_print("Checking internet connectivity", BLUE, CYAN, 10);
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        log_message("Failed to create socket", RED);
        return 0;
    }
    
    // Set non-blocking
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    // Try to resolve and connect to dns.google (8.8.8.8)
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = inet_addr("8.8.8.8");
    
    // Try to connect
    connect(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    
    // Check connection status after a small delay
    fd_set wset;
    struct timeval tv;
    
    FD_ZERO(&wset);
    FD_SET(sockfd, &wset);
    
    tv.tv_sec = 2;  // 2 seconds timeout
    tv.tv_usec = 0;
    
    int result = select(sockfd + 1, NULL, &wset, NULL, &tv);
    
    close(sockfd);
    
    if (result > 0) {
        int error = 0;
        socklen_t len = sizeof(error);
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &len);
        
        if (error == 0) {
            log_message("Internet connection available", GREEN);
            return 1;
        }
    }
    
    log_message("No internet connection detected", YELLOW);
    return 0;
}

// Execute a shell command and return its status
int execute_command(const char *command) {
    return system(command);
}

// Execute a shell command quietly (no output)
int execute_command_quiet(const char *command) {
    char quiet_command[1024];
    snprintf(quiet_command, sizeof(quiet_command), "%s >/dev/null 2>&1", command);
    return system(quiet_command);
}

// Execute a command and capture its output
int execute_command_with_output(const char *command, char *output, size_t output_size) {
    FILE *fp;
    int status;
    
    fp = popen(command, "r");
    if (fp == NULL) {
        strncpy(output, "Error executing command", output_size);
        return -1;
    }
    
    if (fgets(output, output_size, fp) == NULL) {
        output[0] = '\0';
    } else {
        // Remove trailing newline
        output[strcspn(output, "\n")] = '\0';
    }
    
    status = pclose(fp);
    return WEXITSTATUS(status);
}

// Check if the system is booted in EFI mode
int is_efi_system() {
    struct stat s;
    return (stat("/sys/firmware/efi", &s) == 0);
}

// Get disk information
char* get_disk_info() {
    static char disk_info[4096];
    FILE *fp;
    char line[1024];
    
    // Execute lsblk to get disk information
    fp = popen("lsblk -o NAME,SIZE,TYPE,MOUNTPOINT | grep -v loop | grep -v sr", "r");
    if (fp == NULL) {
        strcpy(disk_info, "Error retrieving disk information");
        return disk_info;
    }
    
    strcpy(disk_info, "");
    while (fgets(line, sizeof(line), fp) != NULL) {
        strcat(disk_info, line);
    }
    
    pclose(fp);
    return disk_info;
}

// Check if a disk already has valid partitions
int disk_has_valid_partitions(const char *disk) {
    char command[256];
    char output[1024];
    
    snprintf(command, sizeof(command), "lsblk -n -o NAME /dev/%s | grep -v %s | wc -l", disk, disk);
    execute_command_with_output(command, output, sizeof(output));
    
    return atoi(output) > 0;
}

// Get UUID of a partition
char* get_uuid(const char *partition) {
    static char uuid[64];
    char command[256];
    
    snprintf(command, sizeof(command), "blkid -s UUID -o value /dev/%s", partition);
    if (execute_command_with_output(command, uuid, sizeof(uuid)) != 0) {
        strcpy(uuid, "");
    }
    
    return uuid;
}

// Create disk partitions
void create_disk_partitions(const char *disk) {
    char command[512];
    
    log_message("Creating partitions on disk", CYAN);
    
    // Wipe existing partition table
    snprintf(command, sizeof(command), "wipefs -a /dev/%s", disk);
    execute_command_quiet(command);
    
    if (is_efi_system()) {
        // Create GPT partition table and EFI partitions
        fancy_print("Creating GPT partition table for UEFI system", BLUE, CYAN, 10);
        
        // Create GPT partition table
        snprintf(command, sizeof(command), "parted -s /dev/%s mklabel gpt", disk);
        execute_command_quiet(command);
        
        // Create EFI System Partition (ESP) - 512MB
        snprintf(command, sizeof(command), 
                "parted -s /dev/%s mkpart primary fat32 1MiB 513MiB", disk);
        execute_command_quiet(command);
        
        // Set ESP flag
        snprintf(command, sizeof(command), "parted -s /dev/%s set 1 esp on", disk);
        execute_command_quiet(command);
        
        // Create root partition using the rest of the disk
        snprintf(command, sizeof(command), 
                "parted -s /dev/%s mkpart primary ext4 513MiB 100%%", disk);
        execute_command_quiet(command);
    } else {
        // Create MBR partition table for legacy BIOS
        fancy_print("Creating MBR partition table for BIOS system", BLUE, CYAN, 10);
        
        // Create MBR partition table
        snprintf(command, sizeof(command), "parted -s /dev/%s mklabel msdos", disk);
        execute_command_quiet(command);
        
        // Create boot partition - 512MB
        snprintf(command, sizeof(command), 
                "parted -s /dev/%s mkpart primary ext4 1MiB 513MiB", disk);
        execute_command_quiet(command);
        
        // Set boot flag
        snprintf(command, sizeof(command), "parted -s /dev/%s set 1 boot on", disk);
        execute_command_quiet(command);
        
        // Create root partition using the rest of the disk
        snprintf(command, sizeof(command), 
                "parted -s /dev/%s mkpart primary ext4 513MiB 100%%", disk);
        execute_command_quiet(command);
    }
    
    // Wait for kernel to update partition table
    sleep(2);
    execute_command_quiet("partprobe");
    sleep(1);
}

// Format partitions
void format_partitions(const char *disk) {
    char command[512];
    char part1[32], part2[32];
    
    // Check if the device uses nvme naming (nvme0n1p1) or traditional naming (sda1)
    if (strncmp(disk, "nvme", 4) == 0 || strncmp(disk, "mmcblk", 6) == 0) {
        sprintf(part1, "%sp1", disk);
        sprintf(part2, "%sp2", disk);
    } else {
        sprintf(part1, "%s1", disk);
        sprintf(part2, "%s2", disk);
    }
    
    log_message("Formatting partitions", CYAN);
    
    if (is_efi_system()) {
        // Format EFI partition as FAT32
        snprintf(command, sizeof(command), "mkfs.fat -F32 /dev/%s", part1);
        execute_command_quiet(command);
    } else {
        // Format boot partition as ext4
        snprintf(command, sizeof(command), "mkfs.ext4 -F /dev/%s", part1);
        execute_command_quiet(command);
    }
    
    // Format root partition as ext4
    snprintf(command, sizeof(command), "mkfs.ext4 -F /dev/%s", part2);
    execute_command_quiet(command);
}

// Mount partitions
void mount_partitions(const char *disk) {
    char command[512];
    char part1[32], part2[32];
    
    // Check if the device uses nvme naming (nvme0n1p1) or traditional naming (sda1)
    if (strncmp(disk, "nvme", 4) == 0 || strncmp(disk, "mmcblk", 6) == 0) {
        sprintf(part1, "%sp1", disk);
        sprintf(part2, "%sp2", disk);
    } else {
        sprintf(part1, "%s1", disk);
        sprintf(part2, "%s2", disk);
    }
    
    log_message("Mounting partitions", CYAN);
    
    // Create mount point directory if it doesn't exist
    execute_command_quiet("mkdir -p " MOUNT_POINT);
    
    // Mount root partition
    snprintf(command, sizeof(command), "mount /dev/%s " MOUNT_POINT, part2);
    execute_command_quiet(command);
    
    // Create and mount boot/EFI partition
    if (is_efi_system()) {
        execute_command_quiet("mkdir -p " MOUNT_POINT "/boot/efi");
        snprintf(command, sizeof(command), "mount /dev/%s " MOUNT_POINT "/boot/efi", part1);
    } else {
        execute_command_quiet("mkdir -p " MOUNT_POINT "/boot");
        snprintf(command, sizeof(command), "mount /dev/%s " MOUNT_POINT "/boot", part1);
    }
    execute_command_quiet(command);
    
    // Mount virtual filesystems needed for installation
    execute_command_quiet("mount --bind /dev " MOUNT_POINT "/dev");
    execute_command_quiet("mount --bind /proc " MOUNT_POINT "/proc");
    execute_command_quiet("mount --bind /sys " MOUNT_POINT "/sys");
}

// Unmount all partitions
void unmount_partitions() {
    log_message("Unmounting filesystems", CYAN);
    
    // Unmount in reverse order
    execute_command_quiet("umount " MOUNT_POINT "/sys");
    execute_command_quiet("umount " MOUNT_POINT "/proc");
    execute_command_quiet("umount " MOUNT_POINT "/dev");
    
    if (is_efi_system()) {
        execute_command_quiet("umount " MOUNT_POINT "/boot/efi");
    } else {
        execute_command_quiet("umount " MOUNT_POINT "/boot");
    }
    
    execute_command_quiet("umount " MOUNT_POINT);
}

// Copy system files
void copy_system_files() {
    char command[512];
    
    log_message("Copying system files", CYAN);
    
    // Create necessary directories
    execute_command_quiet("mkdir -p " MOUNT_POINT "/dev");
    execute_command_quiet("mkdir -p " MOUNT_POINT "/proc");
    execute_command_quiet("mkdir -p " MOUNT_POINT "/sys");
    execute_command_quiet("mkdir -p " MOUNT_POINT "/run");
    execute_command_quiet("mkdir -p " MOUNT_POINT "/tmp");
    
    // Copy system files using rsync, excluding temporary and virtual filesystems
    snprintf(command, sizeof(command), 
             "rsync -aAXv --exclude={/dev/*,/proc/*,/sys/*,/tmp/*,/run/*,/mnt/*,/media/*,/lost+found} / " MOUNT_POINT);
    execute_command(command);
}

// Generate fstab file
void generate_fstab(const char *disk) {
    char command[512];
    char part1[32], part2[32];
    char uuid_root[64], uuid_boot[64];
    FILE *fstab;
    
    // Check if the device uses nvme naming (nvme0n1p1) or traditional naming (sda1)
    if (strncmp(disk, "nvme", 4) == 0 || strncmp(disk, "mmcblk", 6) == 0) {
        sprintf(part1, "%sp1", disk);
        sprintf(part2, "%sp2", disk);
    } else {
        sprintf(part1, "%s1", disk);
        sprintf(part2, "%s2", disk);
    }
    
    log_message("Generating fstab file", CYAN);
    
    // Get UUIDs
    strcpy(uuid_root, get_uuid(part2));
    strcpy(uuid_boot, get_uuid(part1));
    
    // Create fstab file
    snprintf(command, sizeof(command), MOUNT_POINT "%s", FSTAB_PATH);
    fstab = fopen(command, "w");
    if (fstab == NULL) {
        log_message("Error creating fstab file", RED);
        return;
    }
    
    // Write header
    fprintf(fstab, "# /etc/fstab: static file system information.\n");
    fprintf(fstab, "# Generated by WolfOS Installer\n\n");
    
    // Root partition
    if (strlen(uuid_root) > 0) {
        fprintf(fstab, "UUID=%s / ext4 defaults,noatime 0 1\n", uuid_root);
    } else {
        fprintf(fstab, "/dev/%s / ext4 defaults,noatime 0 1\n", part2);
    }
    
    // Boot/EFI partition
    if (is_efi_system()) {
        if (strlen(uuid_boot) > 0) {
            fprintf(fstab, "UUID=%s /boot/efi vfat defaults 0 2\n", uuid_boot);
        } else {
            fprintf(fstab, "/dev/%s /boot/efi vfat defaults 0 2\n", part1);
        }
    } else {
        if (strlen(uuid_boot) > 0) {
            fprintf(fstab, "UUID=%s /boot ext4 defaults 0 2\n", uuid_boot);
        } else {
            fprintf(fstab, "/dev/%s /boot ext4 defaults 0 2\n", part1);
        }
    }
    
    // Add tmpfs
    fprintf(fstab, "tmpfs /tmp tmpfs defaults,nosuid,nodev 0 0\n");
    
    fclose(fstab);
}

// Install EFI bootloader
void install_efi_bootloader(const char *disk) {
    char command[512];
    
    log_message("Installing EFI bootloader", CYAN);
    
    // Install GRUB packages
    snprintf(command, sizeof(command), 
             "chroot " MOUNT_POINT " apt-get update && apt-get install -y grub-efi-amd64 efibootmgr");
    execute_command(command);
    
    // Install GRUB to EFI directory
    snprintf(command, sizeof(command), 
             "chroot " MOUNT_POINT " grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=wolfos");
    execute_command(command);
    
    // Update GRUB configuration
    snprintf(command, sizeof(command), "chroot " MOUNT_POINT " update-grub");
    execute_command(command);
}

// Install legacy BIOS bootloader
void install_legacy_bootloader(const char *disk) {
    char command[512];
    
    log_message("Installing BIOS bootloader", CYAN);
    
    // Install GRUB packages
    snprintf(command, sizeof(command), 
             "chroot " MOUNT_POINT " apt-get update && apt-get install -y grub-pc");
    execute_command(command);
    
    // Install GRUB to MBR
    snprintf(command, sizeof(command), 
             "chroot " MOUNT_POINT " grub-install /dev/%s", disk);
    execute_command(command);
    
    // Update GRUB configuration
    snprintf(command, sizeof(command), "chroot " MOUNT_POINT " update-grub");
    execute_command(command);
}

// Install bootloader
void install_bootloader(const char *disk) {
    if (is_efi_system()) {
        install_efi_bootloader(disk);
    } else {
        install_legacy_bootloader(disk);
    }
}

// Configure system settings
void configure_system(const char *disk) {
    char command[512];
    char hostname[64];
    
    log_message("Configuring system", CYAN);
    
    // Set hostname
    printf("%sEnter hostname for your system: %s", CYAN, RESET);
    if (fgets(hostname, sizeof(hostname), stdin) != NULL) {
        hostname[strcspn(hostname, "\n")] = '\0';
        
        if (strlen(hostname) > 0) {
            // Write hostname to /etc/hostname
            snprintf(command, sizeof(command), "echo '%s' > " MOUNT_POINT "/etc/hostname", hostname);
            execute_command_quiet(command);
            
            // Update /etc/hosts
            snprintf(command, sizeof(command), 
                     "sed -i 's/127.0.1.1.*/127.0.1.1\\t%s/g' " MOUNT_POINT "/etc/hosts", hostname);
            execute_command_quiet(command);
        }
    }
    
    // Generate fstab
    generate_fstab(disk);
    
    // Set root password
    fancy_print("Setting root password", BLUE, YELLOW, 10);
    snprintf(command, sizeof(command), "chroot " MOUNT_POINT " passwd");
    execute_command(command);
    
    // Create a regular user
    char username[64];
    printf("%sCreate a regular user (leave empty to skip): %s", CYAN, RESET);
    if (fgets(username, sizeof(username), stdin) != NULL) {
        username[strcspn(username, "\n")] = '\0';
        
        if (strlen(username) > 0) {
            // Create user
            snprintf(command, sizeof(command), 
                     "chroot " MOUNT_POINT " useradd -m -G sudo,audio,video,netdev,plugdev -s /bin/bash %s", 
                     username);
            execute_command_quiet(command);
            
            // Set password for the user
            fancy_print("Setting password for the new user", BLUE, YELLOW, 10);
            snprintf(command, sizeof(command), "chroot " MOUNT_POINT " passwd %s", username);
            execute_command(command);
        }
    }
    
    // Update initramfs
    log_message("Updating initramfs", CYAN);
    execute_command("chroot " MOUNT_POINT " update-initramfs -u");
}

// Function to install WolfOS to disk
void install_to_disk() {
    char target_disk[20];
    char confirm[10];
    int valid_choice = 0;
    
    // Check if running as root
    check_root_permissions();
    
    printf("\033[2J\033[H"); // Clear screen
    
    printf("%s%s╔══════════════════════════════════════════════════════════════╗%s\n", BOLD, BLUE, RESET);
    printf("%s%s║ %sWOLFOS DISK INSTALLATION                                   %s║%s\n", BOLD, BLUE, RED, BLUE, RESET);
    printf("%s%s╠══════════════════════════════════════════════════════════════╣%s\n", BOLD, BLUE, RESET);
    
    // Display available disks
    char* disk_info = get_disk_info();
    printf("%s%s║ %s%-60s%s║%s\n", BOLD, BLUE, WHITE, disk_info, BLUE, RESET);
    
    printf("%s%s╠══════════════════════════════════════════════════════════════╣%s\n", BOLD, BLUE, RESET);
    printf("%s%s║ %sWARNING: Installing will erase all data on target disk!    %s║%s\n", BOLD, BLUE, RED, BLUE, RESET);
    printf("%s%s╚══════════════════════════════════════════════════════════════╝%s\n\n", BOLD, BLUE, RESET);
    
    // Get target disk from user
    while (!valid_choice) {
        printf("%sEnter target disk (e.g., sda, nvme0n1) or 'q' to quit: %s", YELLOW, RESET);
// Get target disk from user
while (!valid_choice) {
    printf("%sEnter target disk (e.g., sda, nvme0n1) or 'q' to quit: %s", YELLOW, RESET);
    if (fgets(target_disk, sizeof(target_disk), stdin) != NULL) {
        // Remove newline
        target_disk[strcspn(target_disk, "\n")] = '\0';
        
        // Check if user wants to quit
        if (target_disk[0] == 'q' || target_disk[0] == 'Q') {
            log_message("Installation aborted by user", YELLOW);
            return;
        }
        
        // Validate disk exists
        char command[256];
        char output[256];
        snprintf(command, sizeof(command), "lsblk -no NAME /dev/%s 2>/dev/null | grep -q '^%s$'", 
                 target_disk, target_disk);
        
        if (execute_command_quiet(command) == 0) {
            // Disk exists, check if it already has partitions
            if (disk_has_valid_partitions(target_disk)) {
                printf("\n%sWARNING: Disk /dev/%s already has partitions!%s\n", RED, target_disk, RESET);
                printf("%sAre you ABSOLUTELY sure you want to erase ALL data? (yes/no): %s", RED, RESET);
                
                if (fgets(confirm, sizeof(confirm), stdin) != NULL) {
                    confirm[strcspn(confirm, "\n")] = '\0';
                    if (strcmp(confirm, "yes") == 0) {
                        valid_choice = 1;
                    } else {
                        printf("%sInstallation cancelled.%s\n", YELLOW, RESET);
                    }
                }
            } else {
                // Disk exists and has no partitions
                printf("%sAre you sure you want to install to /dev/%s? (yes/no): %s", 
                       YELLOW, target_disk, RESET);
                
                if (fgets(confirm, sizeof(confirm), stdin) != NULL) {
                    confirm[strcspn(confirm, "\n")] = '\0';
                    if (strcmp(confirm, "yes") == 0) {
                        valid_choice = 1;
                    } else {
                        printf("%sInstallation cancelled.%s\n", YELLOW, RESET);
                    }
                }
            }
        } else {
            printf("%sError: Disk /dev/%s not found!%s\n", RED, target_disk, RESET);
        }
    }
}}

// Begin installation process
printf("\033[2J\033[H"); // Clear screen
fancy_print("Beginning WolfOS installation to disk", BLUE, GREEN, 15);

// Create partitions
fancy_print("Step 1/6: Creating disk partitions", BLUE, CYAN, 15);
create_disk_partitions(target_disk);
show_progress_bar("Creating partitions", 1000);

// Format partitions
fancy_print("Step 2/6: Formatting partitions", BLUE, CYAN, 15);
format_partitions(target_disk);
show_progress_bar("Formatting filesystems", 800);

// Mount partitions
fancy_print("Step 3/6: Mounting partitions", BLUE, CYAN, 15);
mount_partitions(target_disk);
show_progress_bar("Mounting filesystems", 500);

// Copy system files
fancy_print("Step 4/6: Copying system files", BLUE, CYAN, 15);
copy_system_files();
show_progress_bar("Copying files", 5000);

// Configure system
fancy_print("Step 5/6: Configuring system", BLUE, CYAN, 15);
configure_system(target_disk);
show_progress_bar("Configuring system", 1500);

// Install bootloader
fancy_print("Step 6/6: Installing bootloader", BLUE, CYAN, 15);
install_bootloader(target_disk);
show_progress_bar("Installing bootloader", 2000);

// Unmount filesystems
fancy_print("Finalizing installation", BLUE, CYAN, 15);
unmount_partitions();

printf("\n%s%s╔══════════════════════════════════════════════════════════════╗%s\n", BOLD, GREEN, RESET);
printf("%s%s║ %sWolfOS INSTALLATION COMPLETE                               %s║%s\n", BOLD, GREEN, WHITE, GREEN, RESET);
printf("%s%s╠══════════════════════════════════════════════════════════════╣%s\n", BOLD, GREEN, RESET);
printf("%s%s║ %sYour system has been successfully installed!               %s║%s\n", BOLD, GREEN, WHITE, GREEN, RESET);
printf("%s%s║ %sYou can now reboot into your new WolfOS installation.      %s║%s\n", BOLD, GREEN, WHITE, GREEN, RESET);
printf("%s%s╚══════════════════════════════════════════════════════════════╝%s\n\n", BOLD, GREEN, RESET);

// Ask if user wants to reboot
printf("%sWould you like to reboot now? (yes/no): %s", CYAN, RESET);
if (fgets(confirm, sizeof(confirm), stdin) != NULL) {
    confirm[strcspn(confirm, "\n")] = '\0';
    if (strcmp(confirm, "yes") == 0) {
        fancy_print("Rebooting system...", BLUE, RED, 20);
        execute_command("reboot");
    } else {
        fancy_print("You can reboot manually when ready using the 'reboot' command.", BLUE, YELLOW, 15);
    }
}
}

// Function to show the main menu and handle user choices
void show_main_menu() {
    int running = 1;
    char choice[10];
    int valid_choice;
    
    while (running) {
        printf("\033[2J\033[H"); // Clear screen
        
        printf("%s%s╔══════════════════════════════════════════════════════════════╗%s\n", BOLD, BLUE, RESET);
        printf("%s%s║ %sWOLFOS INSTALLER - MAIN MENU                               %s║%s\n", BOLD, BLUE, RED, BLUE, RESET);
        printf("%s%s╠══════════════════════════════════════════════════════════════╣%s\n", BOLD, BLUE, RESET);
        printf("%s%s║ %s1) Install WolfOS to disk                                  %s║%s\n", BOLD, BLUE, GREEN, BLUE, RESET);
        printf("%s%s║ %s2) Live environment tools                                  %s║%s\n", BOLD, BLUE, GREEN, BLUE, RESET);
        printf("%s%s║ %s3) System information                                      %s║%s\n", BOLD, BLUE, GREEN, BLUE, RESET);
        printf("%s%s║ %s4) Network configuration                                   %s║%s\n", BOLD, BLUE, GREEN, BLUE, RESET);
        printf("%s%s║ %s5) Help and documentation                                  %s║%s\n", BOLD, BLUE, GREEN, BLUE, RESET);
        printf("%s%s║ %s6) Exit to shell                                           %s║%s\n", BOLD, BLUE, GREEN, BLUE, RESET);
        printf("%s%s╚══════════════════════════════════════════════════════════════╝%s\n\n", BOLD, BLUE, RESET);
        
        valid_choice = 0;
        while (!valid_choice) {
            printf("%sEnter your choice (1-6): %s", YELLOW, RESET);
            if (fgets(choice, sizeof(choice), stdin) != NULL) {
                // Remove newline
                choice[strcspn(choice, "\n")] = '\0';
                
                // Check if input is a valid choice
                if (strlen(choice) == 1 && isdigit(choice[0])) {
                    int option = choice[0] - '0';
                    if (option >= 1 && option <= 6) {
                        valid_choice = 1;
                        
                        // Process choice
                        switch (option) {
                            case 1:
                                install_to_disk();
                                break;
                            case 2:
                                // Live environment tools submenu would go here
                                fancy_print("Live environment tools - Feature coming soon", BLUE, YELLOW, 10);
                                printf("\n%sPress Enter to continue...%s", CYAN, RESET);
                                getchar();
                                break;
                            case 3:
                                print_system_info();
                                printf("\n%sPress Enter to continue...%s", CYAN, RESET);
                                getchar();
                                break;
                            case 4:
                                // Network configuration would go here
                                if (check_internet_connection()) {
                                    fancy_print("Internet connection already available", BLUE, GREEN, 10);
                                } else {
                                    fancy_print("No internet connection detected", BLUE, YELLOW, 10);
                                    fancy_print("Network configuration - Feature coming soon", BLUE, YELLOW, 10);
                                }
                                printf("\n%sPress Enter to continue...%s", CYAN, RESET);
                                getchar();
                                break;
                            case 5:
                                // Help and documentation would go here
                                fancy_print("WolfOS Help and Documentation", BLUE, CYAN, 10);
                                printf("\n%sWolfOS is a minimal Linux distribution designed for advanced users.%s\n", WHITE, RESET);
                                printf("%sIt provides a lightweight base system with essential tools.%s\n", WHITE, RESET);
                                printf("%sFor more information, visit: https://wolfos.uk%s\n", WHITE, RESET);
                                printf("\n%sPress Enter to continue...%s", CYAN, RESET);
                                getchar();
                                break;
                            case 6:
                                running = 0;
                                fancy_print("Exiting to shell . . .", BLUE, YELLOW, 10);
                                break;
                        }
                    } else {
                        printf("%sInvalid choice. Please enter a number between 1 and 6.%s\n", RED, RESET);
                    }
                } else {
                    printf("%sInvalid choice. Please enter a number between 1 and 6.%s\n", RED, RESET);
                }
            }
        }
    }
}

// Function to prompt for GUI installation
void prompt_gui_installation() {
    char choice[10];
    int valid_choice = 0;
    
    printf("\n%s%s╔══════════════════════════════════════════════════════════════╗%s\n", BOLD, BLUE, RESET);
    printf("%s%s║ %sGRAPHICAL USER INTERFACE INSTALLATION                       %s║%s\n", BOLD, BLUE, CYAN, BLUE, RESET);
    printf("%s%s╠══════════════════════════════════════════════════════════════╣%s\n", BOLD, BLUE, RESET);
    printf("%s%s║ %sWolfOS detected an internet connection.                     %s║%s\n", BOLD, BLUE, WHITE, BLUE, RESET);
    printf("%s%s║ %sWould you like to install a graphical environment?          %s║%s\n", BOLD, BLUE, WHITE, BLUE, RESET);
    printf("%s%s╠══════════════════════════════════════════════════════════════╣%s\n", BOLD, BLUE, RESET);
    printf("%s%s║ %s1) KDE Plasma  %s- Full-featured desktop environment         %s║%s\n", BOLD, BLUE, GREEN, WHITE, BLUE, RESET);
    printf("%s%s║ %s2) GNOME       %s- Modern desktop with a clean interface     %s║%s\n", BOLD, BLUE, GREEN, WHITE, BLUE, RESET);
    printf("%s%s║ %s3) XFCE4       %s- Lightweight and efficient desktop         %s║%s\n", BOLD, BLUE, GREEN, WHITE, BLUE, RESET);
    printf("%s%s║ %s4) i3          %s- Minimalist tiling window manager          %s║%s\n", BOLD, BLUE, GREEN, WHITE, BLUE, RESET);
    printf("%s%s║ %s5) Skip        %s- Continue with command-line only           %s║%s\n", BOLD, BLUE, GREEN, WHITE, BLUE, RESET);
    printf("%s%s╚══════════════════════════════════════════════════════════════╝%s\n\n", BOLD, BLUE, RESET);
    
    while (!valid_choice) {
        printf("%sPlease enter your choice (1-5): %s", YELLOW, RESET);
        if (fgets(choice, sizeof(choice), stdin) != NULL) {
            // Remove newline
            choice[strcspn(choice, "\n")] = '\0';
            
            // Check if input is a single digit between 1-5
            if (strlen(choice) == 1 && isdigit(choice[0])) {
                int option = choice[0] - '0';
                if (option >= 1 && option <= 5) {
                    valid_choice = 1;
                    
                    // Process the choice
                    switch (option) {
                        case 1:
                            fancy_print("Installing KDE Plasma desktop environment...", BLUE, CYAN, 10);
                            show_progress_bar("Downloading packages", 1500);
                            execute_command("apt-get install -y kde-plasma-desktop");
                            show_progress_bar("Installing packages", 2000);
                            fancy_print("KDE Plasma installation complete!", BLUE, GREEN, 10);
                            break;
                        case 2:
                            fancy_print("Installing GNOME desktop environment...", BLUE, CYAN, 10);
                            show_progress_bar("Downloading packages", 1500);
                            execute_command("apt-get install -y gnome-shell gnome-session gdm3");
                            show_progress_bar("Installing packages", 2000);
                            fancy_print("GNOME installation complete!", BLUE, GREEN, 10);
                            break;
                        case 3:
                            fancy_print("Installing XFCE4 desktop environment...", BLUE, CYAN, 10);
                            show_progress_bar("Downloading packages", 1000);
                            execute_command("apt-get install -y xfce4 xfce4-goodies");
                            show_progress_bar("Installing packages", 1500);
                            fancy_print("XFCE4 installation complete!", BLUE, GREEN, 10);
                            break;
                        case 4:
                            fancy_print("Installing i3 window manager...", BLUE, CYAN, 10);
                            show_progress_bar("Downloading packages", 800);
                            execute_command("apt-get install -y i3 i3status i3lock dmenu xorg");
                            show_progress_bar("Installing packages", 1200);
                            fancy_print("i3 window manager installation complete!", BLUE, GREEN, 10);
                            break;
                        case 5:
                            fancy_print("Continuing with command-line interface only", BLUE, CYAN, 10);
                            break;
                    }
                    
                    if (option >= 1 && option <= 4) {
                        fancy_print("The system will need to restart to complete the installation", BLUE, YELLOW, 15);
                        printf("%sWould you like to restart now? (y/n): %s", YELLOW, RESET);
                        char restart[10];
                        if (fgets(restart, sizeof(restart), stdin) != NULL) {
                            restart[strcspn(restart, "\n")] = '\0';
                            if (restart[0] == 'y' || restart[0] == 'Y') {
                                fancy_print("Restarting system...", BLUE, RED, 20);
                                execute_command("reboot");
                                exit(0);
                            } else {
                                fancy_print("System will continue in CLI mode. Restart later to use the GUI.", BLUE, YELLOW, 15);
                            }
                        }
                    }
                }
            }
            
            if (!valid_choice) {
                printf("%sInvalid choice. Please enter a number between 1 and 5.%s\n", RED, RESET);
            }
        }
    }
}

int main() {
    // Seed random number generator
    srand(time(NULL));
    
    // Set up signal handling
    signal(SIGINT, handle_sigint);
    
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
    
    // If internet is available, prompt for GUI installation
    if (internet_available) {
        prompt_gui_installation();
    } else {
        fancy_print("No internet connection available. Running in minimal CLI mode.", BLUE, YELLOW, 15);
}
    
    // Show main menu
    show_main_menu();
    
    fancy_print("Starting minimal bash environment...", BLUE, CYAN, 10);
    
    // Final message
    printf("\n%s%s╔════════════════════════════════════════════════════════════╗%s\n", BOLD, BLUE, RESET);
    printf("%s%s║ %sWolfOS is ready                                           %s║%s\n", BOLD, BLUE, GREEN, BLUE, RESET);
    printf("%s%s╚══════════════════════════════════════════════════════════════╝%s\n\n", BOLD, BLUE, RESET);
    
    // Execute shell
    execl("/bin/bash", "bash", "--login", "-i", NULL);
    execl("clear", NULL);
    execl("login", NULL);
    // Should never reach here
    return 0;
}
