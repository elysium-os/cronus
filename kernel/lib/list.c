#include "list.h"

void list_append(list_element_t *position, list_element_t *element) {
    element->prev = position;
    element->next = position->next;
    if(position->next) position->next->prev = element;
    position->next = element;
}

void list_prepend(list_element_t *position, list_element_t *element) {
    element->next = position;
    element->prev = position->prev;
    if(position->prev) position->prev->next = element;
    position->prev = element;
}

void list_delete(list_element_t *element) {
    if(element->prev) element->prev->next = element->next;
    if(element->next) element->next->prev = element->prev;
}

bool list_is_empty(list_t *list) {
    return !list->next || list == list->next;
}
