#define FUSE_USE_VERSION 35

#include <fuse3/fuse.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <string>
#include <sys/wait.h>
#include <fcntl.h>

#include "vfs.hpp"

static bool check_shell_compatibility(struct passwd* user_entry) {
    if (!user_entry || !user_entry->pw_shell) 
        return false;
    size_t shell_len = strlen(user_entry->pw_shell);
    return shell_len >= 2 && strcmp(user_entry->pw_shell + shell_len - 2, "sh") == 0;
}

// Получение атрибутов файла
static int getattr_callback(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));
    time_t current_time = time(nullptr);
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = current_time;
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        return 0;
    }
    return -ENOENT;
}

// Чтение содержимого директории
static int readdir_callback(const char *path, void *buf, fuse_fill_dir_t filler,
                           off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;
    filler(buf, ".", NULL, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", NULL, 0, FUSE_FILL_DIR_PLUS);
    if (strcmp(path, "/") == 0) {
        struct passwd* user_entry;
        setpwent();
        while ((user_entry = getpwent()) != nullptr) {
            if (check_shell_compatibility(user_entry)) {
                filler(buf, user_entry->pw_name, NULL, 0, FUSE_FILL_DIR_PLUS);
            }
        }
        endpwent();
        return 0;
    }
    return -ENOENT;
}

static struct fuse_operations vfs_operations = {
    .getattr = getattr_callback,
    .readdir = readdir_callback,
};

// Запуск FUSE в отдельном потоке
static void* start_vfs(void* arg) {
    (void) arg;
    char* fuse_args[] = {
        (char*)"kubsh_vfs",
        (char*)"-f", // foreground mode
        (char*)"-odefault_permissions", // проверка прав доступа
        (char*)"/opt/users", // точка монтирования
        NULL
    };
    int argc = sizeof(fuse_args) / sizeof(fuse_args[0]) - 1;
    fuse_main(argc, fuse_args, &vfs_operations, NULL);
    return NULL;
}

// Инициализация VFS
void init_virtual_fs() {
    // Создаем директорию для монтирования
    system("mkdir -p /opt/users");
    
    // Запускаем FUSE в отдельном потоке
    pthread_t vfs_thread;
    pthread_create(&vfs_thread, NULL, start_vfs, NULL);
    pthread_detach(vfs_thread);  // Не ждем завершения потока
    
    // Даем время FUSE запуститься
    sleep(1);
}