#pragma once

#include "sched/process.h"
#include "sched/thread.h"

/**
 * @brief Create reaper thread.
 */
thread_t *reaper_create();

/*
 * @brief Queue a thread onto the reaper.
 */
void reaper_queue_thread(thread_t *thread);

/*
 * @brief Queue a process onto the reaper.
 */
void reaper_queue_process(process_t *process);
