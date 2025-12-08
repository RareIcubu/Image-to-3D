 # ETAP 1: BASE (Ubuntu 24.04)

FROM ubuntu:24.04 AS base


ENV TZ=Europe/Warsaw

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ >/etc/timezone


# Instalacja zależności deweloperskich

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \

    build-essential cmake ninja-build git pkg-config wget unzip ca-certificates \

    # QT DEV

    qt6-base-dev qt6-base-private-dev qt6-tools-dev qt6-tools-dev-tools \

    libqt6core5compat6-dev qt6-declarative-dev qt6-quick3d-dev \

    libqt6opengl6-dev libqt6shadertools6-dev qt6-quick3d-dev-tools \

    # Biblioteki Assimp

    assimp-utils libassimp-dev \

    # Moduły QML (BASE) - Muszą tu być, żeby COPY je przeniósł!

    qml6-module-qtquick \

    qml6-module-qtquick-window \

    qml6-module-qtquick-controls \

    qml6-module-qtquick-layouts \

    qml6-module-qtqml-workerscript \

    # !!! NAPRAWA: Dodajemy Templates do BASE !!!

    qml6-module-qtquick-templates \

    # -------------------------------------------

    # Inne

    colmap meshlab x11-apps libx11-dev libgl1-mesa-dev \

    libopencv-dev python3-dev python3-numpy \

    libvulkan-dev vulkan-tools \

    && rm -rf /var/lib/apt/lists/*


# Instalacja ONNX Runtime

WORKDIR /tmp

RUN wget -q https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz \

    && tar -xf onnxruntime-linux-x64-1.17.1.tgz \

    && cp -r onnxruntime-linux-x64-1.17.1/include/* /usr/local/include/ \

    && cp -r onnxruntime-linux-x64-1.17.1/lib/* /usr/local/lib/ \

    && rm -rf onnxruntime* \

    && ldconfig


# User

ARG USER_ID

ARG GROUP_ID

RUN userdel -r ubuntu || true && groupdel ubuntu || true

RUN groupadd -g ${GROUP_ID:-1000} devgroup && \

    useradd -l -u ${USER_ID:-1000} -g devgroup -m devuser

USER devuser

WORKDIR /app

CMD ["sleep", "infinity"]


# ETAP 2: BUILDER

FROM base AS builder

COPY --chown=devuser:devgroup src/ /app/src/

WORKDIR /app/src/

RUN rm -rf build && mkdir build && cd build && \

    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release && ninja


# ETAP 3: FINAL (Runtime)

FROM ubuntu:24.04 AS final

ENV TZ=Europe/Warsaw

RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ >/etc/timezone


# Instalacja bibliotek uruchomieniowych

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \

    libqt6widgets6 libqt6gui6 libqt6core6 libqt6core5compat6 \

    # Moduły QML (FINAL)

    qml6-module-qtquick qml6-module-qtquick-window qml6-module-qtquick-controls \

    qml6-module-qtquick-layouts qml6-module-qtqml-workerscript \

    # !!! NAPRAWA: Dodajemy Templates do FINAL !!!

    qml6-module-qtquick-templates \

    libqt6quicktemplates2-6 \

    # --------------------------------------------

    # 3D

    libqt6quick3d6 libqt6quick3dhelpers6 libqt6quick3dassetimport6 \

    libqt6quick3dparticleeffects6 libqt6quick3druntimerender6 \

    libqt6opengl6 libqt6shadertools6 qt6-quick3d-assetimporters-plugin \

    libassimp-dev \

    # Inne

    libvulkan1 libopencv-dev colmap x11-apps libx11-6 libgl1 \

    && rm -rf /var/lib/apt/lists/*


# Kopiowanie ONNX

COPY --from=base /usr/local/lib/libonnxruntime.so* /usr/local/lib/

RUN ldconfig


# Kopiowanie QML i Pluginów

# Teraz folder QML w builderze zawiera Templates, więc kopiujemy dobrą wersję

COPY --from=builder /usr/lib/x86_64-linux-gnu/qt6/qml /usr/lib/x86_64-linux-gnu/qt6/qml

COPY --from=builder /usr/lib/x86_64-linux-gnu/qt6/plugins /usr/lib/x86_64-linux-gnu/qt6/plugins


# Zmienne środowiskowe

ENV QML2_IMPORT_PATH=/usr/lib/x86_64-linux-gnu/qt6/qml

ENV QML_IMPORT_PATH=/usr/lib/x86_64-linux-gnu/qt6/qml

ENV QT_PLUGIN_PATH=/usr/lib/x86_64-linux-gnu/qt6/plugins

ENV QT_QPA_PLATFORM=xcb

ENV XDG_RUNTIME_DIR=/tmp/runtime-app

# Włączamy debugowanie importów QML, jeśli coś jeszcze pójdzie nie tak

ENV QML_IMPORT_TRACE=0


RUN mkdir -p /tmp/runtime-app && chmod 777 /tmp/runtime-app


RUN useradd -ms /bin/bash appuser

USER appuser

WORKDIR /home/appuser


COPY --from=builder /app/src/build/ImageTo3D .

COPY src/models ./models


CMD ["./ImageTo3D"] 
