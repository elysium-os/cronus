#pragma once

/**
 * @brief Get the container of a child struct.
 * @param PTR pointer to the child struct
 * @param TYPE type of the container
 * @param MEMBER name of the child member in the container
 */
#define CONTAINER_OF(PTR, TYPE, MEMBER)                                                                                                \
    ({                                                                                                                                 \
        static_assert(__builtin_types_compatible_p(typeof(((TYPE *) 0)->MEMBER), typeof(*PTR)), "member type does not match pointer"); \
        (TYPE *) (((uintptr_t) (PTR)) - __builtin_offsetof(TYPE, MEMBER));                                                             \
    })
