# 📸 ImageTo3D: Konwerter Obrazu 2D do 3D

> **Projekt na przedmiot: Grafika komputerowa i GUI**

Zaawansowana aplikacja desktopowa w **C++/Qt6**, umożliwiająca rekonstrukcję modeli 3D ze zdjęć przy użyciu dwóch metod: klasycznej fotogrametrii oraz sztucznej inteligencji. Całość zamknięta w kontenerze **Docker** dla zapewnienia powtarzalności środowiska.

---

## ✨ Główne funkcjonalności

1.  **Metoda Hybrydowa:**
    * 🏛️ **Fotogrametria (COLMAP):** Rekonstrukcja wysokiej jakości z serii zdjęć (Structure-from-Motion + Multi-View Stereo). Wykorzystuje akcelerację CUDA.
    * 🧠 **AI (ONNX Runtime):** Szybka estymacja głębi z pojedynczego zdjęcia (MiDaS) i konwersja do modelu 3D.
2.  **Generowanie Siatki (Meshing):**
    * Automatyczna konwersja chmury punktów do siatki trójkątów (Poisson Reconstruction) przy użyciu **Open3D**.
    * Inteligentne mapowanie kolorów z chmury punktów na wierzchołki modelu.
3.  **Wbudowany Viewer 3D (Qt Quick 3D):**
    * Obsługa formatów `.ply`, `.obj`, `.glb`.
    * **Auto-Fit:** Inteligentne skalowanie i centrowanie modelu niezależnie od jego rozmiaru.
    * Podgląd hybrydowy (Siatka / Chmura punktów).
    * Dynamiczna zmiana oświetlenia i edycja transformacji (Skala/Obrót/Pozycja).

---

## 🛠️ Stos technologiczny

| Kategoria | Technologia | Rola w projekcie |
| :--- | :--- | :--- |
| **Core** | **C++17** | Główny język logiki aplikacji. |
| **GUI** | **Qt 6.7 (QML)** | Nowoczesny interfejs użytkownika i rendering 3D. |
| **Fotogrametria** | **COLMAP (CUDA)** | Ekstrakcja cech, dopasowywanie i gęsta rekonstrukcja. |
| **AI / ML** | **ONNX Runtime** | Uruchamianie modelu MiDaS (Depth Estimation). |
| **Przetwarzanie 3D** | **Open3D 0.17** | Przetwarzanie chmur punktów, generowanie meshy. |
| **Obraz** | **OpenCV 4.5** | Manipulacja obrazami i mapami głębi. |
| **Build System** | **CMake + Ninja** | Szybka kompilacja wielowątkowa. |
| **Środowisko** | **Docker** | Izolacja zależności (Ubuntu 22.04 + Nvidia Drivers). |

---

## 🚀 Jak zacząć (Instalacja)

### Wymagania wstępne

1.  **System:** Linux (zalecane) lub Windows (WSL2).
2.  **GPU:** Karta graficzna NVIDIA + zainstalowane sterowniki na hoście.
3.  **Docker:** Zainstalowany Docker Engine.
4.  **NVIDIA Container Toolkit:**
    * Kluczowe dla działania COLMAP na GPU wewnątrz kontenera.
    * Test poprawności instalacji: `docker run --rm --gpus all nvidia/cuda:11.8.0-base-ubuntu22.04 nvidia-smi`

### Konfiguracja wyświetlania okien (X11)

Kontener musi mieć dostęp do serwera X11 hosta, aby wyświetlić GUI.

* **Linux:**
    Zazwyczaj działa automatycznie dzięki mapowaniu `/tmp/.X11-unix`.
    W razie problemów z uprawnieniami:
    ```bash
    xhost +local:docker
    ```

* **Windows (WSL2):**
    1. Zainstaluj **VcXsrv**.
    2. Uruchom **XLaunch** z ustawieniami:
        * ✅ Multiple windows
        * ✅ Disable access control (Krytyczne!)
        * ❌ Native opengl (Odznacz, jeśli okno znika)

---

## 💻 Cykl pracy (Development)

Projekt wykorzystuje `docker compose` do zarządzania środowiskiem deweloperskim.

### 1. Uruchomienie kontenera
Ta komenda zbuduje obraz (z bibliotekami Open3D, Qt, Colmap) i uruchomi kontener w tle.

```bash

### 2. Wejście do środowiska

```bash
docker compose exec dev bash
```

### 3. Kompilacja i Uruchomienie

Wewnątrz kontenera:

```bash
# Przejdź do folderu budowania
cd src/build

# Skonfiguruj i zbuduj (używamy Ninja dla szybkości)
cmake .. -GNinja
ninja

# Uruchom aplikację
./ImageTo3D
```

---

## 📖 Instrukcja Obsługi

### Tryb 1: Fotogrametria (COLMAP)

Dla najlepszej jakości. Wymaga serii zdjęć obiektu dookoła (min. 5-10).

1. W drzewie plików po lewej zaznacz **Folder** ze zdjęciami.
2. Wybierz metodę: **Fotogrametria (COLMAP)**.
3. Kliknij **START**.
4. Aplikacja automatycznie wykona: Feature Extraction -> Matching -> Sparse Reconstruction -> Dense Stereo -> Meshing.

### Tryb 2: AI (Single Image)

Dla szybkiego podglądu z jednego zdjęcia.

1. W drzewie plików rozwiń folder i zaznacz **pojedyncze zdjęcie** (lub kilka z wciśniętym `Ctrl` dla przetwarzania wsadowego).
2. Wybierz model AI (np. `midas_v21.onnx`).
3. Kliknij **START**.
4. Aplikacja wygeneruje mapę głębi, przekształci ją w chmurę punktów i nałoży teksturę.

### Nawigacja w Viewerze

* **LPM + Mysz:** Obracanie kamery (Orbit).
* **PPM + Mysz:** Przesuwanie kamery (Pan).
* **Scroll:** Przybliżanie (Zoom).
* **Prawy Panel:** Suwaki do ręcznej korekty rotacji/skali/pozycji (X/Y/Z).
* **Przycisk "Centruj i Normalizuj":** Automatycznie dopasowuje mikroskopijne lub gigantyczne modele do widoku kamery.

---

## 🔧 Rozwiązywanie problemów

### 🔴 `docker: Error response from daemon: could not select device driver "nvidia"`
https://github.com/RareIcubu/Image-to-3D
Docker nie widzi Twojej karty graficznej lub toolkita.

1. Zainstaluj `nvidia-container-toolkit`.
2. Upewnij się, że w `/etc/docker/daemon.json` jest sekcja `runtimes`:
```json
{
    "data-root": "/home/docker-data",
    "default-runtime": "runc",
    "runtimes": {
        "nvidia": {
            "path": "nvidia-container-runtime",
            "runtimeArgs": []
        }
    }
}
```


3. `sudo systemctl restart docker`

### 🔴 `No space left on device` podczas budowania

Obrazy Docker z CUDA i bibliotekami C++ są duże (>10GB tymczasowo).

1. Wyczyść cache: `docker system prune -a --volumes`.
2. Jeśli masz mało miejsca na partycji root (`/`), przenieś dane Dockera na inną partycję (edycja `data-root` w `daemon.json`).

### 🔴 COLMAP Crash (Kod 6 / SIGABRT)

Zazwyczaj oznacza brak pamięci VRAM na karcie graficznej przy ustawieniach "High".

* **Rozwiązanie:** W menu *Plik -> Preferencje* zmień jakość rekonstrukcji na **Medium**.

---

## 👥 Autorzy
Jakub Jasiński, Kamil Pojedynek, Kacper Ulanowski
All rights reserved © 2025.
