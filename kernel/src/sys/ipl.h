#pragma once

typedef enum ipl {
    IPL_PREEMPT,
    IPL_NORMAL,
    IPL_CRITICAL
} ipl_t;

/**
 * @brief Raise IPL.
 * @note `ipl_to` must be equal or higher than the current IPL.
 */
ipl_t ipl_raise(ipl_t ipl_to);

/**
 * @brief Lower IPL.
 * @note `ipl_to` must be equal or lower than the current IPL.
 */
void ipl_lower(ipl_t ipl_to);
