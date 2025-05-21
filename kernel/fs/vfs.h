#pragma once

#include "lib/list.h"
#include "lib/param.h"

#include <stddef.h>

#define VFS_ABSOLUTE_PATH(PATH) ((vfs_path_t) { .root = nullptr, .relative_path = (PATH) })

typedef struct vfs vfs_t;
typedef struct vfs_ops vfs_ops_t;
typedef struct vfs_node vfs_node_t;
typedef struct vfs_node_ops vfs_node_ops_t;
typedef struct vfs_path vfs_path_t;

typedef enum {
    VFS_RESULT_OK,
    VFS_RESULT_ERR_UNSUPPORTED,
    VFS_RESULT_ERR_NOT_FILE,
    VFS_RESULT_ERR_NOT_DIR,
    VFS_RESULT_ERR_NOT_FOUND,
    VFS_RESULT_ERR_EXISTS,
    VFS_RESULT_ERR_READ_ONLY_FS
} vfs_result_t;

typedef enum {
    VFS_LOOKUP_CREATE_NONE,
    VFS_LOOKUP_CREATE_FILE,
    VFS_LOOKUP_CREATE_DIR,
} vfs_lookup_create_t;

typedef enum {
    VFS_NODE_TYPE_FILE,
    VFS_NODE_TYPE_DIR
} vfs_node_type_t;

struct vfs {
    vfs_ops_t *ops;
    void *private_data;
    vfs_node_t *mount_point;
    list_node_t list_node;
};

struct vfs_node {
    vfs_t *vfs;
    vfs_t *mounted_vfs;
    vfs_node_ops_t *ops;
    vfs_node_type_t type;
    void *private_data;
};

struct vfs_path {
    struct vfs_node *root;
    const char *relative_path;
};

typedef struct {
    enum {
        VFS_RW_READ,
        VFS_RW_WRITE
    } rw;
    size_t offset;
    size_t size;
    void *buffer;
} vfs_rw_t;

typedef struct {
    size_t size;
} vfs_node_attr_t;

struct vfs_ops {
    /**
     * @brief Called on VFS mount.
     */
    vfs_result_t (*mount)(vfs_t *vfs);

    /**
     * @brief Retrieves root node from the VFS.
     */
    vfs_result_t (*root_node)(vfs_t *vfs, PARAM_OUT(vfs_node_t **) root_node);
};

struct vfs_node_ops {
    /**
     * @brief Read/write a file.
     * @param rw_count bytes read/written
     */
    vfs_result_t (*rw)(vfs_node_t *node, vfs_rw_t *rw, PARAM_OUT(size_t *) rw_count);

    /**
     * @brief Retrieve node attributes.
     * @param attr attributes struct to populate
     */
    vfs_result_t (*attr)(vfs_node_t *node, vfs_node_attr_t *attr);

    /**
     * @brief Retrieve node name.
     * @warn The string is only safe for as long as the refcount is kept.
     */
    const char *(*name)(vfs_node_t *node);

    /**
     * @brief Look up a node by name.
     * @param found_node set to looked up node, nullptr on parent lookup of fs root
     */
    vfs_result_t (*lookup)(vfs_node_t *node, char *name, PARAM_OUT(vfs_node_t **) found_node);

    /**
     * @brief Read the next entry in a directory.
     * @note Offset is incremented to the next entry. If dirent_name is nullptr, it is the last entry.
     */
    vfs_result_t (*readdir)(vfs_node_t *node, PARAM_INOUT(size_t *) offset, PARAM_OUT(const char **) dirent_name);

    /**
     * @brief Create a new directory in a directory.
     */
    vfs_result_t (*mkdir)(vfs_node_t *node, const char *name, PARAM_OUT(vfs_node_t **) new_node);

    /**
     * @brief Create a new file in a directory.
     */
    vfs_result_t (*mkfile)(vfs_node_t *node, const char *name, PARAM_OUT(vfs_node_t **) new_node);

    /**
     * @brief Truncate a file.
     */
    vfs_result_t (*truncate)(vfs_node_t *node, size_t length);
};

extern list_t g_vfs_all;

/**
 * @brief Mount a VFS at path.
 */
vfs_result_t vfs_mount(vfs_ops_t *vfs_ops, const char *path, void *private_data);

/**
 * @brief Gets the root node of the entire VFS.
 */
vfs_result_t vfs_root(PARAM_OUT(vfs_node_t **) root_node);

/**
 * @brief Extended lookup a node at path.
 */
vfs_result_t vfs_lookup_ext(vfs_path_t *path, vfs_lookup_create_t create_mode, bool exclusive, PARAM_OUT(vfs_node_t **) found_node);

/**
 * @brief Lookup node at path.
 */
vfs_result_t vfs_lookup(vfs_path_t *path, PARAM_OUT(vfs_node_t **) found_node);

/**
 * @brief Read/write file at path.
 * @param rw_count bytes read/written
 */
vfs_result_t vfs_rw(vfs_path_t *path, vfs_rw_t *rw, PARAM_OUT(size_t *) rw_count);

/**
 * @brief Create directory at path.
 */
vfs_result_t vfs_mkdir(vfs_path_t *path, const char *name, PARAM_OUT(vfs_node_t **) new_node);

/**
 * @brief Create file at path.
 */
vfs_result_t vfs_mkfile(vfs_path_t *path, const char *name, PARAM_OUT(vfs_node_t **) new_node);
