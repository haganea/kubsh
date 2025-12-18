#define FUSE_USE_VERSION 35

#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <pwd.h>
#include <sys/types.h>
#include <cerrno>
#include <ctime>
#include <string>
#include <sys/wait.h>
#include <fuse3/fuse.h>
#include <pthread.h>
#include <fcntl.h>

#include "vfs.hpp"

int run_command(const char* prog, char* const args[]) {
    pid_t pid = fork();

    if (pid == 0) {
        execvp(prog, args);
        _exit(127);
    }

    int exit_status;
    waitpid(pid, &exit_status, 0);

    return WIFEXITED(exit_status) && WEXITSTATUS(exit_status) == 0 ? 0 : -1;
}

bool is_valid_shell(struct passwd* pwd_entry) {
    if (!pwd_entry || !pwd_entry->pw_shell) 
        return false;
    size_t len = strlen(pwd_entry->pw_shell);
    return len >= 2 && strcmp(pwd_entry->pw_shell + len - 2, "sh") == 0;
}

int get_file_info(const char* path, struct stat* st, struct fuse_file_info* fi) {
    (void)fi;
    memset(st, 0, sizeof(struct stat));
    
    time_t now = time(nullptr);
    st->st_atime = st->st_mtime = st->st_ctime = now;

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_uid = getuid();
        st->st_gid = getgid();
        return 0;
    }

    char uname[256];
    char fname[256];

    if (sscanf(path, "/%255[^/]/%255[^/]", uname, fname) == 2) {
        struct passwd* pwd_entry = getpwnam(uname);

        if (strcmp(fname, "id") != 0 &&
            strcmp(fname, "home") != 0 &&
            strcmp(fname, "shell") != 0) {
            return -ENOENT;
        }

        if (pwd_entry != nullptr) {
            st->st_mode = S_IFREG | 0644;
            st->st_uid = pwd_entry->pw_uid;
            st->st_gid = pwd_entry->pw_gid;
            st->st_size = 256;
            return 0;
        }
        return -ENOENT;
    }

    if (sscanf(path, "/%255[^/]", uname) == 1) {
        struct passwd* pwd_entry = getpwnam(uname);
        if (pwd_entry != nullptr) {
            st->st_mode = S_IFDIR | 0755;
            st->st_uid = pwd_entry->pw_uid;
            st->st_gid = pwd_entry->pw_gid;
            return 0;
        }
        return -ENOENT;
    }

    return -ENOENT;
}

int read_dir(
    const char* path,
    void* buf, 
    fuse_fill_dir_t filler, 
    off_t offset, 
    struct fuse_file_info* fi, 
    enum fuse_readdir_flags flags
) {
    (void)offset;
    (void)fi;
    (void)flags;

    filler(buf, ".", nullptr, 0, FUSE_FILL_DIR_PLUS);
    filler(buf, "..", nullptr, 0, FUSE_FILL_DIR_PLUS);

    if (strcmp(path, "/") == 0) {
        struct passwd* pwd_entry;
        setpwent();

        while ((pwd_entry = getpwent()) != nullptr) {
            if (is_valid_shell(pwd_entry)) {
                filler(buf, pwd_entry->pw_name, nullptr, 0, FUSE_FILL_DIR_PLUS);
            }
        }

        endpwent();
        return 0;
    }

    char uname[256] = {0};
    if (sscanf(path, "/%255[^/]", uname) == 1) {
        struct passwd* pwd_entry = getpwnam(uname);
        if (pwd_entry != nullptr) {
            filler(buf, "id", nullptr, 0, FUSE_FILL_DIR_PLUS);
            filler(buf, "home", nullptr, 0, FUSE_FILL_DIR_PLUS);
            filler(buf, "shell", nullptr, 0, FUSE_FILL_DIR_PLUS);
            return 0;
        }
    }

    return -ENOENT;
}

int read_file(const char* path, char* buf, size_t size, 
                     off_t offset, struct fuse_file_info* fi) {
    (void)fi;

    char uname[256];
    char fname[256];

    sscanf(path, "/%255[^/]/%255[^/]", uname, fname);

    struct passwd* pwd_entry = getpwnam(uname);
    if (!pwd_entry) return -ENOENT;
    
    char content[256];
    content[0] = '\0';

    if (strcmp(fname, "id") == 0) {
        snprintf(content, sizeof(content), "%d", pwd_entry->pw_uid);
    } else if (strcmp(fname, "home") == 0) {
        snprintf(content, sizeof(content), "%s", pwd_entry->pw_dir);
    } else {
        snprintf(content, sizeof(content), "%s", pwd_entry->pw_shell);
    }

    size_t len = strlen(content);
    if (len > 0 && content[len-1] == '\n') {
        content[len-1] = '\0';
        len--;
    }

    if ((size_t)offset >= len) {
        return 0;
    }

    if (offset + size > len) {
        size = len - offset;
    }

    memcpy(buf, content + offset, size);
    return size;
}

int make_dir(const char* path, mode_t mode) {
    (void)mode;

    char uname[256];

    if (sscanf(path, "/%255[^/]", uname) == 1) {
        struct passwd* pwd_entry = getpwnam(uname);
        
        if (pwd_entry != nullptr) {
            return -EEXIST;
        }

        char* const args[] = {
            (char*)"adduser", 
            (char*)"--disabled-password",
            (char*)"--gecos", 
            (char*)"", 
            (char*)uname, 
            nullptr
        };

        if (run_command("adduser", args) != 0) return -EIO;
    }

    return 0;
}

int remove_dir(const char* path) {
    char uname[256];
    
    if (sscanf(path, "/%255[^/]", uname) == 1) {
        if (strchr(path + 1, '/') == nullptr) {
            struct passwd* pwd_entry = getpwnam(uname);
            
            if (pwd_entry != nullptr) {
                char* const args[] = {
                    (char*)"userdel", 
                    (char*)"--remove", 
                    (char*)uname, 
                    nullptr
                };

                if (run_command("userdel", args) != 0) return -EIO;
                return 0;
            }
            return -ENOENT;
        }
        return -EPERM;
    }
    return -EPERM;
}

struct fuse_operations ops = {};

void setup_ops() {
    ops.getattr = get_file_info;
    ops.readdir = read_dir;
    ops.mkdir   = make_dir;
    ops.rmdir   = remove_dir;
    ops.read    = read_file;
}

void* start_thread(void* arg) {
    (void)arg;

    setup_ops();

    int dev_null = open("/dev/null", O_WRONLY);
    int saved_err = dup(STDERR_FILENO);
    dup2(dev_null, STDERR_FILENO);
    close(dev_null);

    char* fuse_args[] = {
        (char*)"kubsh_fs",
        (char*)"-f",
        (char*)"-odefault_permissions",
        (char*)"-oauto_unmount",
        (char*)"/opt/users"
    };

    int argc = sizeof(fuse_args) / sizeof(fuse_args[0]);

    fuse_main(argc, fuse_args, &ops, nullptr);

    dup2(saved_err, STDERR_FILENO);
    close(saved_err);

    return nullptr;
}

void init_virtual_fs() {
    pthread_t thread;
    pthread_create(&thread, nullptr, start_thread, nullptr);
    pthread_detach(thread);
}