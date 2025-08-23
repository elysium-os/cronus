#pragma once

#include "../../../include/arch/cpu.h"

#define CPU_LOCAL [[gnu::section(".cpu_local")]] __seg_gs
