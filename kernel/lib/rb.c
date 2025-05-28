#include "rb.h"

#include "common/assert.h"

#include <stddef.h>

#define GET_DIRECTION(NODE) ((NODE) == (NODE)->parent->right ? RB_DIRECTION_RIGHT : RB_DIRECTION_LEFT)
#define IS_RED(NODE) ((NODE) != nullptr && (NODE)->red)

typedef enum {
    RB_DIRECTION_LEFT,
    RB_DIRECTION_RIGHT,
} rb_direction_t;

/// Replace u with the subtree at v.
static void transplant(rb_tree_t *tree, rb_node_t *u, rb_node_t *v) {
    if(u->parent == nullptr) {
        ASSERT(tree->root == u);
        tree->root = v;
    } else {
        switch(GET_DIRECTION(u)) {
            case RB_DIRECTION_RIGHT: u->parent->right = v; break;
            case RB_DIRECTION_LEFT:  u->parent->left = v; break;
        }
    }
    if(v != nullptr) v->parent = u->parent;
}

static rb_node_t *rotate(rb_tree_t *tree, rb_node_t *node, rb_direction_t direction) {
    rb_node_t *rotation_parent = node->parent;
    rb_node_t *new_root = node->children[1 - direction];

    node->children[1 - direction] = new_root->children[direction];
    if(node->children[1 - direction] != nullptr) node->children[1 - direction]->parent = node;

    new_root->children[direction] = node;
    node->parent = new_root;

    new_root->parent = rotation_parent;
    if(rotation_parent != nullptr) {
        rotation_parent->children[node == rotation_parent->children[1]] = new_root;
    } else {
        tree->root = new_root;
    }

    return new_root;
}

static void rebalance_insert(rb_tree_t *tree, rb_node_t *node) {
    rb_node_t *parent = node->parent;
    if(parent == nullptr) return;

    do {
        if(!IS_RED(parent)) return;

        rb_node_t *grandparent = parent->parent;

        if(grandparent == nullptr) {
            parent->red = false;
            return;
        }

        rb_direction_t direction = GET_DIRECTION(parent);
        rb_node_t *uncle = grandparent->children[1 - direction];
        if(!uncle || !IS_RED(uncle)) {
            if(node == parent->children[1 - direction]) {
                rotate(tree, parent, direction);
                node = parent;
                parent = grandparent->children[direction];
            }

            rotate(tree, grandparent, 1 - direction);
            parent->red = false;
            grandparent->red = true;
            return;
        }

        parent->red = false;
        uncle->red = false;
        grandparent->red = true;
        node = grandparent;
    } while((parent = node->parent) != nullptr);

    tree->root->red = false;
}

void rb_insert(rb_tree_t *tree, rb_node_t *node) {
    node->left = nullptr;
    node->right = nullptr;
    node->parent = nullptr;
    node->red = true;

    rb_node_t *current = tree->root;
    rb_direction_t direction;

    rb_value_t node_value = tree->value(node);
    while(current != nullptr) {
        node->parent = current;

        rb_value_t current_value = tree->value(current);
        if(node_value < current_value) {
            direction = RB_DIRECTION_LEFT;
        } else {
            direction = RB_DIRECTION_RIGHT;
        }

        current = current->children[direction];
    }

    if(node->parent == nullptr) {
        tree->root = node;
        node->red = false;
        return;
    }
    node->parent->children[direction] = node;

    if(node->parent->parent == nullptr) return;

    rebalance_insert(tree, node);
}

void rb_remove(rb_tree_t *tree, rb_node_t *node) {
    bool original_is_red = IS_RED(node);

    rb_node_t *x, *x_p;
    if(node->left == nullptr) {
        x = node->right;
        x_p = node->parent;
        transplant(tree, node, x);
    } else if(node->right == nullptr) {
        x = node->left;
        x_p = node->parent;
        transplant(tree, node, x);
    } else {
        rb_node_t *successor = node->right;
        while(successor->left != nullptr) successor = successor->left;

        original_is_red = IS_RED(successor);

        x = successor->right;
        if(successor->parent == node) {
            x_p = successor;
        } else {
            x_p = successor->parent;
            transplant(tree, successor, x);
            successor->right = node->right;
            successor->right->parent = successor;
        }
        ASSERT(successor->left == nullptr);

        transplant(tree, node, successor);
        successor->left = node->left;
        successor->left->parent = successor;
        successor->red = IS_RED(node);
    }

    ASSERT(x_p == nullptr || x == x_p->left || x == x_p->right);

    if(original_is_red) return;

    while(x != tree->root && !IS_RED(x)) {
        if(x == x_p->left) {
            rb_node_t *w = x_p->right;
            if(IS_RED(w)) {
                w->red = false;
                x_p->red = true;
                rotate(tree, x_p, RB_DIRECTION_LEFT);
                ASSERT(x == x_p->left);
                w = x_p->right;
            }
            if(!IS_RED(w->left) && !IS_RED(w->right)) {
                w->red = true;
                x = x_p;
            } else {
                if(!IS_RED(w->right)) {
                    w->left->red = false;
                    w->red = true;
                    rotate(tree, w, RB_DIRECTION_RIGHT);
                    w = x_p->right;
                }
                w->red = IS_RED(x_p);
                x_p->red = false;
                w->right->red = false;
                rotate(tree, x_p, RB_DIRECTION_LEFT);
                x = tree->root;
            }
        } else {
            rb_node_t *w = x_p->left;
            if(IS_RED(w)) {
                w->red = false;
                x_p->red = true;
                rotate(tree, x_p, RB_DIRECTION_RIGHT);
                ASSERT(x == x_p->right);
                w = x_p->left;
            }
            if(!IS_RED(w->right) && !IS_RED(w->left)) {
                w->red = true;
                x = x_p;
            } else {
                if(!IS_RED(w->left)) {
                    w->right->red = false;
                    w->red = true;
                    rotate(tree, w, RB_DIRECTION_LEFT);
                    w = x_p->left;
                }
                w->red = IS_RED(x_p);
                x_p->red = false;
                w->left->red = false;
                rotate(tree, x_p, RB_DIRECTION_RIGHT);
                x = tree->root;
            }
        }
        x_p = x->parent;
    }
    if(x) x->red = false;
}

rb_node_t *rb_search(rb_tree_t *tree, rb_value_t search_value, rb_search_type_t search_type) {
    rb_node_t *nearest_node = nullptr;
    rb_value_t nearest_value = 0;

    for(rb_node_t *current = tree->root; current != nullptr;) {
        rb_value_t current_value = tree->value(current);

        switch(search_type) {
            case RB_SEARCH_TYPE_EXACT:
                if(current_value == search_value) return current;
                current = (search_value > current_value) ? current->right : current->left;
                break;

            case RB_SEARCH_TYPE_NEAREST:
                nearest_node = current;
                current = (search_value > current_value) ? current->right : current->left;
                break;

            case RB_SEARCH_TYPE_NEAREST_LT: [[fallthrough]];
            case RB_SEARCH_TYPE_NEAREST_LTE:
                if(current_value > search_value || (search_type == RB_SEARCH_TYPE_NEAREST_LT && current_value == search_value)) {
                    current = current->left;
                    continue;
                }

                if(nearest_node == nullptr || current_value > nearest_value) {
                    nearest_node = current;
                    nearest_value = current_value;
                }

                current = current->right;
                break;

            case RB_SEARCH_TYPE_NEAREST_GT: [[fallthrough]];
            case RB_SEARCH_TYPE_NEAREST_GTE:
                if(current_value < search_value || (search_type == RB_SEARCH_TYPE_NEAREST_GT && current_value == search_value)) {
                    current = current->right;
                    continue;
                }

                if(nearest_node == nullptr || current_value < nearest_value) {
                    nearest_node = current;
                    nearest_value = current_value;
                }

                current = current->left;
                break;
        }
    }
    return nearest_node;
}
