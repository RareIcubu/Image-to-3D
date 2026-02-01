# ==========================================
# STAGE 1: COLMAP BUILDER (CUDA Support)
# ==========================================
# Musimy budować na obrazie devel, żeby mieć nagłówki CUDA.
FROM nvidia/cuda:11.8.0-devel-ubuntu22.04 AS colmap_builder

ARG PARALLEL_JOBS=2

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

# Instalacja zależności do budowania COLMAP
RUN apt-get update && apt-get install -y \
    cmake build-essential git curl unzip ninja-build \
    libboost-program-options-dev libboost-filesystem-dev libboost-graph-dev \
    libboost-system-dev libboost-test-dev libboost-serialization-dev \
    libeigen3-dev libfreeimage-dev libgoogle-glog-dev libgflags-dev \
    libglew-dev libmetis-dev libceres-dev \
    libflann-dev liblz4-dev libsqlite3-dev libcgal-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Budowanie COLMAP 3.9.1
# Kompilujemy z CUDA, ale binarka będzie działać (z błędami runtime) na maszynach bez GPU
# jeśli nie zadbamy o logikę w kodzie C++. (My zadbaliśmy w SystemChecks).
RUN git clone --branch 3.9.1 --depth 1 https://github.com/colmap/colmap.git . && \
    mkdir build && cd build && \
    cmake .. -GNinja \
    -DCMAKE_BUILD_TYPE=Release \
    -DGUI_ENABLED=OFF \
    -DCUDA_ENABLED=ON \
    -DCMAKE_CUDA_ARCHITECTURES="60;61;70;75;80;86" \
    && ninja -j ${PARALLEL_JOBS} && ninja install

# ==========================================
# STAGE 2: QT6 & DEPENDENCIES BASE
# ==========================================
# Używamy wersji runtime - lżejsza, ma biblioteki, nie ma kompilatora CUDA.
# To jest bezpieczne dla maszyn bez GPU (po prostu nie użyją funkcji CUDA).
FROM nvidia/cuda:11.8.0-runtime-ubuntu22.04 AS base

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Warsaw

# Instalacja Qt6 deps, X11, Wayland, OpenGL i bibliotek runtime dla Colmap
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git python3-pip \
    libgl1-mesa-dev libvulkan1 libxkbcommon-x11-0 \
    libxcb1 libxcb-glx0 libxcb-keysyms1 libxcb-image0 libxcb-shm0 \
    libxcb-icccm4 libxcb-sync1 libxcb-xfixes0 libxcb-shape0 libxcb-randr0 \
    libxcb-render-util0 libxcb-util1 libxcb-xinerama0 libxcb-xinput0 \
    libx11-xcb1 libxrender1 libxi6 libfontconfig1 \
    libxcb-cursor0 libdbus-1-3 libpulse0 \
    # Colmap Runtime Dependencies (muszą pasować do wersji z buildera)
    libboost-program-options1.74.0 libboost-filesystem1.74.0 libboost-graph1.74.0 \
    libboost-system1.74.0 libboost-serialization1.74.0 \
    libgoogle-glog0v5 libgflags2.2 libglew2.2 libmetis5 \
    libfreeimage3 libflann1.9 liblz4-1 libsqlite3-0 libcgal-dev \
    libceres-dev \
    # App Dependencies
    libassimp5 libopencv-dev assimp-utils \
    p7zip-full wget ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Instalacja Qt via aqtinstall
RUN pip3 install --upgrade aqtinstall
ENV QT_VERSION=6.7.2
# aqtinstall uses 'linux_gcc_64' for 6.7.x, but the folder it creates is 'gcc_64'
ENV QT_ARCH=gcc_64
RUN aqt install-qt linux desktop ${QT_VERSION} linux_${QT_ARCH} -O /opt/qt --external 7z \
    -m qt5compat qtimageformats qt3d qtquick3d qtshadertools qtquicktimeline

# FIX: Nadajemy uprawnienia dla wszystkich użytkowników (ważne dla devuser)
RUN chmod -R 755 /opt/qt

# Instalacja ONNX Runtime
WORKDIR /tmp
RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz \
    && tar -xf onnxruntime-linux-x64-1.17.1.tgz \
    && cp -r onnxruntime-linux-x64-1.17.1/include/* /usr/local/include/ \
    && cp -r onnxruntime-linux-x64-1.17.1/lib/* /usr/local/lib/ \
    && rm -rf onnxruntime* \
    && ldconfig

# Kopiowanie binarki COLMAP z buildera
COPY --from=colmap_builder /usr/local/bin/colmap /usr/local/bin/colmap
# Colmap instaluje też biblioteki w /usr/local/lib? Czasem tak. Sprawdźmy to bezpiecznie.
# W stage 1 było 'ninja install'. Zwykle idzie do /usr/local/lib.
# Skopiujmy je na wszelki wypadek, jeśli nie są systemowe.
COPY --from=colmap_builder /usr/local/share/colmap /usr/local/share/colmap

# Zmienne środowiskowe Qt
ENV PATH="/opt/qt/${QT_VERSION}/${QT_ARCH}/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/qt/${QT_VERSION}/${QT_ARCH}/lib:${LD_LIBRARY_PATH}"
ENV QT_PLUGIN_PATH="/opt/qt/${QT_VERSION}/${QT_ARCH}/plugins"
ENV QML_IMPORT_PATH="/opt/qt/${QT_VERSION}/${QT_ARCH}/qml"
ENV QML2_IMPORT_PATH="/opt/qt/${QT_VERSION}/${QT_ARCH}/qml"
ENV CMAKE_PREFIX_PATH="/opt/qt/${QT_VERSION}/${QT_ARCH}"

# FIX: Dodajemy biblioteki Qt do systemowego cache linkera (ldconfig).
# To naprawia błąd "cannot open shared object file", bo LD_LIBRARY_PATH bywa zawodne.
RUN echo "/opt/qt/${QT_VERSION}/${QT_ARCH}/lib" > /etc/ld.so.conf.d/qt6.conf && ldconfig

ARG USER_ID
ARG GROUP_ID
RUN groupadd -g ${GROUP_ID:-1000} devgroup && \
    useradd -l -u ${USER_ID:-1000} -g devgroup -m devuser

# Fix: Ustawiamy domyślny folder roboczy na /app, żeby nie lądować w /tmp
WORKDIR /app
USER devuser
# CMD ["sleep", "infinity"] - to jest nadpisywane w docker-compose, ale warto mieć

# ==========================================
# STAGE 3: APPLICATION BUILDER
# ==========================================

# ==========================================
# STAGE 3: APPLICATION BUILDER
# ==========================================
FROM base AS builder
ARG PARALLEL_JOBS=2
COPY --chown=devuser:devgroup src/ /app/src/
WORKDIR /app/src/
RUN rm -rf build && mkdir build && cd build && \
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release && ninja -j ${PARALLEL_JOBS}

# ==========================================
# STAGE 4: FINAL PRODUCTION IMAGE
# ==========================================
FROM base AS final

ENV XDG_RUNTIME_DIR=/tmp/runtime-app
RUN mkdir -p /tmp/runtime-app && chmod 777 /tmp/runtime-app

# Użytkownik runtime
RUN useradd -ms /bin/bash appuser
USER appuser
WORKDIR /home/appuser

# Kopiowanie zbudowanej aplikacji
COPY --from=builder /app/src/build/ImageTo3D .
COPY src/models ./models

# Ustawienie renderowania (Software fallback jeśli brak GPU)
# QT_QPA_PLATFORM=xcb jest standardem dla X11
ENV QT_QPA_PLATFORM=xcb
# Ważne dla uniknięcia błędów OpenGL na maszynach bez GPU:
ENV QT_XCB_GL_INTEGRATION=xcb_egl

CMD ["./ImageTo3D"]
