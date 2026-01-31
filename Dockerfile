# --- STAGE 1: COLMAP BUILDER (CUDA Support) ---
# Używamy oficjalnego obrazu NVIDIA, żeby mieć kompilator NVCC i biblioteki CUDA
FROM nvidia/cuda:11.8.0-devel-ubuntu22.04 AS colmap_builder

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

# Instalacja zależności do budowania COLMAP
# Budujemy wersję CLI (bez GUI), więc nie potrzebujemy Qt5 w tym etapie
RUN apt-get update && apt-get install -y \
    cmake build-essential git \
    libboost-program-options-dev libboost-filesystem-dev libboost-graph-dev \
    libboost-system-dev libboost-test-dev libboost-serialization-dev \
    libeigen-dev libfreeimage-dev libgoogle-glog-dev libgflags-dev \
    libglew-dev libmetis-dev libceres-dev \
    ninja-build curl unzip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Pobieramy COLMAP w wersji 3.9.1 (Stabilna, sprawdzona z CUDA)
# Flagy kluczowe:
# - GUI_ENABLED=OFF: Eliminuje zależności X11 i Qt5 (brak konfliktów z naszym Qt6)
# - CUDA_ENABLED=ON: Włącza akcelerację GPU
RUN git clone --branch 3.9.1 --depth 1 https://github.com/colmap/colmap.git . && \
    mkdir build && cd build && \
    cmake .. -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DGUI_ENABLED=OFF \
        -DCUDA_ENABLED=ON \
        -DCUDA_ARCHS="all" \
    && ninja && ninja install

# --- STAGE 2: QT6 BASE ---
FROM nvidia/cuda:11.8.0-runtime-ubuntu22.04 AS base

ENV TZ=Europe/Warsaw
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ >/etc/timezone

# Instalujemy Runtime dependencies dla COLMAP i nasze Qt6
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git pkg-config \
    # Colmap Runtime Deps
    libboost-program-options1.74.0 libboost-filesystem1.74.0 libboost-graph1.74.0 \
    libboost-system1.74.0 libboost-serialization1.74.0 \
    libfreeimage3 libgoogle-glog0v5 libgflags2.2 libglew2.2 libmetis5 libceres2 \
    # Qt6 i narzędzia
    qt6-base-dev qt6-base-private-dev qt6-tools-dev qt6-tools-dev-tools \
    libqt6core5compat6-dev qt6-declarative-dev qt6-quick3d-dev \
    libqt6opengl6-dev libqt6shadertools6-dev qt6-quick3d-dev-tools \
    qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qtqml-workerscript qml6-module-qtquick-templates \
    libqt6quicktemplates2-6 \
    # Assimp & OpenCV & ONNX
    assimp-utils libassimp-dev libopencv-dev wget \
    # Systemowe
    x11-apps libx11-dev libgl1-mesa-dev libvulkan-dev vulkan-tools \
    && rm -rf /var/lib/apt/lists/*

# Instalacja ONNX Runtime (CPU/GPU compat)
WORKDIR /tmp
RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz \
    && tar -xf onnxruntime-linux-x64-1.17.1.tgz \
    && cp -r onnxruntime-linux-x64-1.17.1/include/* /usr/local/include/ \
    && cp -r onnxruntime-linux-x64-1.17.1/lib/* /usr/local/lib/ \
    && rm -rf onnxruntime* \
    && ldconfig

# Kopiujemy skompilowany COLMAP z pierwszego etapu
COPY --from=colmap_builder /usr/local/bin/colmap /usr/local/bin/colmap
# (Opcjonalnie) Kopiujemy biblioteki jeśli COLMAP zbudował jakieś statyczne/współdzielone niestandardowe
# Ale przy apt-get install libceres-dev w obu etapach powinno działać.

ARG USER_ID
ARG GROUP_ID
RUN groupadd -g ${GROUP_ID:-1000} devgroup && \
    useradd -l -u ${USER_ID:-1000} -g devgroup -m devuser

USER devuser
WORKDIR /app
CMD ["sleep", "infinity"]

# --- STAGE 3: BUILDER (Nasza aplikacja) ---
FROM base AS builder
COPY --chown=devuser:devgroup src/ /app/src/
WORKDIR /app/src/
RUN rm -rf build && mkdir build && cd build && \
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release && ninja

# --- STAGE 4: FINAL (Prod) ---
FROM nvidia/cuda:11.8.0-runtime-ubuntu22.04 AS final

ENV TZ=Europe/Warsaw
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ >/etc/timezone

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    # Minimal Qt6 libs
    libqt6widgets6 libqt6gui6 libqt6core6 libqt6core5compat6 \
    qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qtqml-workerscript qml6-module-qtquick-templates \
    libqt6quicktemplates2-6 \
    libqt6quick3d6 libqt6quick3dhelpers6 libqt6quick3dassetimport6 \
    libqt6quick3dparticleeffects6 libqt6quick3druntimerender6 \
    libqt6opengl6 libqt6shadertools6 qt6-quick3d-assetimporters-plugin \
    # Colmap Deps
    libboost-program-options1.74.0 libboost-filesystem1.74.0 libboost-graph1.74.0 \
    libboost-system1.74.0 libboost-serialization1.74.0 \
    libfreeimage3 libgoogle-glog0v5 libgflags2.2 libglew2.2 libmetis5 libceres2 \
    # Inne
    libassimp-dev libvulkan1 libopencv-dev x11-apps libx11-6 libgl1 \
    && rm -rf /var/lib/apt/lists/*

# Kopiujemy COLMAP
COPY --from=colmap_builder /usr/local/bin/colmap /usr/local/bin/colmap

# Kopiujemy ONNX
COPY --from=base /usr/local/lib/libonnxruntime.so* /usr/local/lib/
RUN ldconfig

# Kopiujemy QML
COPY --from=builder /usr/lib/x86_64-linux-gnu/qt6/qml /usr/lib/x86_64-linux-gnu/qt6/qml
COPY --from=builder /usr/lib/x86_64-linux-gnu/qt6/plugins /usr/lib/x86_64-linux-gnu/qt6/plugins

ENV QML_IMPORT_PATH=/usr/lib/x86_64-linux-gnu/qt6/qml
ENV QT_PLUGIN_PATH=/usr/lib/x86_64-linux-gnu/qt6/plugins
ENV QT_QPA_PLATFORM=xcb
ENV XDG_RUNTIME_DIR=/tmp/runtime-app

RUN mkdir -p /tmp/runtime-app && chmod 777 /tmp/runtime-app
RUN useradd -ms /bin/bash appuser
USER appuser
WORKDIR /home/appuser

COPY --from=builder /app/src/build/ImageTo3D .
COPY src/models ./models

CMD ["./ImageTo3D"]