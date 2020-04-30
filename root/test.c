#include <stdint.h>
#include <stddef.h>

uint64_t __syscall1(uint64_t number, int64_t arg1) {
    uint64_t ret = 0;
    asm volatile ("syscall"
        : "=a"(ret)
        : "a"(number), "D"(arg1)
        : "rcx", "r11");
    return ret;
}

uint64_t __syscall2(uint64_t number, int64_t arg1, int64_t arg2) {
    uint64_t ret = 0;
    asm volatile ("syscall"
    : "=a"(ret)
    : "a"(number), "D"(arg1), "S"(arg2)
    : "rcx", "r11");
    return ret;
}

uint64_t __syscall3(uint64_t number, int64_t arg1, int64_t arg2, int32_t arg3) {
    uint64_t ret = 0;
    asm volatile ("syscall"
    : "=a"(ret)
    : "a"(number), "D"(arg1), "S"(arg2), "d"(arg3)
    : "rcx", "r11");
    return ret;
}

static void sys_log(const char* str) {
    __syscall1(0x00, (uintptr_t) str);
}

static int vfs_open(const char* path) {
    return (int)(int64_t)__syscall1(0x50, (uintptr_t)path);
}

static int file_read(int file, void* buffer, size_t len) {
    return (int)(int64_t)__syscall3(0x70, file, (uintptr_t)buffer, len);
}

void _start() {

    // resolve the path and open it
    int file = vfs_open("/hello.txt");
    if (file < 0) goto cleanup;

    // read itf
    char buffer[255] = {0};
    int ret = file_read(file, buffer, sizeof(buffer));
    if (ret < 0) goto cleanup;

    // log it
    sys_log((char*)1);
    while(1);

cleanup:
    sys_log("Got error in syscall");
    while(1);
}
