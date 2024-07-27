#!/bin/bash

if ! command bear &> /dev/null; then
	echo "Please install bear. https://github.com/rizsotto/Bear"
	exit 1
fi

bear -- make host

