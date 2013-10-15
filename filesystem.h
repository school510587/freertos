#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include <stdint.h>
#include <hash-djb2.h>
#include "fattr.h"

#define MAX_FS 16

typedef int (*fs_open_t)(void * opaque, const char * fname, int flags, int mode);
typedef void * (*fs_mount_t)(void * mountpoint, file_attr_t * attr);

/* Need to be called before using any other fs functions */
__attribute__((constructor)) void fs_init();

int register_fs(const char * mountpoint, fs_mount_t, fs_open_t callback, void * opaque);
int fs_open(const char * path, int flags, int mode);
int fs_mount(const char * path, file_attr_t * attr);

#endif
