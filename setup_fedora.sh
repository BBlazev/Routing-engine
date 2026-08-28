#!/bin/bash
set -e

echo "=== Vulkan Streaming Project — Fedora Setup ==="
echo ""

# -------------------------------------------------------
# 1. Compiler — need GCC 13+ or Clang 17+ for C++23
# -------------------------------------------------------
echo "[1/5] Checking compiler..."
GCC_VER=$(gcc -dumpversion 2>/dev/null || echo "0")
if [ "${GCC_VER%%.*}" -lt 13 ]; then
    echo "  GCC $GCC_VER is too old for C++23. Installing GCC 13+..."
    sudo dnf install -y gcc gcc-c++
else
    echo "  GCC $GCC_VER — good."
fi

# -------------------------------------------------------
# 2. Build tools
# -------------------------------------------------------
echo "[2/5] Installing build tools..."
sudo dnf install -y \
    cmake \
    ninja-build \
    git \
    pkg-config

# -------------------------------------------------------
# 3. Vulkan SDK — headers, loader, validation layers,
#    shader compiler, and debug tools
# -------------------------------------------------------
echo "[3/5] Installing Vulkan SDK..."
sudo dnf install -y \
    vulkan-headers \
    vulkan-loader-devel \
    vulkan-validation-layers \
    vulkan-tools \
    glslang \
    glslc \
    spirv-tools

# -------------------------------------------------------
# 4. Windowing + math libraries
# -------------------------------------------------------
echo "[4/5] Installing GLFW and GLM..."
sudo dnf install -y \
    glfw-devel \
    glm-devel

# -------------------------------------------------------
# 5. Optional but helpful
# -------------------------------------------------------
echo "[5/5] Installing extras..."
sudo dnf install -y \
    renderdoc \
    gdb \
    valgrind \
    clang-tools-extra

# -------------------------------------------------------
# Verify everything works
# -------------------------------------------------------
echo ""
echo "=== Verification ==="
echo "GCC:        $(gcc --version | head -1)"
echo "CMake:      $(cmake --version | head -1)"
echo "Ninja:      $(ninja --version)"
echo "glslc:      $(glslc --version 2>&1 | head -1)"
echo "Vulkan ICD: $(vulkaninfo --summary 2>/dev/null | grep 'deviceName' | head -1 || echo 'run vulkaninfo to check')"
echo ""
echo "=== Done. Run: cmake --preset=debug && cmake --build build/debug ==="
