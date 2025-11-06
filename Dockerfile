#
# === ETAP 1: "BASE" (Obraz dla DEV) ===
# Zawiera WSZYSTKIE narzędzia deweloperskie i zależności.
#
FROM ubuntu:22.04 AS base

ENV TZ=Europe/Warsaw
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Instalujemy WSZYSTKIE zależności deweloperskie
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    qt6-base-dev \
    qt6-base-private-dev \
    qt6-tools-dev \
    qt6-tools-dev-tools \
    libopencv-dev \
    x11-apps \
    libx11-dev \
    libgl1-mesa-dev \
    && apt-get autoremove -y && apt-get clean && rm -rf /var/lib/apt/lists/*

# Tworzymy użytkownika, aby 'dev' nie działał jako root
ARG USER_ID
ARG GROUP_ID
RUN groupadd -g ${GROUP_ID:-1000} devgroup \
    && useradd -l -u ${USER_ID:-1000} -g devgroup -m devuser
USER devuser
WORKDIR /app

# Domyślna komenda dla 'dev' - po prostu czekaj
CMD ["sleep", "infinity"]


#
# === ETAP 2: "BUILDER" (Etap pośredni do budowania PROD) ===
# Używa 'base' (ze wszystkimi narzędziami), aby skompilować aplikację
#
FROM base AS builder

# Skopiuj kod źródłowy z komputera
COPY src/ /app/src/

# Skompiluj aplikację w trybie Release
RUN cd /app/src && \
    mkdir build && cd build && \
    cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release && \
    ninja
# Ten etap zawiera teraz skompilowany plik w /app/src/build/TwojaAplikacja


#
# === ETAP 3: "FINAL" (Obraz PROD - "tylko do użytku") ===
# Zaczyna od nowa, od czystego Ubuntu
#
FROM ubuntu:22.04 AS final

ENV TZ=Europe/Warsaw
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Instalujemy TYLKO biblioteki RUNTIME (lekkie)
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    libqt6widgets6 \
    libqt6gui6 \
    libqt6core6 \
    libopencv-core4.5d \
    libopencv-imgproc4.5d \
    libopencv-imgcodecs4.5d \
    x11-apps \
    libx11-6 \
    libgl1 \
    && apt-get autoremove -y && apt-get clean && rm -rf /var/lib/apt/lists/*

# Tworzymy użytkownika dla aplikacji
RUN useradd -ms /bin/bash appuser
USER appuser
WORKDIR /home/appuser

# Skopiuj TYLKO gotowy, skompilowany program z etapu "builder"
# Zmień "TwojaAplikacja" na poprawną nazwę
COPY --from=builder /app/src/build/TwojaAplikacja .

# Ustaw domyślną komendę na uruchomienie aplikacji
CMD ["./TwojaAplikacja"]
