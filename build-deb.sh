#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Configuration
PACKAGE_NAME="msuper-gui"
VERSION="2.0.0"
ARCH="amd64"
BUILD_DIR="debian"
PACKAGE_FILE="${PACKAGE_NAME}_${VERSION}_${ARCH}.deb"

echo "=========================================="
echo "Building ${PACKAGE_NAME} DEB package"
echo "=========================================="

# Step 1: Build the binary
echo ""
echo "[1/6] Building msuper-gui..."
make clean
make

# Step 2: Prepare build directory
echo ""
echo "[2/6] Preparing build directory..."
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR/usr/lib/${PACKAGE_NAME}"
mkdir -p "$BUILD_DIR/usr/share/applications"
mkdir -p "$BUILD_DIR/usr/share/icons/hicolor/scalable/apps"
mkdir -p "$BUILD_DIR/DEBIAN"

# Step 3: Copy files
echo ""
echo "[3/6] Copying files..."

# Copy binaries
cp msuper-gui "$BUILD_DIR/usr/lib/${PACKAGE_NAME}/"
cp lpunpack "$BUILD_DIR/usr/lib/${PACKAGE_NAME}/"
cp simg2img "$BUILD_DIR/usr/lib/${PACKAGE_NAME}/"

# Set executable permissions
chmod 755 "$BUILD_DIR/usr/lib/${PACKAGE_NAME}/msuper-gui"
chmod 755 "$BUILD_DIR/usr/lib/${PACKAGE_NAME}/lpunpack"
chmod 755 "$BUILD_DIR/usr/lib/${PACKAGE_NAME}/simg2img"

# Copy desktop file
cp msuper-gui.desktop "$BUILD_DIR/usr/share/applications/"
chmod 644 "$BUILD_DIR/usr/share/applications/msuper-gui.desktop"

# Copy icon
cp msuper-gui.svg "$BUILD_DIR/usr/share/icons/hicolor/scalable/apps/"
chmod 644 "$BUILD_DIR/usr/share/icons/hicolor/scalable/apps/msuper-gui.svg"

# Step 4: Create DEBIAN control file
echo ""
echo "[4/6] Creating DEBIAN control file..."
cat > "$BUILD_DIR/DEBIAN/control" <<EOF
Package: ${PACKAGE_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${ARCH}
Maintainer: MSuper GUI Team
Depends: libgtk-3-0, libc6, libstdc++6, libgcc1, pkexec
Description: Android Super Image Tool
 A GUI tool for processing and mounting Android Super image files.
 Features:
  - Convert sparse Android images
  - Unpack partition images
  - Mount partitions for browsing
EOF

chmod 755 "$BUILD_DIR/DEBIAN"

# Step 5: Build the package
echo ""
echo "[5/6] Building DEB package..."
fakeroot dpkg-deb --build "$BUILD_DIR" "$PACKAGE_FILE"

# Step 6: Clean up
echo ""
echo "[6/6] Cleaning up..."
rm -rf "$BUILD_DIR"

echo ""
echo "=========================================="
echo "Success! DEB package built:"
echo "  ${PACKAGE_FILE}"
echo ""
echo "To install:"
echo "  sudo dpkg -i ${PACKAGE_FILE}"
echo ""
echo "To uninstall:"
echo "  sudo dpkg -r ${PACKAGE_NAME}"
echo "=========================================="
