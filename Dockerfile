# ==========================================
# STAGE 1: COLMAP BUILDER
# ==========================================
FROM nvidia/cuda:11.8.0-devel-ubuntu22.04 AS colmap_builder
ARG PARALLEL_JOBS=4
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

RUN apt-get update && apt-get install -y \
    cmake build-essential git curl unzip ninja-build \
    libboost-program-options-dev libboost-filesystem-dev libboost-graph-dev \
    libboost-system-dev libboost-test-dev libboost-serialization-dev \
    libeigen3-dev libfreeimage-dev libgoogle-glog-dev libgflags-dev \
    libglew-dev libmetis-dev libceres-dev \
    libflann-dev liblz4-dev libsqlite3-dev libcgal-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
# COLMAP 3.9.1
RUN git clone --branch 3.9.1 --depth 1 https://github.com/colmap/colmap.git . && \
    mkdir build && cd build && \
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DGUI_ENABLED=OFF \
    -DCUDA_ENABLED=ON -DCMAKE_CUDA_ARCHITECTURES="60;61;70;75;80;86" \
    && ninja -j ${PARALLEL_JOBS} && ninja install

# ==========================================
# STAGE 2: OPEN3D BUILDER (Source)
# ==========================================
FROM nvidia/cuda:11.8.0-devel-ubuntu22.04 AS open3d_builder
ARG PARALLEL_JOBS=4
ENV DEBIAN_FRONTEND=noninteractive

# Zależności do budowania Open3D
RUN apt-get update && apt-get install -y \
    build-essential cmake git python3-dev \
    libgl1-mesa-dev libglu1-mesa-dev \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /open3d
# Pobieramy wersję 0.17.0 (Stabilna, nowoczesna, działa z C++17)
RUN git clone --branch v0.17.0 --depth 1 https://github.com/isl-org/Open3D.git . 

WORKDIR /open3d/build
# Konfiguracja CMake - WYŁĄCZAMY WSZYSTKO CO ZBĘDNE dla szybkości
RUN cmake .. \
    -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_GUI=OFF \
    -DBUILD_PYTHON_MODULE=OFF \
    -DBUILD_WEBRTC=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_UNIT_TESTS=OFF \
    -DGLIBCXX_USE_CXX11_ABI=ON \
    && make -j${PARALLEL_JOBS} && make install

# ==========================================
# STAGE 3: BASE RUNTIME
# ==========================================
FROM nvidia/cuda:11.8.0-runtime-ubuntu22.04 AS base
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git python3-pip wget \
    libgl1-mesa-dev libvulkan1 libxkbcommon-x11-0 \
    libxkbcommon-dev libvulkan-dev libglu1-mesa-dev \
    libxcb1 libxcb-glx0 libxcb-keysyms1 libxcb-image0 libxcb-shm0 \
    libxcb-icccm4 libxcb-sync1 libxcb-xfixes0 libxcb-shape0 libxcb-randr0 \
    libxcb-render-util0 libxcb-util1 libxcb-xinerama0 libxcb-xinput0 \
    libx11-xcb1 libxrender1 libxi6 libfontconfig1 \
    libxcb-cursor0 libdbus-1-3 libpulse0 \
    # Colmap deps
    libboost-program-options1.74.0 libboost-filesystem1.74.0 libboost-graph1.74.0 \
    libboost-system1.74.0 libboost-serialization1.74.0 \
    libgoogle-glog0v5 libgflags2.2 libglew2.2 libmetis5 \
    libfreeimage3 libflann1.9 liblz4-1 libsqlite3-0 libcgal-dev libceres-dev \
    # Open3D deps (runtime)
    libgomp1 \
    # App
    libassimp5 libopencv-dev assimp-utils p7zip-full ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --upgrade aqtinstall
ENV QT_VERSION=6.7.2
ENV QT_ARCH=gcc_64
RUN aqt install-qt linux desktop ${QT_VERSION} linux_${QT_ARCH} -O /opt/qt --external 7z \
    -m qt5compat qtimageformats qt3d qtquick3d qtshadertools qtquicktimeline
RUN chmod -R 755 /opt/qt

# ONNX Runtime
WORKDIR /tmp
RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz \
    && tar -xf onnxruntime-linux-x64-1.17.1.tgz \
    && cp -r onnxruntime-linux-x64-1.17.1/include/* /usr/local/include/ \
    && cp -r onnxruntime-linux-x64-1.17.1/lib/* /usr/local/lib/ \
    && rm -rf onnxruntime* && ldconfig

# Kopiowanie COLMAP
COPY --from=colmap_builder /usr/local/bin/colmap /usr/local/bin/colmap
COPY --from=colmap_builder /usr/local/share/colmap /usr/local/share/colmap

# Kopiowanie OPEN3D (Ze zbudowanego źródła)
COPY --from=open3d_builder /usr/local/include/open3d /usr/local/include/open3d
COPY --from=open3d_builder /usr/local/lib/libOpen3D.so /usr/local/lib/
COPY --from=open3d_builder /usr/local/lib/cmake/Open3D /usr/local/lib/cmake/Open3D
RUN ldconfig

# Env Qt
ENV PATH="/opt/qt/${QT_VERSION}/${QT_ARCH}/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/qt/${QT_VERSION}/${QT_ARCH}/lib:${LD_LIBRARY_PATH}"
ENV QT_PLUGIN_PATH="/opt/qt/${QT_VERSION}/${QT_ARCH}/plugins"
ENV QML_IMPORT_PATH="/opt/qt/${QT_VERSION}/${QT_ARCH}/qml"
ENV CMAKE_PREFIX_PATH="/opt/qt/${QT_VERSION}/${QT_ARCH}"
RUN echo "/opt/qt/${QT_VERSION}/${QT_ARCH}/lib" > /etc/ld.so.conf.d/qt6.conf && ldconfig

ARG USER_ID
ARG GROUP_ID
RUN groupadd -g ${GROUP_ID:-1000} devgroup && \
    useradd -l -u ${USER_ID:-1000} -g devgroup -m devuser

WORKDIR /app
USER devuser

# ==========================================
# STAGE 4: APP BUILDER
# ==========================================
FROM base AS builder
ARG PARALLEL_JOBS=4
COPY --chown=devuser:devgroup src/ /app/src/
WORKDIR /app/src/
RUN rm -rf build && mkdir build && cd build && \
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release && ninja -j ${PARALLEL_JOBS}

# ==========================================
# STAGE 5: FINAL IMAGE
# ==========================================
FROM base AS final
ENV XDG_RUNTIME_DIR=/tmp/runtime-app
RUN mkdir -p /tmp/runtime-app && chmod 777 /tmp/runtime-app
RUN useradd -ms /bin/bash appuser
USER appuser
WORKDIR /home/appuser
COPY --from=builder /app/src/build/ImageTo3D .
COPY src/models ./models
COPY --from=builder /app/src/build/shaders ./shaders 
ENV QT_QPA_PLATFORM=xcb
ENV QT_XCB_GL_INTEGRATION=xcb_glx
CMD ["./ImageTo3D"]
