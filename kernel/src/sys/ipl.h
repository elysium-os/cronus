#pragma once

typedef enum {
    IPL_PREEMPT,
    IPL_NORMAL,
    IPL_IPC,
    IPL_CRITICAL
} ipl_t;

/**
 * @brief Swap IPL level.
 * @param ipl new ipl
 * @returns previous ipl
 */
ipl_t ipl(ipl_t ipl);