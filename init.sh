#!/bin/bash

echo "Cloning Submodules..."

git submodule update --init --recursive
cd PheonixCore || exit

if ! git diff --quiet || ! git diff --cached --quiet; then
    echo "Local changes detected in PheonixCore. Stashing..."
    git stash push -u -m "auto-stash before submodule update"
    STASHED=1
else
    echo "No local changes in PheonixCore."
    STASHED=0
fi

cd ..

git submodule update --remote

cd PheonixCore || exit

if [ "$STASHED" -eq 1 ]; then
    echo "Dropping auto-created stash..."
    git stash pop >/dev/null 2>&1
fi

cd ..