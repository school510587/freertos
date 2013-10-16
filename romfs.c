#include <string.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <unistd.h>
#include "fio.h"
#include "filesystem.h"
#include "romfs.h"
#include "osdebug.h"
#include "hash-djb2.h"

struct romfs_fds_t {
    const uint8_t * file;
    uint32_t cursor;
};

static struct romfs_fds_t romfs_fds[MAX_FDS];

static uint32_t get_unaligned(const uint8_t * d) {
    return ((uint32_t) d[0]) | ((uint32_t) (d[1] << 8)) | ((uint32_t) (d[2] << 16)) | ((uint32_t) (d[3] << 24));
}

static ssize_t romfs_read(void * opaque, void * buf, size_t count) {
    struct romfs_fds_t * f = (struct romfs_fds_t *) opaque;
    const uint8_t * size_p = f->file - 4;
    uint32_t size = get_unaligned(size_p);

    if ((f->cursor + count) > size)
        count = size - f->cursor;

    memcpy(buf, f->file + f->cursor, count);
    f->cursor += count;

    return count;
}

static off_t romfs_seek(void * opaque, off_t offset, int whence) {
    struct romfs_fds_t * f = (struct romfs_fds_t *) opaque;
    const uint8_t * size_p = f->file - 4;
    uint32_t size = get_unaligned(size_p);
    uint32_t origin;

    switch (whence) {
    case SEEK_SET:
        origin = 0;
        break;
    case SEEK_CUR:
        origin = f->cursor;
        break;
    case SEEK_END:
        origin = size;
        break;
    default:
        return -1;
    }

    offset = origin + offset;

    if (offset < 0)
        return -1;
    if (offset > size)
        offset = size;

    f->cursor = offset;

    return offset;
}

const uint8_t * romfs_get_file_by_hash(const uint8_t * romfs, uint32_t h) {
    const uint8_t * meta;
    size_t l;

    for (meta = romfs; get_unaligned(meta); meta += 4 + l) {
        uint32_t x = get_unaligned(meta);

        for (meta += 4; *meta; meta++);
        l = get_unaligned(++meta);
        if (x == h) {
            return meta + 4;
        }
    }

    return NULL;
}

static int romfs_open(void * opaque, const char * path, int flags, int mode) {
    uint32_t h = hash_djb2((const uint8_t *) path, -1);
    const uint8_t * romfs = (const uint8_t *) opaque;
    const uint8_t * file;
    int r = -1;

    file = romfs_get_file_by_hash(romfs, h);

    if (file) {
        r = fio_open(romfs_read, NULL, romfs_seek, NULL, NULL);
        if (r > 0) {
            romfs_fds[r].file = file;
            romfs_fds[r].cursor = 0;
            fio_set_opaque(r, romfs_fds + r);
        }
    }
    return r;
}

static void * romfs_mount(void * mountpoint, file_attr_t * attr) {
    uint8_t * p = (uint8_t *)mountpoint;

    if (!mountpoint || !attr)
        return NULL;

    attr->hash = get_unaligned(p);
    p += 4;
    attr->name = (const char *)p;
    p += strlen(attr->name) + 1;
    attr->size = get_unaligned(p);
    p += 4;
    attr->content = p;
    p += attr->size;

    return get_unaligned(p) ? p : NULL;
}

void register_romfs(const char * mountpoint, const uint8_t * romfs) {
//    DBGOUT("Registering romfs `%s' @ %p\r\n", mountpoint, romfs);
    register_fs(mountpoint, romfs_mount, romfs_open, (void *) romfs);
}
