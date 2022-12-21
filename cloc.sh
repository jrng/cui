#! /bin/sh

if ! [ -x "$(command -v cloc)" ]; then
    echo "'cloc' was not found. Please install it."
    exit 1
fi

REL_PATH=$(dirname "$0")

cloc --exclude-dir=build --exclude-list-file="${REL_PATH}/.clocignore" $@ "${REL_PATH}"
