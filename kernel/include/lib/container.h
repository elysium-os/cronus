#pragma once

/// Get the container of a child struct.
/// @param PTR Pointer to the child struct
/// @param TYPE Type of the container
/// @param MEMBER Name of the child member in the container
#define CONTAINER_OF(PTR, TYPE, MEMBER)                                                                                                \
    ({                                                                                                                                 \
        static_assert(__builtin_types_compatible_p(typeof(((TYPE *) 0)->MEMBER), typeof(*PTR)), "member type does not match pointer"); \
        (TYPE *) (((uintptr_t) (PTR)) - __builtin_offsetof(TYPE, MEMBER));                                                             \
    })
