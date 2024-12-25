#pragma once

#include <stdint.h>

#define LIST_INIT ((list_t) { .next = NULL, .prev = NULL })
#define LIST_INIT_CIRCULAR(NAME) ((list_t) { .next = &(NAME), .prev = &(NAME) })

typedef struct list_element list_element_t;
typedef struct list_element list_t;

struct list_element {
    list_element_t *next;
    list_element_t *prev;
};

/**
 * @brief Insert a new element behind an existing element in a list.
 * @param position element to insert behind
 */
void list_append(list_element_t *position, list_element_t *element);

/**
 * @brief Insert a new element before an existing element in a list.
 * @param position element to insert before
 */
void list_prepend(list_element_t *position, list_element_t *element);

/**
 * @brief Delete a element from a list.
 */
void list_delete(list_element_t *element);

/**
 * @brief Test if a list is empty.
 * @param list list head
 */
bool list_is_empty(list_t *list);

/**
 * @brief Get the structure/container that the element is embedded in.
 * @param ELEMENT embedded element
 * @param TYPE type of container
 * @param MEMBER name of the list embedded within the container
 * @returns pointer to container
 */
#define LIST_CONTAINER_GET(ELEMENT, TYPE, MEMBER) ((TYPE *) ((uintptr_t) (ELEMENT) - __builtin_offsetof(TYPE, MEMBER)))

/**
 * @brief Get the next element in a list.
 */
#define LIST_NEXT(ELEMENT) ((ELEMENT)->next)

/**
 * @brief Get the previous element in a list.
 */
#define LIST_PREVIOUS(ELEMENT) ((ELEMENT)->prev)

/**
 * @brief Iterate over a list.
 * @param LIST `list_t` to iterate over
 * @param ELEMENT `list_element_t *` iterator name
 */
#define LIST_FOREACH(LIST, ELEMENT) for(list_element_t *(ELEMENT) = LIST_NEXT(LIST); (ELEMENT) != NULL && (ELEMENT) != (LIST); (ELEMENT) = LIST_NEXT(ELEMENT))