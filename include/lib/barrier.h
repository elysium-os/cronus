#pragma once

#define BARRIER __atomic_signal_fence(__ATOMIC_ACQ_REL)
