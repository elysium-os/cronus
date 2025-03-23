#include "rdsk.h"

#include "lib/mem.h"
#include "lib/string.h"
#include "memory/heap.h"
#include "vfs.h"

#include <stdint.h>

#define SUPPORTED_REVISION ((1 << 8) | 1)

#define INFO(VFS) ((info_t *) (VFS)->private_data)
#define FILE(NODE) ((rdsk_file_t *) (NODE)->private_data)
#define DIR(NODE) ((rdsk_dir_t *) (NODE)->private_data)

typedef uint64_t rdsk_index_t;

typedef struct [[gnu::packed]] {
    char signature[4];
    uint16_t revision;
    uint16_t header_size;
    uint64_t root_index;
    uint64_t nametable_offset;
    uint64_t nametable_size;
    uint16_t dirtable_entry_size;
    uint64_t dirtable_entry_count;
    uint64_t dirtable_offset;
    uint16_t filetable_entry_size;
    uint64_t filetable_entry_count;
    uint64_t filetable_offset;
} rdsk_header_t;

typedef struct [[gnu::packed]] {
    bool used;
    uint64_t nametable_offset;
    uint64_t data_offset;
    uint64_t size;
    uint64_t next_index;
    uint64_t parent_index;
} rdsk_file_t;

typedef struct [[gnu::packed]] {
    bool used;
    uint64_t nametable_offset;
    uint64_t filetable_index;
    uint64_t dirtable_index;
    uint64_t next_index;
    uint64_t parent_index;
} rdsk_dir_t;

typedef struct {
    rdsk_header_t *header;
    vfs_node_t **dir_cache;
    uint64_t dir_cache_size;
    vfs_node_t **file_cache;
    uint64_t file_cache_size;
} info_t;

static vfs_node_ops_t g_node_ops;

static const char *get_name(vfs_t *vfs, uint64_t offset) {
    info_t *info = (info_t *) vfs->private_data;
    return (const char *) ((uintptr_t) info->header + info->header->nametable_offset + offset);
}

static rdsk_dir_t *get_dir(vfs_t *vfs, rdsk_index_t index) {
    info_t *info = (info_t *) vfs->private_data;
    return (rdsk_dir_t *) ((uintptr_t) info->header + info->header->dirtable_offset + (index - 1) * info->header->dirtable_entry_size);
}

static rdsk_file_t *get_file(vfs_t *vfs, rdsk_index_t index) {
    info_t *info = (info_t *) vfs->private_data;
    return (rdsk_file_t *) ((uintptr_t) info->header + info->header->filetable_offset + (index - 1) * info->header->filetable_entry_size);
}

static rdsk_index_t get_dir_index(vfs_t *vfs, rdsk_dir_t *dir) {
    info_t *info = (info_t *) vfs->private_data;
    return ((uintptr_t) dir - ((uintptr_t) info->header + info->header->dirtable_offset)) / info->header->dirtable_entry_size + 1;
}

static rdsk_index_t get_file_index(vfs_t *vfs, rdsk_file_t *file) {
    info_t *info = (info_t *) vfs->private_data;
    return ((uintptr_t) file - ((uintptr_t) info->header + info->header->filetable_offset)) / info->header->filetable_entry_size + 1;
}

static vfs_node_t *get_dir_vfs_node(vfs_t *vfs, rdsk_index_t index) {
    info_t *info = (info_t *) vfs->private_data;
    if(info->dir_cache[index - 1]) return info->dir_cache[index - 1];
    vfs_node_t *node = heap_alloc(sizeof(vfs_node_t));
    memset(node, 0, sizeof(vfs_node_t));
    node->vfs = vfs;
    node->type = VFS_NODE_TYPE_DIR;
    node->private_data = get_dir(vfs, index);
    node->ops = &g_node_ops;
    info->dir_cache[index - 1] = node;
    return node;
}

static vfs_node_t *get_file_vfs_node(vfs_t *vfs, rdsk_index_t index) {
    info_t *info = (info_t *) vfs->private_data;
    if(info->file_cache[index - 1]) return info->file_cache[index - 1];
    vfs_node_t *node = heap_alloc(sizeof(vfs_node_t));
    memset(node, 0, sizeof(vfs_node_t));
    node->vfs = vfs;
    node->type = VFS_NODE_TYPE_FILE;
    node->private_data = get_file(vfs, index);
    node->ops = &g_node_ops;
    info->file_cache[index - 1] = node;
    return node;
}

static rdsk_dir_t *find_dir(vfs_t *vfs, rdsk_dir_t *dir, char *name) {
    rdsk_index_t curindex = dir->dirtable_index;
    while(curindex != 0) {
        rdsk_dir_t *curdir = get_dir(vfs, curindex);
        curindex = curdir->next_index;
        if(!string_eq(name, get_name(vfs, curdir->nametable_offset))) continue;
        return curdir;
    }
    return NULL;
}

static rdsk_file_t *find_file(vfs_t *vfs, rdsk_dir_t *dir, char *name) {
    rdsk_index_t curindex = dir->filetable_index;
    while(curindex != 0) {
        rdsk_file_t *curfile = get_file(vfs, curindex);
        curindex = curfile->next_index;
        if(!string_eq(name, get_name(vfs, curfile->nametable_offset))) continue;
        return curfile;
    }
    return NULL;
}

static vfs_result_t rdsk_rw(vfs_node_t *node, vfs_rw_t *rw, PARAM_OUT(size_t *) rw_count) {
    if(node->type != VFS_NODE_TYPE_FILE) return VFS_RESULT_ERR_NOT_FILE;
    if(rw->rw == VFS_RW_WRITE) return VFS_RESULT_ERR_READ_ONLY_FS;

    if(rw->offset >= FILE(node)->size) {
        *rw_count = 0;
        return VFS_RESULT_OK;
    }
    size_t count = FILE(node)->size - rw->offset;
    if(count > rw->size) count = rw->size;
    memcpy(rw->buffer, (void *) (((uintptr_t) INFO(node->vfs)->header + FILE(node)->data_offset) + rw->offset), count);
    *rw_count = count;
    return VFS_RESULT_OK;
}

static vfs_result_t rdsk_attr(vfs_node_t *node, vfs_node_attr_t *attr) {
    attr->size = node->type == VFS_NODE_TYPE_FILE ? FILE(node)->size : 0;
    return VFS_RESULT_OK;
}

static const char *rdsk_name(vfs_node_t *node) {
    const char *name;
    switch(node->type) {
        case VFS_NODE_TYPE_DIR:  name = get_name(node->vfs, DIR(node)->nametable_offset);
        case VFS_NODE_TYPE_FILE: name = get_name(node->vfs, FILE(node)->nametable_offset);
    }
    return name;
}

static vfs_result_t rdsk_lookup(vfs_node_t *node, char *name, PARAM_OUT(vfs_node_t **) found_node) {
    if(node->type != VFS_NODE_TYPE_DIR) return VFS_RESULT_ERR_NOT_DIR;

    if(string_eq(name, ".")) {
        *found_node = node;
        return VFS_RESULT_OK;
    }

    if(string_eq(name, "..")) {
        if(DIR(node)->parent_index != 0) {
            *found_node = get_dir_vfs_node(node->vfs, DIR(node)->parent_index);
        } else {
            *found_node = NULL;
        }
        return VFS_RESULT_OK;
    }

    rdsk_file_t *found_file = find_file(node->vfs, DIR(node), name);
    if(found_file != NULL) {
        *found_node = get_file_vfs_node(node->vfs, get_file_index(node->vfs, found_file));
        return VFS_RESULT_OK;
    }

    rdsk_dir_t *found_dir = find_dir(node->vfs, DIR(node), name);
    if(found_dir != NULL) {
        *found_node = get_dir_vfs_node(node->vfs, get_dir_index(node->vfs, found_dir));
        return VFS_RESULT_OK;
    }

    return VFS_RESULT_ERR_NOT_FOUND;
}

static vfs_result_t rdsk_readdir(vfs_node_t *node, PARAM_INOUT(size_t *) offset, PARAM_OUT(const char **) dirent_name) {
    if(node->type != VFS_NODE_TYPE_DIR) return VFS_RESULT_ERR_NOT_DIR;

    int local_offset = *offset;
    rdsk_header_t *header = INFO(node->vfs)->header;
    if(local_offset < (int) header->filetable_entry_count) {
        rdsk_index_t index = DIR(node)->filetable_index;
        for(int i = 0; i < local_offset && index != 0; i++) index = get_file(node->vfs, index)->next_index;
        if(index == 0) {
            local_offset = header->filetable_entry_count;
        } else {
            *dirent_name = (char *) get_name(node->vfs, get_file(node->vfs, index)->nametable_offset);
            *offset = local_offset + 1;
            return VFS_RESULT_OK;
        }
    }

    rdsk_index_t index = DIR(node)->dirtable_index;
    for(int i = 0; i < local_offset - (int) header->filetable_entry_count && index != 0; i++) index = get_dir(node->vfs, index)->next_index;
    if(index == 0) {
        *dirent_name = NULL;
        return VFS_RESULT_OK;
    }

    *dirent_name = (char *) get_name(node->vfs, get_dir(node->vfs, index)->nametable_offset);
    *offset = local_offset + 1;
    return VFS_RESULT_OK;
}

static vfs_result_t rdsk_mkdir([[maybe_unused]] vfs_node_t *node, [[maybe_unused]] const char *name, [[maybe_unused]] PARAM_OUT(vfs_node_t **) new_node) {
    return VFS_RESULT_ERR_READ_ONLY_FS;
}

static vfs_result_t rdsk_mkfile([[maybe_unused]] vfs_node_t *node, [[maybe_unused]] const char *name, [[maybe_unused]] PARAM_OUT(vfs_node_t **) new_node) {
    return VFS_RESULT_ERR_READ_ONLY_FS;
}

static vfs_result_t rdsk_truncate([[maybe_unused]] vfs_node_t *node, [[maybe_unused]] size_t length) {
    return VFS_RESULT_ERR_READ_ONLY_FS;
}

static vfs_result_t rdsk_mount(vfs_t *vfs) {
    rdsk_header_t *header = (rdsk_header_t *) vfs->private_data;
    if(header->revision > SUPPORTED_REVISION) return VFS_RESULT_ERR_UNSUPPORTED;

    info_t *info = heap_alloc(sizeof(info_t));
    info->header = header;
    info->dir_cache_size = header->dirtable_entry_count;
    info->file_cache_size = header->filetable_entry_count;
    info->dir_cache = heap_alloc(sizeof(vfs_node_t *) * info->dir_cache_size);
    memset(info->dir_cache, 0, sizeof(vfs_node_t *) * info->dir_cache_size);
    info->file_cache = heap_alloc(sizeof(vfs_node_t *) * info->file_cache_size);
    memset(info->file_cache, 0, sizeof(vfs_node_t *) * info->file_cache_size);

    vfs->private_data = info;
    return VFS_RESULT_OK;
}

static vfs_result_t rdsk_root(vfs_t *vfs, PARAM_OUT(vfs_node_t **) root_node) {
    *root_node = get_dir_vfs_node(vfs, INFO(vfs)->header->root_index);
    return VFS_RESULT_OK;
}

static vfs_node_ops_t g_node_ops =
    {.attr = rdsk_attr, .name = rdsk_name, .lookup = rdsk_lookup, .rw = rdsk_rw, .mkdir = rdsk_mkdir, .readdir = rdsk_readdir, .mkfile = rdsk_mkfile, .truncate = rdsk_truncate
};

vfs_ops_t g_rdsk_ops = {.mount = rdsk_mount, .root_node = rdsk_root};
