#include <errno.h>
#include <string.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <unistd.h>
#include "fio.h"
#include "filesystem.h"
#include "osdebug.h"
#include "hash-djb2.h"
#include "serial_io.h"

static struct fddef_t fio_fds[MAX_FDS];

static ssize_t stdin_read(void * opaque, void * buf, size_t count) {
    int i;
    char * data = buf;

    for (i = 0; i < count; i++)
        data[i] = recv_byte(NULL);

    return count;
}

static ssize_t stdout_write(void * opaque, const void * buf, size_t count) {
    int i;
    const char * data = (const char *) buf;

    for (i = 0; i < count; i++) {
        if (data[i] == '\n')
            send_byte('\r');

        send_byte(data[i]);
    }

    return count;
}

static xSemaphoreHandle fio_sem = NULL;

__attribute__((constructor)) void fio_init() {
    memset(fio_fds, 0, sizeof(fio_fds));
    fio_fds[0].fdread = stdin_read;
    fio_fds[1].fdwrite = stdout_write;
    fio_fds[2].fdwrite = stdout_write;
    fio_sem = xSemaphoreCreateMutex();
}

struct fddef_t * fio_getfd(int fd) {
    if ((fd < 0) || (fd >= MAX_FDS))
        return NULL;
    return fio_fds + fd;
}

static int fio_is_open_int(int fd) {
    if ((fd < 0) || (fd >= MAX_FDS))
        return 0;
    int r = !((fio_fds[fd].fdread == NULL) &&
              (fio_fds[fd].fdwrite == NULL) &&
              (fio_fds[fd].fdseek == NULL) &&
              (fio_fds[fd].fdclose == NULL) &&
              (fio_fds[fd].opaque == NULL));
    return r;
}

static int fio_findfd() {
    int i;

    for (i = 0; i < MAX_FDS; i++) {
        if (!fio_is_open_int(i))
            return i;
    }

    return -1;
}

int fio_is_open(int fd) {
    int r = 0;
    xSemaphoreTake(fio_sem, portMAX_DELAY);
    r = fio_is_open_int(fd);
    xSemaphoreGive(fio_sem);
    return r;
}

int fio_open(fdread_t fdread, fdwrite_t fdwrite, fdseek_t fdseek, fdclose_t fdclose, void * opaque) {
    int fd;
//    DBGOUT("fio_open(%p, %p, %p, %p, %p)\r\n", fdread, fdwrite, fdseek, fdclose, opaque);
    xSemaphoreTake(fio_sem, portMAX_DELAY);
    fd = fio_findfd();

    if (fd >= 0) {
        fio_fds[fd].fdread = fdread;
        fio_fds[fd].fdwrite = fdwrite;
        fio_fds[fd].fdseek = fdseek;
        fio_fds[fd].fdclose = fdclose;
        fio_fds[fd].opaque = opaque;
    }
    xSemaphoreGive(fio_sem);

    return fd;
}

ssize_t fio_read(int fd, void * buf, size_t count) {
    ssize_t r = 0;
//    DBGOUT("fio_read(%i, %p, %i)\r\n", fd, buf, count);
    if (fio_is_open_int(fd)) {
        if (fio_fds[fd].fdread) {
            r = fio_fds[fd].fdread(fio_fds[fd].opaque, buf, count);
        } else {
            r = -3;
        }
    } else {
        r = -2;
    }
    return r;
}

ssize_t fio_write(int fd, const void * buf, size_t count) {
    ssize_t r = 0;
//    DBGOUT("fio_write(%i, %p, %i)\r\n", fd, buf, count);
    if (fio_is_open_int(fd)) {
        if (fio_fds[fd].fdwrite) {
            r = fio_fds[fd].fdwrite(fio_fds[fd].opaque, buf, count);
        } else {
            r = -3;
        }
    } else {
        r = -2;
    }
    return r;
}

off_t fio_seek(int fd, off_t offset, int whence) {
    off_t r = 0;
//    DBGOUT("fio_seek(%i, %i, %i)\r\n", fd, offset, whence);
    if (fio_is_open_int(fd)) {
        if (fio_fds[fd].fdseek) {
            r = fio_fds[fd].fdseek(fio_fds[fd].opaque, offset, whence);
        } else {
            r = -3;
        }
    } else {
        r = -2;
    }
    return r;
}

int fio_close(int fd) {
    int r = 0;
//    DBGOUT("fio_close(%i)\r\n", fd);
    if (fio_is_open_int(fd)) {
        if (fio_fds[fd].fdclose)
            r = fio_fds[fd].fdclose(fio_fds[fd].opaque);
        xSemaphoreTake(fio_sem, portMAX_DELAY);
        memset(fio_fds + fd, 0, sizeof(struct fddef_t));
        xSemaphoreGive(fio_sem);
    } else {
        r = -2;
    }
    return r;
}

void fio_perror(const char * prefix) {
    int err = errno;

    fio_write(2, prefix, strlen(prefix));
    fio_write(2, ": ", 2);
    switch (err) {
        case EPERM:
            fio_write(2, "Permission denied", 17);
        break;
        case ENOENT:
            fio_write(2, "No such file or directory", 25);
        break;
    }
    fio_write(2, "\n", 1);
}

void fio_set_opaque(int fd, void * opaque) {
    if (fio_is_open_int(fd))
        fio_fds[fd].opaque = opaque;
}

char *fio_getline(int fd, char *str, size_t n)
{
    size_t i;
    char c = '\0';

    for (i = 0; fio_read(fd, &c, 1) && c != '\r' && c != '\n' && i < n - 1; i++)
        str[i] = c;
    str[i] = '\0';

    return i > 0 || c == '\n'  || c == '\r' ? str : NULL;
}

size_t fio_list(const char * dir, file_attr_t * attr, size_t n) {
    int ret;
    size_t i;

    xSemaphoreTake(fio_sem, portMAX_DELAY);
    ret = fs_mount(dir, &attr[0]);
    for (i = 1; i < n && ret; i++)
        ret = fs_mount(NULL, &attr[i]);
    xSemaphoreGive(fio_sem);

    return i;
}

#define stdin_hash 0x0BA00421
#define stdout_hash 0x7FA08308
#define stderr_hash 0x7FA058A3

static int devfs_open(void * opaque, const char * path, int flags, int mode) {
    uint32_t h = hash_djb2((const uint8_t *) path, -1);
//    DBGOUT("devfs_open(%p, \"%s\", %i, %i)\r\n", opaque, path, flags, mode);
    switch (h) {
    case stdin_hash:
        if (flags & (O_WRONLY | O_RDWR))
            return -1;
        return fio_open(stdin_read, NULL, NULL, NULL, NULL);
        break;
    case stdout_hash:
        if (flags & O_RDONLY)
            return -1;
        return fio_open(NULL, stdout_write, NULL, NULL, NULL);
        break;
    case stderr_hash:
        if (flags & O_RDONLY)
            return -1;
        return fio_open(NULL, stdout_write, NULL, NULL, NULL);
        break;
    }
    return -1;
}

void register_devfs() {
    DBGOUT("Registering devfs.\r\n");
    register_fs("dev", NULL, devfs_open, NULL);
}
