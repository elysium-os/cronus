#include "list.h"

#include <stddef.h>

void list_append(list_element_t *position, list_element_t *element) {
    element->prev = position;
    element->next = position->next;
    if(position->next != nullptr) position->next->prev = element;
    position->next = element;
}

void list_prepend(list_element_t *position, list_element_t *element) {
    element->next = position;
    element->prev = position->prev;
    if(position->prev != nullptr) position->prev->next = element;
    position->prev = element;
}

void list_delete(list_element_t *element) {
    if(element->prev != nullptr) element->prev->next = element->next;
    if(element->next != nullptr) element->next->prev = element->prev;
}

bool list_is_empty(list_t *list) {
    return list->next == nullptr || list->next == list;
}
