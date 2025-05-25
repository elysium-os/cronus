#pragma once

#include <stddef.h>

#define RB_TREE_INIT(VALUE_FN) ((rb_tree_t) { .value = (VALUE_FN), .root = nullptr })

typedef size_t rb_value_t;
typedef struct rb_node rb_node_t;

typedef enum {
    RB_SEARCH_TYPE_EXACT, /* Find an exact match */
    RB_SEARCH_TYPE_NEAREST, /* Find the nearest match */
    RB_SEARCH_TYPE_NEAREST_LT, /* Find the nearest match that is less than the search value */
    RB_SEARCH_TYPE_NEAREST_LTE, /* Find the nearest match that is less than or equals to the search value */
    RB_SEARCH_TYPE_NEAREST_GT, /* Find the nearest match that is greater than the search value */
    RB_SEARCH_TYPE_NEAREST_GTE, /* Find the nearest match that is greater than or equals to the search value */
} rb_search_type_t;

struct rb_node {
    bool red;
    rb_node_t *parent;
    union {
        struct {
            rb_node_t *left, *right;
        };
        rb_node_t *children[2];
    };
};

typedef struct {
    rb_value_t (*value)(rb_node_t *node);
    rb_node_t *root;
} rb_tree_t;

/// Insert a node into red black tree.
/// @note Only member preserved is `value`.
void rb_insert(rb_tree_t *tree, rb_node_t *node);

/// Remove a node from red black tree.
void rb_remove(rb_tree_t *tree, rb_node_t *node);

/// Binary search for a node.
/// @returns Pointer to found node or `nullptr`
rb_node_t *rb_search(rb_tree_t *tree, rb_value_t search_value, rb_search_type_t search_type);
