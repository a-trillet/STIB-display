#!/bin/bash

SRC_DIR="bus_display_led"
DEST_DIR="build/bus_display_led"
MPY_CROSS="./mpy-cross"  # Adjust path if needed

if [ ! -x "$MPY_CROSS" ]; then
    echo "Error: mpy-cross not found or not executable."
    exit 1
fi

rm -rf "$DEST_DIR"
mkdir -p "$DEST_DIR"

# Extract app_version from config.py
VERSION=$(grep -oP "app_version\s*=\s*['\"]\K[^'\"]+" "$SRC_DIR/config.py")
if [ -z "$VERSION" ]; then
    echo "Error: Could not find app_version in $SRC_DIR/config.py"
    exit 1
fi

echo "Building version: $VERSION"

# Compile .py files to .mpy
find "$SRC_DIR" -type f -name "*.py" | while read -r src_file; do
    rel_path="${src_file#$SRC_DIR/}"
    dest_path="$DEST_DIR/${rel_path%.py}.mpy"
    dest_dir="$(dirname "$dest_path")"
    mkdir -p "$dest_dir"
    echo "Compiling: $src_file -> $dest_path"
    "$MPY_CROSS" -o "$dest_path" "$src_file"
done

# Copy non-.py files (like .css, .svg, etc.) as-is
find "$SRC_DIR" -type f ! -name "*.py" ! -name "*.pyc" | while read -r src_file; do
    rel_path="${src_file#$SRC_DIR/}"
    dest_path="$DEST_DIR/$rel_path"
    dest_dir="$(dirname "$dest_path")"
    mkdir -p "$dest_dir"
    echo "Copying: $src_file -> $dest_path"
    cp "$src_file" "$dest_path"
done

# Create tarball named with the version
TAR_NAME="build/bus_display_led_${VERSION}.tar"
echo "Creating tar archive $TAR_NAME ..."
tar -cf "$TAR_NAME" -C build bus_display_led

echo "✅ Build and tar creation complete. Archive: $TAR_NAME"
