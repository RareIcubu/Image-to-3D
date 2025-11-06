---

# 📸 Projekt: Konwerter Obrazu 2D do 3D

Repozytorium projektu na przedmiot **Grafika i GUI**.
Aplikacja desktopowa w **C++/Qt6**, korzystająca z **Docker** do stworzenia spójnego środowiska deweloperskiego.

---

## 📑 Spis treści

1. [🚀 Stos technologiczny](#-stos-technologiczny)
2. [🛠️ Jak zacząć pracę (Środowisko deweloperskie)](#️-1-jak-zacząć-pracę-środowisko-deweloperskie)

   * [Wymagania wstępne](#wymagania-wstępne)
   * [Konfiguracja wyświetlania GUI](#-konfiguracja-wyświetlania-gui-krytyczne)
   * [Uruchomienie środowiska](#-uruchomienie-środowiska)
3. [💻 Codzienny cykl pracy (Workflow)](#-2-codzienny-cykl-pracy-workflow)

   * [Praca z Qt Creator + Docker](#-praca-z-qt-creator--docker)
   * [Pierwsze uruchomienie](#-pierwsze-uruchomienie-kompilacja)
   * [Zatrzymywanie pracy](#-zatrzymywanie-pracy)
4. [📦 Wersjonowanie i wydania (Releases)](#-3-wersjonowanie-i-wydania-releases)
5. [📁 Struktura projektu](#-4-struktura-projektu)
6. [🎓 Jak uruchomić gotową aplikację (Dla prowadzącego)](#-5-jak-uruchomić-gotową-aplikację-dla-prowadzącego)

---

## 🚀 Stos technologiczny

| Komponent                | Technologia                            |
| ------------------------ | -------------------------------------- |
| **Język**                | C++ (C++17 / C++20)                    |
| **GUI**                  | Qt 6                                   |
| **Przetwarzanie obrazu** | OpenCV                                 |
| **Budowanie**            | CMake + Ninja                          |
| **Środowisko**           | Docker + Docker Compose (Ubuntu 22.04) |
| **Kontrola wersji**      | Git + GitHub                           |

---

## 🛠️ 1. Jak zacząć pracę (Środowisko deweloperskie)

Kontener **dev** zawiera wszystkie potrzebne zależności (C++, Qt, OpenCV, CMake).
Kod edytujesz lokalnie, a kompilacja odbywa się **wewnątrz kontenera**.

---

### Wymagania wstępne

* **Git** — pobranie repozytorium
* **Docker** i **Docker Compose** (Linux) lub **Docker Desktop** (Windows/macOS)
* **Konfiguracja wyświetlania GUI** (patrz niżej)

---

### 🖥️ Konfiguracja wyświetlania GUI (krytyczne)

Kontener to system **Linux**, ale musi wyświetlać okna na Twoim komputerze.

#### 🔹 Windows

1. Pobierz i zainstaluj [**VcXsrv**](https://sourceforge.net/projects/vcxsrv/).
2. Uruchom **XLaunch**:

   * *Multiple windows*
   * *Start no client*
   * W zakładce *Extra settings* → zaznacz **Disable access control**
3. Po uruchomieniu ikona VcXsrv powinna być widoczna w zasobniku systemowym.

#### 🔹 Linux (Docker Desktop)

Jeśli przy starcie pojawia się błąd:

```
mounts denied: /tmp/.X11-unix
```

dodaj ścieżkę ręcznie:

1. Otwórz **Docker Desktop → Settings → Resources → File Sharing**
2. Kliknij `+` i dodaj `/tmp/.X11-unix`
3. Kliknij **Apply & Restart**

---

### 🚀 Uruchomienie środowiska

W terminalu w folderze głównym projektu:

#### Linux

```bash
docker-compose -f docker-compose.yml -f compose-linux.yml up -d --build
```

#### Windows (z działającym VcXsrv)

```bash
docker-compose -f docker-compose.yml -f compose-windows.yml up -d --build
```

---

## 💻 2. Codzienny cykl pracy (Workflow)

Będziesz pracować w dwóch oknach:

1. **Edytor kodu** — np. VS Code, Qt Creator lub CLion (folder `src/`)
2. **Terminal** — połączony z kontenerem:

   ```bash
   docker-compose exec dev bash
   ```

---

### 🖥️ Praca z Qt Creator + Docker

Qt Creator działa jako **edytor**, a kompilacja i uruchamianie odbywają się **w kontenerze**.

#### Qt Creator

1. Uruchom lokalnie.
2. Otwórz projekt: `Plik → Otwórz Projekt → src/CMakeLists.txt`
3. Zignoruj błędy dotyczące „Kit” – nie będą używane.

#### Terminal (kompilacja)

1. Po zapisaniu zmian w Qt Creatorze:

   ```bash
   cd /app/src/build
   ninja
   ./TwojaAplikacja
   ```
2. Aplikacja otworzy się na Twoim pulpicie.

---

### 🧱 Pierwsze uruchomienie (kompilacja)

```bash
# Wejdź do kontenera
docker-compose exec dev bash

# Przejdź do katalogu źródłowego
cd /app/src

# Stwórz folder build
mkdir build && cd build

# Konfiguracja CMake
cmake .. -GNinja

# Kompilacja
ninja
```

---

### 📴 Zatrzymywanie pracy

Po zakończeniu sesji:

```bash
docker-compose down
```

---

## 📦 3. Wersjonowanie i wydania (Releases)

Tworzenie wydania składa się z trzech kroków:

1. 🔧 Zmiana wersji w kodzie
2. 🐳 Budowa i publikacja obrazu Docker
3. 🏷️ Tagowanie i release na GitHubie

---

### 🔹 Krok 1: Zmiana wersji

W pliku `src/CMakeLists.txt`:

```cmake
project(TwojaAplikacja VERSION 1.1.0 ...)
```

Zatwierdź:

```bash
git commit -am "Bump version to 1.1.0"
git push
```

---

### 🔹 Krok 2: Budowa i publikacja obrazu Docker

```bash
docker build -t moj-obraz-prod --target final .
docker login
docker tag moj-obraz-prod twojanazwa/image-to-3d:1.1.0
docker push twojanazwa/image-to-3d:1.1.0
```

---

### 🔹 Krok 3: Tag i Release na GitHubie

```bash
git tag v1.1.0
git push origin v1.1.0
```

Na GitHubie:

1. Otwórz **Releases → Draft a new release**
2. Wybierz tag `v1.1.0`
3. W opisie dodaj instrukcję uruchamiania (sekcja 5)

---

## 📁 4. Struktura projektu

```
.
├── .git/
├── .gitignore                # Ignoruje m.in. src/build/
├── Dockerfile                # Multi-stage build (dev + prod)
│
├── docker-compose.yml        # Bazowy plik DEV
├── compose-linux.yml         # GUI dla Linuksa (DEV)
├── compose-windows.yml       # GUI dla Windows (DEV)
│
├── docker-compose.prod.yml   # Bazowy plik PROD
├── compose-prod-linux.yml    # GUI dla Linuksa (PROD)
├── compose-prod-windows.yml  # GUI dla Windows (PROD)
│
├── README.md
└── src/
    ├── CMakeLists.txt
    ├── config.h.in
    ├── main.cpp
    └── build/                # Ignorowany przez Git
```

---

## 🎓 5. Jak uruchomić gotową aplikację (Dla prowadzącego)

Sekcja dla użytkownika końcowego, który chce **uruchomić aplikację bez kompilacji**.

---

### Wymagania

* **Docker Desktop** (Windows/Mac) lub **Docker** (Linux)
* (Windows) Uruchomiony **VcXsrv** z opcją *Disable access control*

---

### 🔹 Uruchomienie aplikacji

W terminalu (PowerShell/Bash) wklej komendę odpowiednią dla systemu.
Docker sam pobierze obraz i uruchomi aplikację.

> ⚠️ Prawidłowy tag obrazu znajdziesz w opisie najnowszego **Release** na GitHubie.

#### Linux

```bash
docker run -it --rm \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    twojanazwa/image-to-3d:latest
```

#### Windows

```bash
docker run -it --rm \
    -e DISPLAY=host.docker.internal:0.0 \
    twojanazwa/image-to-3d:latest
```

Po kilku sekundach aplikacja powinna pojawić się na ekranie. 🎉

---
