#!/bin/bash
set -e

#Initial project setup; run before executing the path tracer
#Checks and installs missing dependencies; set-up OIDN denoiser
#GLFW3, GLEW, GLM, pkg-config, OIDN

OIDN_VERSION="2.5.0"
OIDN_URL="https://github.com/OpenImageDenoise/oidn/releases/download/v${OIDN_VERSION}/oidn-${OIDN_VERSION}.x86_64.linux.tar.gz"
OIDN_DIR="/opt/oidn"

echo "Path Tracer - Libraries and Dependencies Setup"

#check required directories
echo "Checking project directories"
for dir in build models hdri; do
    if [ -d "$dir" ]; then
        echo " $dir/ already exists, skipped"
    else
        mkdir -p "$dir"
        echo " $dir/ created"
    fi
done

#check apt dependencies before install
echo "Checking dependencies"
MISSING_PACKAGES=()

dpkg -s libglfw3-dev &>/dev/null || MISSING_PACKAGES+=(libglfw3-dev)
dpkg -s libglew-dev &>/dev/null || MISSING_PACKAGES+=(libglew-dev)
dpkg -s libglm-dev &>/dev/null || MISSING_PACKAGES+=(libglm-dev)
dpkg -s libassimp-dev &>/dev/null || MISSING_PACKAGES+=(libassimp-dev)
dpkg -s libpkgconf-dev &>/dev/null || MISSING_PACKAGES+=(pkg-config)

if [ ${#MISSING_PACKAGES[@]} -eq 0 ]; then
    echo "All apt dependencies already installed, skipped"
else
    echo "Installing: ${MISSING_PACKAGES[*]}"
    sudo apt install -y "${MISSING_PACKAGES[@]}"
fi

#check if OIDN is already installed on the right version
echo "Checking OIDN"
if [ -f "$OIDN_DIR/lib/libOpenImageDenoise.so.${OIDN_VERSION}" ]; then
    echo "OIDN ${OIDN_VERSION} already installed at $OIDN_DIR, skipped"
else
    if [ -d "$OIDN_DIR" ]; then
        echo -e "Different OIDN version found. Removing everything at $OIDN_DIR"
        sudo rm -rf "$OIDN_DIR"
    fi
    echo "Downloading OIDN ${OIDN_VERSION}"
    wget -q --show-progress "$OIDN_URL" -O /tmp/oidn.tar.gz
    tar -xzf /tmp/oidn.tar.gz -C /tmp
    sudo mv /tmp/oidn-${OIDN_VERSION}.x86_64.linux "$OIDN_DIR"
    echo "/opt/oidn/lib" | sudo tee /etc/ld.so.conf.d/oidn.conf > /dev/null
    sudo ldconfig
    rm /tmp/oidn.tar.gz
    echo "OIDN ${OIDN_VERSION} installed"
fi

#compile
echo "Building at ./build"
cd build
cmake .. -DOIDN_ROOT="$OIDN_DIR"
make -j$(nproc)

echo -e "\nDONE, run with ./pathtracer"