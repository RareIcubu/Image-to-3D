# ETAP 1: BASE
FROM ubuntu:22.04 AS base

ENV TZ=Europe/Warsaw
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ >/etc/timezone

# Instalacja narzędzi (OpenCV 4.5.4 z APT)
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git pkg-config wget unzip ca-certificates \
    qt6-base-dev qt6-base-private-dev qt6-tools-dev qt6-tools-dev-tools libqt6core5compat6-dev \
    colmap meshlab x11-apps libx11-dev libgl1-mesa-dev \
    libopencv-dev python3-dev python3-numpy \
    && rm -rf /var/lib/apt/lists/*

# Instalacja ONNX Runtime 1.17.1
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
RUN groupadd -g ${GROUP_ID:-1000} devgroup && \
    useradd -l -u ${USER_ID:-1000} -g devgroup -m devuser
USER devuser
WORKDIR /app
CMD ["sleep", "infinity"]

# ETAP 2: BUILDER
FROM base AS builder
COPY src/ /app/src/
WORKDIR /app/src/
RUN rm -rf build && mkdir build && cd build && \
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release && ninja

# ETAP 3: FINAL
FROM ubuntu:22.04 AS final
ENV TZ=Europe/Warsaw
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ >/etc/timezone

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libqt6widgets6 libqt6gui6 libqt6core6 libqt6core5compat6 \
    colmap x11-apps libx11-6 libgl1 \
    libopencv-core4.5d libopencv-imgproc4.5d libopencv-imgcodecs4.5d libopencv-dnn4.5d \
    libopencv-calib3d4.5d libopencv-features2d4.5d \
    && rm -rf /var/lib/apt/lists/*

# Kopiowanie bibliotek ONNX Runtime
COPY --from=base /usr/local/lib/libonnxruntime.so* /usr/local/lib/
RUN ldconfig

RUN useradd -ms /bin/bash appuser
USER appuser
WORKDIR /home/appuser
COPY --from=builder /app/src/build/ImageTo3D .
# Kopiujemy modele do obrazu produkcyjnego
COPY src/models ./models

CMD ["./ImageTo3D"]
