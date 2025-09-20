#pragma once

#include "../../../include/arch/cpu.h"
#include "x86_64/cpu/cpu.h"

#define CPU_CURRENT (*(__seg_gs x86_64_cpu_t *) nullptr).common

#define CPU_CURRENT_READ(FIELD) __atomic_load_n(&CPU_CURRENT.FIELD, __ATOMIC_RELAXED)
#define CPU_CURRENT_WRITE(FIELD, VALUE) __atomic_store_n(&CPU_CURRENT.FIELD, (VALUE), __ATOMIC_RELAXED)

#define CPU_CURRENT_EXCHANGE(FIELD, VALUE) __atomic_exchange_n(&CPU_CURRENT.FIELD, (VALUE), __ATOMIC_RELAXED)

#define CPU_CURRENT_INC(FIELD) __atomic_fetch_add(&CPU_CURRENT.FIELD, 1, __ATOMIC_RELAXED)
#define CPU_CURRENT_DEC(FIELD) __atomic_fetch_sub(&CPU_CURRENT.FIELD, 1, __ATOMIC_RELAXED)
