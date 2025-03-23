#!/usr/bin/env bash
addr2line -fai -e .chariot-cache/target/kernel/install/usr/bin/kernel.elf $1
