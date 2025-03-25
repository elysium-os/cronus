#include "list.h"

#include <stddef.h>

void list_append(list_element_t *position, list_element_t *element) {
    element->prev = position;
    element->next = position->next;
    if(position->next != NULL) position->next->prev = element;
    position->next = element;
}

void list_prepend(list_element_t *position, list_element_t *element) {
    element->next = position;
    element->prev = position->prev;
    if(position->prev != NULL) position->prev->next = element;
    position->prev = element;
}

void list_delete(list_element_t *element) {
    if(element->prev != NULL) element->prev->next = element->next;
    if(element->next != NULL) element->next->prev = element->prev;
}

bool list_is_empty(list_t *list) {
    return list->next == NULL || list->next == list;
}
