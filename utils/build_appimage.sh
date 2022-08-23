#!/bin/bash
set -e

# Builds Endless Sky and packages it as AppImage
# Control the output filename with the OUTPUT environment variable
# You may have to set the ARCH environment variable to e.g. x86_64.

# Install
DESTDIR=AppDir cmake --install build/ci --prefix /usr

# Inside an AppImage, the executable is a link called "AppRun" at the root of AppDir/.
# Keeping the data files next to the executable is perfectly valid, so we just move them to AppDir/ to avoid errors.
mv AppDir/usr/share/games/editor/* AppDir/

# Now build the actual AppImage
curl -sSL https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -o linuxdeploy && chmod +x linuxdeploy
./linuxdeploy --appdir AppDir -e build/ci/editor -d resources/editor.desktop --output appimage

# Clean up
rm -rf AppDir linuxdeploy
