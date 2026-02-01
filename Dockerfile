# ==========================================
# STAGE 1: COLMAP BUILDER (CUDA Support)
# ==========================================
FROM nvidia/cuda:11.8.0-devel-ubuntu22.04 AS colmap_builder

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

# 1. Instalacja zależności COLMAP
# Fixes: libeigen3-dev, libflann-dev, liblz4-dev, libsqlite3-dev, libcgal-dev
RUN apt-get update && apt-get install -y \
    cmake build-essential git curl unzip ninja-build \
    libboost-program-options-dev libboost-filesystem-dev libboost-graph-dev \
    libboost-system-dev libboost-test-dev libboost-serialization-dev \
    libeigen3-dev libfreeimage-dev libgoogle-glog-dev libgflags-dev \
    libglew-dev libmetis-dev libceres-dev \
    libflann-dev liblz4-dev libsqlite3-dev libcgal-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# 2. Budowanie COLMAP 3.9.1
# Fixes: CMAKE_CUDA_ARCHITECTURES="60...86" (dla CMake 3.22)
RUN git clone --branch 3.9.1 --depth 1 https://github.com/colmap/colmap.git . && \
    mkdir build && cd build && \
    cmake .. -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DGUI_ENABLED=OFF \
    -DCUDA_ENABLED=ON \
    -DCMAKE_CUDA_ARCHITECTURES="60;61;70;75;80;86" \
    && ninja -j 4 && ninja install

# ==========================================
# STAGE 2: QT6 BASE (Environment Setup)
# ==========================================
FROM nvidia/cuda:11.8.0-devel-ubuntu22.04 AS base

ENV DEBIAN_FRONTEND=noninteractive

# 1. Instalacja zależności systemowych i Qt
# Fixes: p7zip-full, wget, opencv-dev, xkb/vulkan, xcb-cursor0
RUN apt-get update && apt-get install -y \
    build-essential cmake ninja-build git python3-pip \
    libgl1-mesa-dev libxkbcommon-x11-0 libpulse-dev libdbus-1-3 \
    libxcb1 libxcb-glx0 libxcb-keysyms1 libxcb-image0 libxcb-shm0 \
    libxcb-icccm4 libxcb-sync1 libxcb-xfixes0 libxcb-shape0 libxcb-randr0 \
    libxcb-render-util0 libxcb-util1 libxcb-xinerama0 libxcb-xinput0 \
    libx11-xcb-dev libxrender-dev libxi-dev \
    # Runtime & Build fixes:
    libxkbcommon-dev libvulkan-dev libopencv-dev libxcb-cursor0 \
    p7zip-full wget \
    # Colmap Runtime Deps (dla buildera):
    libboost-program-options-dev libboost-filesystem-dev libboost-graph-dev \
    libboost-system-dev libboost-test-dev libgoogle-glog-dev libgflags-dev \
    libfreeimage-dev libatlas-base-dev libsuitesparse-dev liblz4-dev libmetis-dev \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install aqtinstall

# 2. Instalacja Qt 6.6.0 via aqtinstall
# WAŻNE: Dodano 'qtquicktimeline' do listy modułów!
# To naprawia błąd: (libQt6QuickTimeline.so.6: cannot open shared object file)
ARG QT_VERSION=6.6.0
RUN aqt install-qt linux desktop ${QT_VERSION} gcc_64 -O /opt/qt --external 7z \
    -m qt5compat qtimageformats qt3d qtquick3d qtshadertools qtquicktimeline

RUN chmod -R 755 /opt/qt

# 3. Zmienne środowiskowe Qt
ENV PATH="/opt/qt/${QT_VERSION}/gcc_64/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/qt/${QT_VERSION}/gcc_64/lib:${LD_LIBRARY_PATH}"
ENV Qt6_DIR="/opt/qt/${QT_VERSION}/gcc_64/lib/cmake/Qt6"
ENV QT_PLUGIN_PATH="/opt/qt/${QT_VERSION}/gcc_64/plugins"
ENV QML_IMPORT_PATH="/opt/qt/${QT_VERSION}/gcc_64/qml"
ENV QML2_IMPORT_PATH="/opt/qt/${QT_VERSION}/gcc_64/qml"

# 4. Instalacja ONNX Runtime
WORKDIR /tmp
RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz \
    && tar -xf onnxruntime-linux-x64-1.17.1.tgz \
    && cp -r onnxruntime-linux-x64-1.17.1/include/* /usr/local/include/ \
    && cp -r onnxruntime-linux-x64-1.17.1/lib/* /usr/local/lib/ \
    && rm -rf onnxruntime* \
    && ldconfig

# Kopiowanie binarki COLMAP
COPY --from=colmap_builder /usr/local/bin/colmap /usr/local/bin/colmap

ARG USER_ID
ARG GROUP_ID
RUN groupadd -g ${GROUP_ID:-1000} devgroup && \
    useradd -l -u ${USER_ID:-1000} -g devgroup -m devuser

USER devuser
WORKDIR /app
CMD ["sleep", "infinity"]

# ==========================================
# STAGE 3: BUILDER (Project Compilation)
# ==========================================
FROM base AS builder
COPY --chown=devuser:devgroup src/ /app/src/
WORKDIR /app/src/
RUN rm -rf build && mkdir build && cd build && \
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release && ninja

# ==========================================
# STAGE 4: FINAL (Production Image)
# ==========================================
FROM nvidia/cuda:11.8.0-runtime-ubuntu22.04 AS final

ENV TZ=Europe/Warsaw
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ >/etc/timezone

# Instalacja bibliotek runtime
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libgl1-mesa-dev libxkbcommon-x11-0 libpulse0 libdbus-1-3 \
    libxcb1 libxcb-glx0 libxcb-keysyms1 libxcb-image0 libxcb-shm0 \
    libxcb-icccm4 libxcb-sync1 libxcb-xfixes0 libxcb-shape0 libxcb-randr0 \
    libxcb-render-util0 libxcb-util1 libxcb-xinerama0 libxcb-xinput0 \
    libx11-xcb1 libxrender1 libxi6 libfontconfig1 \
    libxcb-cursor0 \
    # Colmap base deps
    libboost-program-options1.74.0 libboost-filesystem1.74.0 libboost-graph1.74.0 \
    libboost-system1.74.0 libboost-serialization1.74.0 \
    libgoogle-glog0v5 libgflags2.2 libglew2.2 libmetis5 \
    libatlas3-base \
    # --- ZMIANA: Zamiast kombinować, instalujemy wersję DEV dla Ceres ---
    libceres-dev \
    # --------------------------------------------------------------------
    # App deps
    libassimp5 libvulkan1 libopencv-core4.5d libopencv-imgcodecs4.5d \
    && rm -rf /var/lib/apt/lists/*

# --- USUNĄŁEM RĘCZNE KOPIOWANIE LIBCERES (apt install libceres-dev to załatwił) ---
COPY --from=colmap_builder /usr/lib/x86_64-linux-gnu/libfreeimage.so* /usr/lib/x86_64-linux-gnu/
COPY --from=colmap_builder /usr/lib/x86_64-linux-gnu/libflann.so* /usr/lib/x86_64-linux-gnu/
COPY --from=colmap_builder /usr/lib/x86_64-linux-gnu/liblz4.so* /usr/lib/x86_64-linux-gnu/# --------------------------------------------

# Kopiowanie Qt
COPY --from=base /opt/qt /opt/qt

# Zmienne środowiskowe Runtime
ENV QT_VERSION=6.6.0
ENV PATH="/opt/qt/${QT_VERSION}/gcc_64/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/qt/${QT_VERSION}/gcc_64/lib:${LD_LIBRARY_PATH}"
ENV QT_PLUGIN_PATH="/opt/qt/${QT_VERSION}/gcc_64/plugins"
ENV QML_IMPORT_PATH="/opt/qt/${QT_VERSION}/gcc_64/qml"
ENV QML2_IMPORT_PATH="/opt/qt/${QT_VERSION}/gcc_64/qml"

# Kopiowanie binarek
COPY --from=colmap_builder /usr/local/bin/colmap /usr/local/bin/colmap
COPY --from=base /usr/local/lib/libonnxruntime.so* /usr/local/lib/
RUN ldconfig

# Ustawienia platformy
ENV QT_QPA_PLATFORM=xcb
ENV XDG_RUNTIME_DIR=/tmp/runtime-app

RUN mkdir -p /tmp/runtime-app && chmod 777 /tmp/runtime-app
RUN useradd -ms /bin/bash appuser
USER appuser
WORKDIR /home/appuser

# Kopiowanie aplikacji
COPY --from=builder /app/src/build/ImageTo3D .
COPY src/models ./models 

CMD ["./ImageTo3D"]
