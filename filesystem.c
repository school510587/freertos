#include "osdebug.h"
#include "filesystem.h"
#include "fio.h"

#include <stdint.h>
#include <string.h>
#include <hash-djb2.h>

#define MAX_FS 16

struct fs_t {
    uint32_t hash;
    fs_mount_t mount;
    fs_open_t cb;
    void * opaque;
};

static struct fs_t fss[MAX_FS];
static struct fs_t mount_data;

__attribute__((constructor)) void fs_init() {
    memset(fss, 0, sizeof(fss));
}

int register_fs(const char * mountpoint, fs_mount_t fm_cb, fs_open_t callback, void * opaque) {
    int i;
    DBGOUT("register_fs(\"%s\", %p, %p)\r\n", mountpoint, callback, opaque);

    for (i = 0; i < MAX_FS; i++) {
        if (!fss[i].cb) {
            fss[i].hash = hash_djb2((const uint8_t *) mountpoint, -1);
            fss[i].mount = fm_cb;
            fss[i].cb = callback;
            fss[i].opaque = opaque;
            return 0;
        }
    }

    return -1;
}

int fs_open(const char * path, int flags, int mode) {
    const char * slash;
    uint32_t hash;
    int i;
//    DBGOUT("fs_open(\"%s\", %i, %i)\r\n", path, flags, mode);

    while (path[0] == '/')
        path++;

    slash = strchr(path, '/');

    if (!slash)
        return -2;

    hash = hash_djb2((const uint8_t *) path, slash - path);
    path = slash + 1;

    for (i = 0; i < MAX_FS; i++) {
        if (fss[i].hash == hash)
            return fss[i].cb(fss[i].opaque, path, flags, mode);
    }

    return -2;
}

int fs_mount(const char * path, file_attr_t * attr) {
    if (path) {
        const char * slash;
        uint32_t hash;
        int i;

        while (path[0] == '/')
            path++;

        slash = strchr(path, '/');
        if (!slash)
            slash = path + strlen(path);

        hash = hash_djb2((const uint8_t *) path, slash - path);
        memset(&mount_data, 0, sizeof(mount_data));
        for (i = 0; i < MAX_FS; i++) {
            if (fss[i].hash == hash) {
                memcpy(&mount_data, &fss[i], sizeof(fss[i]));
                break;
            }
        }
    }
    mount_data.opaque = mount_data.mount(mount_data.opaque, attr);

    return mount_data.opaque ? 1 : 0;
}
