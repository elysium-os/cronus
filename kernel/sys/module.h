#pragma once

#include "fs/vfs.h"
#include "lib/list.h"

#include <stddef.h>

typedef enum {
    MODULE_RESULT_OK,
    MODULE_RESULT_ERR_FS,
    MODULE_RESULT_ERR_VM,
    MODULE_RESULT_ERR_NO_KERNEL_SYMBOLS,
    MODULE_RESULT_ERR_NOT_MODULE,
    MODULE_RESULT_ERR_INVALID_TYPE,
    MODULE_RESULT_ERR_INVALID_CLASS,
    MODULE_RESULT_ERR_INVALID_ENCODING,
    MODULE_RESULT_ERR_INVALID_MACHINE,
    MODULE_RESULT_ERR_INVALID_RELOCATION,
    MODULE_RESULT_ERR_MISSING_SYMTAB,
    MODULE_RESULT_ERR_UNRESOLVED_SYMBOL,
    MODULE_RESULT_ERR_UNSUPPORTED
} module_result_t;

typedef struct {
    void *base;
    size_t size;
    list_node_t list_node;
} module_region_t;

typedef struct {
    void (*initialize)();
    void (*uninitialize)();
    list_t module_regions;
} module_t;

/**
 * @brief Load a kernel module.
 */
module_result_t module_load(vfs_node_t *module_file, PARAM_OUT(module_t **) module);

/**
 * @brief Return the string version of a module result.
 */
const char *module_result_stringify(module_result_t result);
