#pragma once

/**
 * @brief Get the container of a child struct.
 * @param PTR pointer to the child struct
 * @param TYPE type of the container
 * @param MEMBER name of the child member in the container
 */
#define CONTAINER_OF(PTR, TYPE, MEMBER)                                \
    ({                                                                 \
        const typeof(((TYPE *) 0)->MEMBER) *__mptr = (PTR);            \
        (TYPE *) ((char *) __mptr - __builtin_offsetof(TYPE, MEMBER)); \
    })