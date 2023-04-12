#include "tmpfs.h"
#include <string.h>
#include <memory/heap.h>

static int vfs_node_lookup(vfs_node_t *node, vfs_node_t **out, const char *name) {
    if(node->type != VFS_NODE_TYPE_DIRECTORY) return -1;
    tmpfs_node_t *tnode = (tmpfs_node_t *) node->data;

    if(strcmp(name, "..") == 0) {
        if(!tnode->parent) *out = node;
        else *out = tnode->parent->vfs_node;
        return 0;
    }

    tnode = tnode->children;
    while(tnode) {
        if(strcmp(tnode->name, name) == 0) return vfs_vget(node->vfs, out, (ino_t) tnode);
        tnode = tnode->next;
    }
    return -1;
}

static int make_node(vfs_node_t *node, vfs_node_t **out, const char *name, bool directory) {
    if(node->type != VFS_NODE_TYPE_DIRECTORY) return -1;
    if(!vfs_node_lookup(node, *out, name)) return -1;
    char *name_dupe = heap_alloc(sizeof(char) * (strlen(name) + 1));
    memcpy(name_dupe, name, strlen(name) + 1);

    tmpfs_node_t *parent = (tmpfs_node_t *) node->data;
    tmpfs_node_t *tnode = heap_alloc(sizeof(tmpfs_node_t));
    tnode->is_dir = directory;
    tnode->name = name_dupe;
    tnode->parent = parent;
    tnode->children = 0;
    tnode->next = parent->children;
    parent->children = tnode;

    return vfs_vget(node->vfs, out, (ino_t) tnode);
}

static int vfs_node_create(vfs_node_t *node, vfs_node_t **out, const char *name) {
    return make_node(node, out, name, false);
}

static int vfs_node_mkdir(vfs_node_t *node, vfs_node_t **out, const char *name) {
    return make_node(node, out, name, true);
}

static vfs_node_operations_t g_node_ops = {
    .lookup = vfs_node_lookup,
    .create = vfs_node_create,
    .mkdir = vfs_node_mkdir
};

static int vfs_root(vfs_t *vfs, vfs_node_t **out) {
    *out = (vfs_node_t *) vfs->data;
    return 0;
}

int vfs_mount(vfs_t *vfs, const char *path, void *data) {
    return -1;
}

int vfs_vget(vfs_t *vfs, vfs_node_t **out, ino_t inode) {
    tmpfs_node_t *tnode = (tmpfs_node_t *) inode;
    if(!tnode->vfs_node) {
        vfs_node_t *node = heap_alloc(sizeof(vfs_node_t));
        node->ops = &g_node_ops;
        node->type = tnode->is_dir ? VFS_NODE_TYPE_DIRECTORY : VFS_NODE_TYPE_REGULAR;
        node->vfs = vfs;
        node->data = (void *) tnode;
        tnode->vfs_node = node;
    }
    *out = tnode->vfs_node;
    return 0;
}

static vfs_operations_t g_ops = {
    .mount = vfs_mount,
    .root = vfs_root,
    .vget = vfs_vget
};

vfs_t *tmpfs_create() {
    vfs_t *vfs = heap_alloc(sizeof(vfs_t));
    memset(vfs, 0, sizeof(vfs_t));
    vfs->ops = &g_ops;

    tmpfs_node_t *tnode = heap_alloc(sizeof(tmpfs_node_t));
    tnode->is_dir = true;
    tnode->name = "TMPFS ROOT";
    tnode->next = 0;
    tnode->parent = 0;
    tnode->children = 0;

    vfs_node_t *node = heap_alloc(sizeof(vfs_node_t));
    node->ops = &g_node_ops;
    node->type = VFS_NODE_TYPE_DIRECTORY;
    node->vfs = vfs;
    node->data = (void *) tnode;

    tnode->vfs_node = node;
    vfs->data = (void *) node;
    return vfs;
}