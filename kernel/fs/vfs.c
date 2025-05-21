#include "vfs.h"

#include "lib/list.h"
#include "lib/mem.h"
#include "memory/heap.h"

list_t g_vfs_all = LIST_INIT;

vfs_result_t vfs_mount(vfs_ops_t *vfs_ops, const char *path, void *private_data) {
    vfs_t *vfs = heap_alloc(sizeof(vfs_t));
    vfs->ops = vfs_ops;
    vfs->private_data = private_data;
    vfs->ops->mount(vfs);

    if(g_vfs_all.count == 0) {
        if(path != nullptr) {
            heap_free(vfs, sizeof(vfs_t));
            return VFS_RESULT_ERR_NOT_FOUND;
        }
        vfs->mount_point = nullptr;
    } else {
        vfs_node_t *node;
        vfs_result_t res = vfs_lookup(&VFS_ABSOLUTE_PATH(path), &node);
        if(res != VFS_RESULT_OK) {
            heap_free(vfs, sizeof(vfs_t));
            return res;
        }
        if(node->type != VFS_NODE_TYPE_DIR) {
            heap_free(vfs, sizeof(vfs_t));
            return VFS_RESULT_ERR_NOT_DIR;
        }
        if(node->mounted_vfs != nullptr) {
            heap_free(vfs, sizeof(vfs_t));
            return VFS_RESULT_ERR_EXISTS;
        }
        node->mounted_vfs = vfs;
        vfs->mount_point = node;
    }
    list_push_back(&g_vfs_all, &vfs->list_node);
    return VFS_RESULT_OK;
}

vfs_result_t vfs_root(PARAM_OUT(vfs_node_t **) root_node) {
    if(g_vfs_all.count == 0) return VFS_RESULT_ERR_NOT_FOUND;
    vfs_t *vfs = LIST_CONTAINER_GET(g_vfs_all.head, vfs_t, list_node);
    return vfs->ops->root_node(vfs, root_node);
}

vfs_result_t vfs_lookup_ext(vfs_path_t *path, vfs_lookup_create_t create_mode, bool exclusive, PARAM_OUT(vfs_node_t **) found_node) {
    int comp_start = 0, comp_end = 0;

    vfs_node_t *current_node = path->root;
    if(path->relative_path[comp_end] == '/' || current_node == nullptr) {
        vfs_result_t res = vfs_root(&current_node);
        if(res != VFS_RESULT_OK) return res;
        comp_start++, comp_end++;
    }
    if(current_node == nullptr) return VFS_RESULT_ERR_NOT_FOUND;

    do {
        bool last_comp = false;
        switch(path->relative_path[comp_end]) {
            case 0: last_comp = true; [[fallthrough]];
            case '/':
                if(comp_start == comp_end) {
                    comp_start++;
                    break;
                }
                int comp_length = comp_end - comp_start;
                char *component = heap_alloc(comp_length + 1);
                memcpy(component, path->relative_path + comp_start, comp_length);
                component[comp_length] = 0;
                comp_start = comp_end + 1;

                vfs_node_t *next_node;
                vfs_result_t res = current_node->ops->lookup(current_node, component, &next_node);
                if(res == VFS_RESULT_OK) {
                    if(next_node == nullptr) {
                        if(current_node->vfs->mount_point != nullptr) {
                            res = current_node->ops->lookup(current_node, "..", &next_node); // TODO: hmm?
                        }
                    } else {
                        current_node = next_node;
                    }
                }

                if(res == VFS_RESULT_ERR_NOT_FOUND && last_comp) {
                    switch(create_mode) {
                        case VFS_LOOKUP_CREATE_FILE: res = current_node->ops->mkfile(current_node, component, &current_node); break;
                        case VFS_LOOKUP_CREATE_DIR:  res = current_node->ops->mkdir(current_node, component, &current_node); break;
                        case VFS_LOOKUP_CREATE_NONE: heap_free(component, comp_length + 1); break;
                    }
                } else {
                    if(exclusive) res = VFS_RESULT_ERR_EXISTS;
                    heap_free(component, comp_length + 1);
                }

                if(res != VFS_RESULT_OK) return res;
                break;
        }

        if(current_node->mounted_vfs == nullptr) continue;
        vfs_result_t res = current_node->mounted_vfs->ops->root_node(current_node->mounted_vfs, &current_node);
        if(res != VFS_RESULT_OK) return res;
    } while(path->relative_path[comp_end++]);

    *found_node = current_node;
    return VFS_RESULT_OK;
}

vfs_result_t vfs_lookup(vfs_path_t *path, PARAM_OUT(vfs_node_t **) found_node) {
    return vfs_lookup_ext(path, VFS_LOOKUP_CREATE_NONE, false, found_node);
}

vfs_result_t vfs_rw(vfs_path_t *path, vfs_rw_t *rw, PARAM_OUT(size_t *) rw_count) {
    vfs_node_t *node;
    vfs_result_t res = vfs_lookup(path, &node);
    if(res != VFS_RESULT_OK) return res;
    return node->ops->rw(node, rw, rw_count);
}

vfs_result_t vfs_mkdir(vfs_path_t *path, const char *name, PARAM_OUT(vfs_node_t **) new_node) {
    vfs_node_t *node;
    vfs_result_t res = vfs_lookup(path, &node);
    if(res != VFS_RESULT_OK) return res;
    return node->ops->mkdir(node, name, new_node);
}

vfs_result_t vfs_mkfile(vfs_path_t *path, const char *name, PARAM_OUT(vfs_node_t **) new_node) {
    vfs_node_t *node;
    vfs_result_t res = vfs_lookup(path, &node);
    if(res != VFS_RESULT_OK) return res;
    return node->ops->mkfile(node, name, new_node);
}
