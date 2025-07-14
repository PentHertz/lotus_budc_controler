# ================================
# Linux Build Stage
# ================================
FROM ubuntu:24.04 AS linux-builder

# Install dependencies for Ubuntu 24.04
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    libserialport-dev \
    libglfw3-dev \
    libgl1-mesa-dev \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libwayland-dev \
    libxkbcommon-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Create build directory and configure for static linking
RUN mkdir -p build && cd build && \
    cmake .. \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
        -DBUILD_SHARED_LIBS=OFF \
        -DGLFW_BUILD_SHARED_LIBS=OFF

# Build the applications
RUN cd build && ninja budc_gui budc_cli

# Create output directory and copy binaries
RUN mkdir -p /output/linux-x64 && \
    cp build/budc_cli build/budc_gui /output/linux-x64/ && \
    strip /output/linux-x64/*

# ================================
# CLI-only Linux Build (fallback)
# ================================
FROM ubuntu:24.04 AS linux-cli-builder

# Install minimal dependencies for CLI only
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libserialport-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Build CLI only
RUN mkdir -p build-cli && cd build-cli && \
    cmake .. \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_CLI_ONLY=ON \
        -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++"

RUN cd build-cli && ninja budc_cli

# Create output
RUN mkdir -p /output/linux-x64 && \
    cp build-cli/budc_cli /output/linux-x64/ && \
    strip /output/linux-x64/*

# ================================
# Windows Build Stage
# ================================
FROM ubuntu:24.04 AS windows-builder

# Install MinGW cross-compiler
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    mingw-w64 \
    pkg-config \
    git \
    wget \
    unzip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /deps

# Download and build libserialport for Windows
RUN wget https://sigrok.org/download/source/libserialport/libserialport-0.1.1.tar.gz && \
    tar xzf libserialport-0.1.1.tar.gz && \
    cd libserialport-0.1.1 && \
    ./configure --host=x86_64-w64-mingw32 --prefix=/usr/x86_64-w64-mingw32 --enable-static --disable-shared && \
    make && make install

WORKDIR /src
COPY . .

# Create MinGW toolchain file (No changes needed here)
RUN mkdir -p cmake && \
    echo '# MinGW-w64 toolchain file for cross-compiling to Windows' > cmake/mingw-w64-x86_64.cmake && \
    echo 'set(CMAKE_SYSTEM_NAME Windows)' >> cmake/mingw-w64-x86_64.cmake && \
    echo 'set(CMAKE_SYSTEM_PROCESSOR x86_64)' >> cmake/mingw-w64-x86_64.cmake && \
    echo 'set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)' >> cmake/mingw-w64-x86_64.cmake && \
    echo 'set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)' >> cmake/mingw-w64-x86_64.cmake && \
    echo 'set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)' >> cmake/mingw-w64-x86_64.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)' >> cmake/mingw-w64-x86_64.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)' >> cmake/mingw-w64-x86_64.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)' >> cmake/mingw-w64-x86_64.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)' >> cmake/mingw-w64-x86_64.cmake && \
    echo 'set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)' >> cmake/mingw-w64-x86_64.cmake && \
    echo 'set(CMAKE_EXECUTABLE_SUFFIX ".exe")' >> cmake/mingw-w64-x86_64.cmake

# Build for Windows - try GUI first, fallback to CLI-only
RUN mkdir -p build-windows && cd build-windows && \
    PKG_CONFIG_PATH=/usr/x86_64-w64-mingw32/lib/pkgconfig \
    PKG_CONFIG_LIBDIR=/usr/x86_64-w64-mingw32/lib/pkgconfig \
    cmake .. \
        -GNinja \
        -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64-x86_64.cmake \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_EXE_LINKER_FLAGS="-static" \
    || (echo "GUI build failed, trying CLI-only..." && \
        rm -rf * && \
        PKG_CONFIG_PATH=/usr/x86_64-w64-mingw32/lib/pkgconfig \
        PKG_CONFIG_LIBDIR=/usr/x86_64-w64-mingw32/lib/pkgconfig \
        cmake .. \
            -GNinja \
            -DCMAKE_TOOLCHAIN_FILE=../cmake/mingw-w64-x86_64.cmake \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_CLI_ONLY=ON \
            -DCMAKE_EXE_LINKER_FLAGS="-static")

# Try to build both CLI and GUI, fall back to CLI only if GUI fails
RUN cd build-windows && \
    (ninja budc_cli budc_gui || ninja budc_cli)

# Create output directory, copy binaries, and strip them. No DLL copy needed.
RUN mkdir -p /output/windows-x64 && \
    cp build-windows/budc_cli.exe /output/windows-x64/ && \
    (cp build-windows/budc_gui.exe /output/windows-x64/ 2>/dev/null || echo "GUI not available for Windows") && \
    x86_64-w64-mingw32-strip /output/windows-x64/*.exe

# ================================
# macOS Placeholder  
# ================================
FROM alpine:latest AS macos-builder
RUN mkdir -p /output/macos-x64
RUN echo "macOS cross-compilation requires macOS SDK" > /output/macos-x64/README.txt

# ================================
# Final Output Stage
# ================================
FROM scratch AS output

# Copy ONLY the output directories from each build stage
COPY --from=linux-builder /output/linux-x64 /linux-x64
COPY --from=windows-builder /output/windows-x64 /windows-x64  
COPY --from=macos-builder /output/macos-x64 /macos-x64