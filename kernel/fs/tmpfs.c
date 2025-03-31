#include "tmpfs.h"

#include "common/assert.h"
#include "common/log.h"
#include "fs/vfs.h"
#include "lib/mem.h"
#include "lib/string.h"
#include "memory/heap.h"

#include <stddef.h>
#include <stdint.h>

#define INFO(VFS) ((tmpfs_info_t *) (VFS)->private_data)
#define TMPFS_NODE(VNODE) ((tmpfs_node_t *) (VNODE)->private_data)

typedef struct tmpfs_info tmpfs_info_t;
typedef struct tmpfs_node tmpfs_node_t;
typedef struct tmpfs_file tmpfs_file_t;

struct tmpfs_info {
    tmpfs_node_t *root;
    uint64_t id_counter;
};

struct tmpfs_node {
    uint64_t id;
    const char *name;
    vfs_node_t *vnode;
    tmpfs_node_t *parent, *sibling;
    union {
        struct {
            tmpfs_node_t *children;
        } dir;
        tmpfs_file_t *file;
    };
};

struct tmpfs_file {
    void *base;
    size_t size;
};

static vfs_node_ops_t g_tmpfs_node_ops;

static tmpfs_node_t *make_tmpfs_node(tmpfs_node_t *parent, vfs_t *vfs, bool is_dir, const char *name) {
    tmpfs_node_t *tmpfs_node = heap_alloc(sizeof(tmpfs_node_t));
    tmpfs_node->name = name;
    tmpfs_node->id = ++INFO(vfs)->id_counter;
    tmpfs_node->parent = parent;

    tmpfs_node->sibling = NULL;
    if(parent != NULL) {
        tmpfs_node->sibling = parent->dir.children;
        parent->dir.children = tmpfs_node;
    }

    if(is_dir) {
        tmpfs_node->dir.children = NULL;
    } else {
        tmpfs_node->file = heap_alloc(sizeof(tmpfs_file_t));
        tmpfs_node->file->size = 0;
        tmpfs_node->file->base = NULL;
    }

    vfs_node_t *vnode = heap_alloc(sizeof(vfs_node_t));
    vnode->vfs = vfs;
    vnode->type = is_dir ? VFS_NODE_TYPE_DIR : VFS_NODE_TYPE_FILE;
    vnode->mounted_vfs = NULL;
    vnode->private_data = tmpfs_node;
    vnode->ops = &g_tmpfs_node_ops;
    tmpfs_node->vnode = vnode;

    return tmpfs_node;
}

static tmpfs_node_t *dir_find(tmpfs_node_t *dir_node, const char *name) {
    ASSERT(dir_node->vnode->type == VFS_NODE_TYPE_DIR);
    tmpfs_node_t *node = dir_node->dir.children;
    while(node != NULL) {
        if(string_eq(node->name, name)) return node;
        node = node->sibling;
    }
    return NULL;
}

static vfs_result_t tmpfs_rw(vfs_node_t *node, vfs_rw_t *rw, PARAM_OUT(size_t *) rw_count) {
    if(node->type != VFS_NODE_TYPE_FILE) return VFS_RESULT_ERR_NOT_FILE;

    tmpfs_file_t *file = TMPFS_NODE(node)->file;
    *rw_count = 0;
    switch(rw->rw) {
        case VFS_RW_READ:
            if(rw->offset >= file->size) return VFS_RESULT_OK;
            size_t count = file->size - rw->offset;
            if(count > rw->size) count = rw->size;
            memcpy(rw->buffer, file->base + rw->offset, count);
            *rw_count = count;
            break;
        case VFS_RW_WRITE:
            if(rw->offset + rw->size > file->size) {
                file->base = heap_realloc(file->base, file->size, rw->offset + rw->size);
                file->size = rw->offset + rw->size;
            }
            memcpy(file->base + rw->offset, rw->buffer, rw->size);
            *rw_count = rw->size;
            break;
    }
    return VFS_RESULT_OK;
}

static vfs_result_t tmpfs_attr(vfs_node_t *node, vfs_node_attr_t *attr) {
    attr->size = node->type == VFS_NODE_TYPE_FILE ? TMPFS_NODE(node)->file->size : 0;
    return VFS_RESULT_OK;
}

static const char *tmpfs_name(vfs_node_t *node) {
    return TMPFS_NODE(node)->name;
}

static vfs_result_t tmpfs_lookup(vfs_node_t *node, char *name, PARAM_OUT(vfs_node_t **) found_node) {
    if(node->type != VFS_NODE_TYPE_DIR) return VFS_RESULT_ERR_NOT_DIR;

    if(string_eq(name, ".")) {
        *found_node = node;
        return VFS_RESULT_OK;
    }

    if(string_eq(name, "..")) {
        tmpfs_node_t *parent_node = TMPFS_NODE(node)->parent;
        if(parent_node != NULL) {
            *found_node = parent_node->vnode;
        } else {
            *found_node = NULL;
        }
        return VFS_RESULT_OK;
    }

    tmpfs_node_t *tmpfs_node = dir_find(TMPFS_NODE(node), name);
    if(tmpfs_node == NULL) return VFS_RESULT_ERR_NOT_FOUND;
    *found_node = tmpfs_node->vnode;
    return VFS_RESULT_OK;
}

static vfs_result_t tmpfs_readdir(vfs_node_t *node, PARAM_INOUT(size_t *) offset, PARAM_OUT(const char **) dirent_name) {
    if(node->type != VFS_NODE_TYPE_DIR) return VFS_RESULT_ERR_NOT_DIR;

    tmpfs_node_t *tmpfs_node = TMPFS_NODE(node)->dir.children;
    for(size_t i = 0; i < *offset && tmpfs_node != NULL; i++) tmpfs_node = tmpfs_node->sibling;
    if(tmpfs_node != NULL) {
        *dirent_name = tmpfs_node->name;
    } else {
        *dirent_name = NULL;
    }
    (*offset)++;
    return VFS_RESULT_OK;
}

static vfs_result_t tmpfs_mkdir(vfs_node_t *node, const char *name, PARAM_OUT(vfs_node_t **) new_node) {
    if(node->type != VFS_NODE_TYPE_DIR) return VFS_RESULT_ERR_NOT_DIR;
    if(dir_find(TMPFS_NODE(node), name) != NULL) return VFS_RESULT_ERR_EXISTS;

    *new_node = make_tmpfs_node(TMPFS_NODE(node), node->vfs, true, name)->vnode;
    return VFS_RESULT_OK;
}

static vfs_result_t tmpfs_mkfile(vfs_node_t *node, const char *name, PARAM_OUT(vfs_node_t **) new_node) {
    if(node->type != VFS_NODE_TYPE_DIR) return VFS_RESULT_ERR_NOT_DIR;
    if(dir_find(TMPFS_NODE(node), name) != NULL) return VFS_RESULT_ERR_EXISTS;

    *new_node = make_tmpfs_node(TMPFS_NODE(node), node->vfs, false, name)->vnode;
    return VFS_RESULT_OK;
}

static vfs_result_t tmpfs_truncate(vfs_node_t *node, size_t length) {
    if(node->type != VFS_NODE_TYPE_FILE) return VFS_RESULT_ERR_NOT_FILE;

    tmpfs_file_t *file = TMPFS_NODE(node)->file;
    void *buffer = NULL;
    if(length > 0) {
        buffer = heap_alloc(length);
        memset(buffer, 0, length);
        if(file->base != NULL) memcpy(buffer, file->base, file->size < length ? file->size : length);
    }

    if(file->base != NULL) heap_free(file->base, file->size);
    file->base = buffer;
    file->size = length;
    return VFS_RESULT_OK;
}

static vfs_result_t tmpfs_mount(vfs_t *vfs) {
    tmpfs_info_t *info = heap_alloc(sizeof(tmpfs_info_t));
    vfs->private_data = info;
    info->id_counter = 1;
    info->root = make_tmpfs_node(NULL, vfs, true, "root");
    return VFS_RESULT_OK;
}

static vfs_result_t tmpfs_root(vfs_t *vfs, PARAM_OUT(vfs_node_t **) root_node) {
    *root_node = INFO(vfs)->root->vnode;
    return VFS_RESULT_OK;
}

static vfs_node_ops_t g_tmpfs_node_ops = { .rw = tmpfs_rw, .attr = tmpfs_attr, .name = tmpfs_name, .lookup = tmpfs_lookup, .readdir = tmpfs_readdir, .mkdir = tmpfs_mkdir, .mkfile = tmpfs_mkfile, .truncate = tmpfs_truncate };

vfs_ops_t g_tmpfs_ops = { .mount = tmpfs_mount, .root_node = tmpfs_root };
