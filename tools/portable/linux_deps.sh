#!/usr/bin/env bash
# System packages + toolchain for the PortableLinux preset (Ubuntu 24.04 x86_64; GitHub ubuntu-24.04 runner or a container). Idempotent. Needs root or sudo.
#
# Compiler: clang/libc++ 23 from apt.llvm.org (the game needs std::views::enumerate / ranges::shift_right: libc++ >= 23 has them, libstdc++ 13 (noble) does not, see
# source/portable/stdcompat.h). Python tools: cmake >= 4.2 (the repo requires it, noble ships 3.28), ninja, conan 2 (pip, --break-system-packages in a throw-away runner).
# usage: tools/portable/linux_deps.sh        env: LLVM_VER (default 23)
set -euo pipefail
SUDO=""; [ "$(id -u)" = 0 ] || SUDO="sudo"
LLVM_VER="${LLVM_VER:-23}"
export DEBIAN_FRONTEND=noninteractive
$SUDO apt-get update -y
$SUDO apt-get install -y --no-install-recommends build-essential ca-certificates curl gnupg lsb-release python3 python3-pip pkg-config git make xz-utils
if [ ! -f /etc/apt/sources.list.d/llvm.list ]; then
    curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key | $SUDO gpg --dearmor -o /usr/share/keyrings/llvm.gpg
    echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] https://apt.llvm.org/$(lsb_release -cs)/ llvm-toolchain-$(lsb_release -cs)-${LLVM_VER} main" | $SUDO tee /etc/apt/sources.list.d/llvm.list >/dev/null
    $SUDO apt-get update -y
fi
$SUDO apt-get install -y --no-install-recommends "clang-${LLVM_VER}" "lld-${LLVM_VER}" "libc++-${LLVM_VER}-dev" "libc++abi-${LLVM_VER}-dev" "libclang-rt-${LLVM_VER}-dev" "llvm-${LLVM_VER}" ccache \
    libgl-dev libglu1-mesa-dev libopenal-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxkbcommon-dev libwayland-dev \
    libdrm-dev libgbm-dev libasound2-dev libpulse-dev libudev-dev libdbus-1-dev libegl-dev libibus-1.0-dev
pip3 install --break-system-packages --upgrade "cmake>=4.2" ninja conan
echo "tools: $(clang++-${LLVM_VER} --version | head -1); cmake $(cmake --version | head -1); conan $(conan --version)"
