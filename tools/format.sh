#!/usr/bin/env bash
clang-format -i $(find ./kernel -name "*.c" -or -name "*.h")