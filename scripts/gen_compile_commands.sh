#!/bin/bash

KERN_DIR="$1"

if [[ ! -f scripts/gen_compile_commands.py ]]; then
	curl -L https://raw.githubusercontent.com/torvalds/linux/refs/heads/master/scripts/clang-tools/gen_compile_commands.py -o scripts/gen_compile_commands.py
	chmod +x scripts/gen_compile_commands.py
fi

./scripts/gen_compile_commands.py

SCRIPT="s/\.\//$(echo $KERN_DIR | sed -E 's/\//\\\//g')/g"
echo $SCRIPT

sed -iE $SCRIPT compile_commands.json
