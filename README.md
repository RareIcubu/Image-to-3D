📸 Projekt: Konwerter Obrazu 2D do 3D

Repozytorium projektu na przedmiot "Grafika i GUI". Jest to aplikacja desktopowa w C++/Qt6, która wykorzystuje Docker do stworzenia spójnego środowiska deweloperskiego.

🚀 Stos technologiczny

    Język: C++ (17/20)

    GUI: Qt 6

    Przetwarzanie obrazu: OpenCV

    Budowanie: CMake + Ninja

    Środowisko: Docker + Docker Compose (na bazie Ubuntu 22.04)

    Kontrola wersji: Git + GitHub

🛠️ 1. Jak zacząć pracę (Środowisko deweloperskie)

Celem jest praca w kontenerze dev. Kontener ten ma już zainstalowane wszystkie zależności (C++, Qt, OpenCV, CMake). Ty piszesz kod na swoim komputerze, a kompilujesz go "wewnątrz" kontenera.

Wymagania wstępne

    Git (do pobrania kodu)

    Docker i Docker Compose (na Linuksie) lub Docker Desktop (na Windows/macOS).

    (Konieczne do GUI) Konfiguracja wyświetlania opisana poniżej.

Konfiguracja wyświetlania GUI (Krytyczne!)

Nasz kontener to Linux, ale musi wyświetlać okna na Twoim komputerze (Windows, Linux, Mac). Wymaga to jednorazowej konfiguracji.

    Na Windows:

        Pobierz i zainstaluj serwer X11, np. VcXsrv: https://sourceforge.net/projects/vcxsrv/

        Uruchom XLaunch (z menu Start).

        Przejdź przez kreator: Multiple windows ➔ Start no client.

        Na ekranie "Extra settings" koniecznie zaznacz "Disable access control". To kluczowe, aby Docker mógł się połączyć.

        Zakończ kreator. Ikona VcXsrv pojawi się w zasobniku systemowym – serwer jest gotowy.

    Na Linuksie (jeśli używasz Docker Desktop): Jeśli przy próbie uruchomienia kontenera (docker-compose up...) dostaniesz błąd mounts denied: /tmp/.X11-unix, musisz ręcznie dodać tę ścieżkę do Docker Desktop:

        Otwórz Docker Desktop > Settings (Ustawienia).

        Idź do Resources > File Sharing.

        Kliknij + i dodaj ścieżkę /tmp/.X11-unix.

        Kliknij Apply & Restart.

Uruchomienie środowiska

Otwórz terminal w głównym folderze projektu (tam, gdzie jest ten plik README).

A. Jeśli jesteś na Linuksie:
Bash

# Uruchamia kontener w tle i go buduje (jeśli trzeba)
docker-compose -f docker-compose.yml -f compose-linux.yml up -d --build

B. Jeśli jesteś na Windows (z uruchomionym VcXsrv):
Bash

# Używamy innego pliku konfiguracyjnego do GUI
docker-compose -f docker-compose.yml -f compose-windows.yml up -d --build

💻 2. Codzienny cykl pracy (Twój Workflow)

Będziesz pracować w dwóch oknach:

    W oknie Edytora Kodu (np. VS Code, Qt Creator, CLion) otwartym na folderze src/ na Twoim komputerze.

    W oknie Terminala połączonym z wnętrzem kontenera.

🖥️ Praca z Qt Creator + Docker (Nasz Workflow)

Nie możesz po prostu kliknąć "Run" (zielonej strzałki) w Qt Creatorze, ponieważ nie ma on dostępu do zależności wewnątrz kontenera. Nasz przepływ pracy opiera się na dwóch programach:

    Qt Creator (jako Edytor Tekstu):

        Uruchom Qt Creator normalnie na swoim komputerze.

        Otwórz projekt przez Plik > Otwórz Projekt i wskaż plik src/CMakeLists.txt.

        Qt Creator zapyta o "Kit" (Zestaw). Możesz zignorować błędy lub wybrać dowolny zestaw. Będziemy go używać tylko do pisania kodu i nawigacji, nie do kompilacji.

    Terminal (jako Kompilator i Uruchamiacz):

        Miej otwarty terminal, w którym jesteś "wewnątrz" kontenera (docker-compose exec dev bash).

        Gdy napiszesz kod w Qt Creatorze i go zapiszesz, przejdź do tego terminala.

Cykl wygląda tak:

    Piszesz kod w Qt Creatorze (np. dodajesz nowy przycisk) i zapisujesz plik.

    Przełączasz się do Terminala (będąc w /app/src/build).

    Kompilujesz zmiany: ninja

    Uruchamiasz aplikację: ./TwojaAplikacja

    Okno aplikacji pojawia się na Twoim pulpicie. Testujesz. Wracasz do pkt 1.

Pierwsze uruchomienie (Kompilacja)

Gdy uruchamiasz projekt po raz pierwszy (lub po git pull, gdy ktoś zmienił CMakeLists.txt):
Bash

# Wejdź do terminala kontenera
docker-compose exec dev bash

# Będąc w /app, przejdź do kodu
cd src

# Stwórz folder budowania (tylko raz)
mkdir build
cd build

# Uruchom CMake (tylko raz)
cmake .. -GNinja

# Skompiluj wszystko (pierwszy raz potrwa dłużej)
ninja

Zatrzymywanie pracy

Gdy kończysz pracę, zamknij aplikację i w terminalu na swoim komputerze (nie w kontenerze) wpisz:
Bash

docker-compose down

📦 3. Wersjonowanie i Wydania (Releases)

Tworzenie "wydania" to ręczny proces składający się z 3 kroków:

    Budowa i publikacja obrazu Docker.

    Tagowanie kodu w Git.

    Stworzenie strony "Release" na GitHubie.

Jak stworzyć nową wersję (np. v1.1.0)

Krok 1: Zmień wersję w kodzie

    Otwórz plik src/CMakeLists.txt.

    Zmień numer wersji w linii project(TwojaAplikacja VERSION 1.0.1 ...). Na przykład na 1.1.0.

    Zatwierdź tę zmianę w Git: git commit -m "Bump version to 1.1.0" i git push.

Krok 2: Zbuduj i wypchnij obraz Docker Będziesz potrzebować konta na rejestrze kontenerów (np. Docker Hub lub GitHub Container Registry (GHCR)).

    Zbuduj obraz produkcyjny:
    Bash

# Buduje etap 'final' z Dockerfile i nadaje mu nazwę
docker build -t moj-obraz-prod --target final .

Zaloguj się (np. do Docker Hub):
Bash

docker login

Otaguj obraz (zmień twojanazwa na swój login):
Bash

docker tag moj-obraz-prod twojanazwa/image-to-3d:1.1.0

Wypchnij obraz do rejestru:
Bash

    docker push twojanazwa/image-to-3d:1.1.0

Krok 3: Stwórz Tag Git i Release na GitHubie

    Stwórz tag Git pasujący do wersji obrazu i wypchnij go:
    Bash

    git tag v1.1.0
    git push origin v1.1.0

    Idź na GitHub do swojego repozytorium i kliknij "Releases" po prawej stronie.

    Kliknij "Draft a new release".

    Wybierz tag v1.1.0, który właśnie wypchnąłeś.

    W opisie koniecznie wklej instrukcję uruchamiania dla użytkownika końcowego (patrz Sekcja 5).

📁 4. Struktura Projektu

.
├── .git/                # Pliki Gita
├── .gitignore           # Mówi Gitowi, co ignorować (WAŻNE: ignoruje src/build/)
├── Dockerfile           # PRZEPIS na obraz (dev i prod, multi-stage)
├──
├── docker-compose.yml   # Plik bazowy dla środowiska DEV
├── compose-linux.yml    # Ustawienia GUI dla Linuksa (dla DEV)
├── compose-windows.yml  # Ustawienia GUI dla Windows (dla DEV)
├──
├── docker-compose.prod.yml   # Plik bazowy dla środowiska PROD (dla użytkownika)
├── compose-prod-linux.yml    # Ustawienia GUI dla Linuksa (dla PROD)
├── compose-prod-windows.yml  # Ustawienia GUI dla Windows (dla PROD)
├──
├── README.md            # Ten plik :)
└── src/
    ├── CMakeLists.txt   # Główny plik budowania C++ (tu dodajesz nowe pliki .cpp)
    ├── config.h.in      # Szablon wersji dla C++
    ├── main.cpp         # Główny plik aplikacji
    └── build/           # (IGNOROWANY PRZEZ GIT) Tu odbywa się kompilacja

🎓 5. Jak Uruchomić Gotową Aplikację (Dla Prowadzącego)

Ta sekcja jest przeznaczona dla użytkownika końcowego, który chce tylko uruchomić gotowy program bez pobierania kodu źródłowego. Obraz jest publikowany ręcznie i dostępny w rejestrze kontenerów.

Wymagania

    Zainstalowany Docker Desktop (dla Windows/Mac) lub Docker (dla Linux).

    (Tylko Windows) Zainstalowany i uruchomiony VcXsrv z opcją "Disable access control" (patrz instrukcja w Sekcji 1).

Instrukcja Uruchomienia

Otwórz terminal (PowerShell na Windows) i wklej jedną komendę odpowiednią dla Twojego systemu. Docker automatycznie pobierze i uruchomi aplikację.

    Uwaga: Poniższy adres URL (twojanazwa/image-to-3d:latest) jest przykładem. Prawidłową komendę do uruchomienia znajdziesz w opisie najnowszego "Release'u" w zakładce "Releases" na stronie tego repozytorium.

A. Uruchomienie na Linuksie (Przykład):
Bash

docker run -it --rm \
    -e DISPLAY=$DISPLAY \
    -v /tmp/.X11-unix:/tmp/.X11-unix \
    twojanazwa/image-to-3d:latest

B. Uruchomienie na Windows (Przykład, z działającym VcXsrv):
Bash

docker run -it --rm \
    -e DISPLAY=host.docker.internal:0.0 \
    twojanazwa/image-to-3d:latest

Aplikacja powinna pojawić się na ekranie po kilku sekundach.
