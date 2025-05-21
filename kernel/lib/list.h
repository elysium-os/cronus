#pragma once

#include <stddef.h>
#include <stdint.h>

#define LIST_INIT ((list_t) { .count = 0, .head = nullptr, .tail = nullptr })

/**
 * @brief Get the structure/container that the element is embedded in.
 * @param ELEMENT embedded element
 * @param TYPE type of container
 * @param MEMBER name of the list embedded within the container
 * @returns pointer to container
 */
#define LIST_CONTAINER_GET(ELEMENT, TYPE, MEMBER) ((TYPE *) ((uintptr_t) (ELEMENT) - __builtin_offsetof(TYPE, MEMBER)))

/**
 * @brief Iterate over a list.
 * @param LIST `list_t` to iterate over
 * @param NODE `list_node_t *` iterator name
 */
#define LIST_ITERATE(LIST, NODE) for(list_node_t * (NODE) = (LIST)->head; (NODE) != nullptr; (NODE) = (NODE)->next)

typedef struct list_node list_node_t;
typedef struct list list_t;

struct list {
    size_t count;
    list_node_t *head, *tail;
};

struct list_node {
    list_node_t *next, *prev;
};

void list_push_front(list_t *list, list_node_t *node);
void list_push_back(list_t *list, list_node_t *node);
void list_push(list_t *list, list_node_t *node);

list_node_t *list_pop_front(list_t *list);
list_node_t *list_pop_back(list_t *list);
list_node_t *list_pop(list_t *list);

void list_node_append(list_t *list, list_node_t *pos, list_node_t *node);
void list_node_prepend(list_t *list, list_node_t *pos, list_node_t *node);
void list_node_delete(list_t *list, list_node_t *node);
