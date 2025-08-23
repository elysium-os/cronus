#include "lib/list.h"

#include "common/assert.h"

static void list_initial_node(list_t *list, list_node_t *node) {
    ASSERT(list->count == 0);
    node->next = nullptr;
    node->prev = nullptr;
    list->head = node;
    list->tail = node;
    list->count = 1;
}

void list_push_front(list_t *list, list_node_t *node) {
    if(list->head == nullptr) {
        ASSERT(list->tail == nullptr);
        list_initial_node(list, node);
        return;
    }

    ASSERT(list->count != 0);
    list_node_prepend(list, list->head, node);
}

void list_push_back(list_t *list, list_node_t *node) {
    if(list->tail == nullptr) {
        ASSERT(list->head == nullptr);
        list_initial_node(list, node);
        return;
    }

    ASSERT(list->count != 0);
    list_node_append(list, list->tail, node);
}

[[gnu::alias("list_push_front")]] void list_push(list_t *list, list_node_t *node);

list_node_t *list_pop_front(list_t *list) {
    list_node_t *node = list->head;
    list_node_delete(list, node);
    return node;
}

list_node_t *list_pop_back(list_t *list) {
    list_node_t *node = list->tail;
    list_node_delete(list, node);
    return node;
}

[[gnu::alias("list_pop_front")]] list_node_t *list_pop(list_t *list);

void list_node_append(list_t *list, list_node_t *pos, list_node_t *node) {
    node->next = pos->next;
    node->prev = pos;

    pos->next = node;
    if(node->next != nullptr) node->next->prev = node;

    if(list->tail == pos) list->tail = node;
    list->count++;
}

void list_node_prepend(list_t *list, list_node_t *pos, list_node_t *node) {
    node->next = pos;
    node->prev = pos->prev;

    pos->prev = node;
    if(node->prev != nullptr) node->prev->next = node;

    if(list->head == pos) list->head = node;
    list->count++;
}

void list_node_delete(list_t *list, list_node_t *node) {
    list->count--;
    if(list->head == node) list->head = node->next;
    if(list->tail == node) list->tail = node->prev;

    if(node->prev != nullptr) node->prev->next = node->next;
    if(node->next != nullptr) node->next->prev = node->prev;
}
