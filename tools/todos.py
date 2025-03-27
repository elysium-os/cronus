#!/usr/bin/env nix-shell
#!nix-shell -i python3 -p python3

import os
import re

root_dir = "./kernel"

def find_tags(tag, color):
    tags = []
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file.endswith(".c") or file.endswith(".h"):
                fullpath = os.path.join(root, file)
                with open(fullpath, "r") as f:
                    for i, line in enumerate(f):
                        match = re.search("(\\/\\/|\\*|@|;) *" + tag + ":? *(.*)", line)
                        if match != None:
                            tags.append({ "tag": tag, "color": color, "path": fullpath, "line": i, "value": match.group(2) })
                            break
    return tags

tags = []
tags += find_tags("TODO", 33)
tags += find_tags("todo", 33)
tags += find_tags("OPTIMIZE", 36)
tags += find_tags("UNIMPLEMENTED", 35)
tags += find_tags("CRITICAL", 31)
tags += find_tags("FIX", 91)
tags += find_tags("TEMPORARY", 32)
tags += find_tags("NOTE", 37)

for tag in tags:
    print(f"\033[{tag["color"]}m[{tag["tag"]}] {tag["path"]}:{tag["line"]}\033[0m {tag["value"]}")
