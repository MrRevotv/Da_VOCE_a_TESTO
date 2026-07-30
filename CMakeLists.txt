cmake_minimum_required(VERSION 3.16)
project(VoiceToChat LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT WIN32)
    message(FATAL_ERROR "Questo progetto usa API Win32 (RegisterHotKey/SendInput) e va compilato su Windows.")
endif()

# --- Build "standalone": tutto statico, niente DLL da distribuire ---------
# 1) Librerie di ggml/whisper/llama compilate DENTRO l'exe invece che come .dll
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# 2) Runtime C++ di Visual Studio linkato staticamente: l'utente finale non
#    deve avere installato il "Visual C++ Redistributable" per eseguirlo.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
# ----------------------------------------------------------------------------

# --- llama.cpp (sottocartella esterna, clonata a mano: vedi README.md) ---
# NOTA: va aggiunta PRIMA di whisper.cpp. Entrambi i progetti portano con sé
# una propria copia della libreria di base "ggml"; whisper.cpp usa un "if
# (NOT TARGET ggml-base)" per evitare di ridefinirla se è già presente.
# Aggiungendo prima llama.cpp (di solito con una ggml più aggiornata),
# whisper.cpp riusa quella stessa copia invece di introdurne una più vecchia
# che manca di funzioni recenti (causa altrimenti errori "identificatore non trovato").
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
# OpenMP richiederebbe una DLL di runtime a parte (vcomp140.dll): la
# disattiviamo per restare davvero senza dipendenze esterne. ggml userà il
# proprio pool di thread interno al posto di OpenMP (leggermente meno
# efficiente su molti core, ma trascurabile per il nostro uso).
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
add_subdirectory(external/llama.cpp)

# --- whisper.cpp (sottocartella esterna, clonata a mano: vedi README.md) ---
add_subdirectory(external/whisper.cpp)

# --- miniaudio è header-only: non serve add_subdirectory, solo include path ---
set(MINIAUDIO_DIR ${CMAKE_SOURCE_DIR}/external/miniaudio)

add_executable(voice_to_chat WIN32
    src/gui.cpp
    src/overlay.cpp
    src/hotkey_listener.cpp
    src/joystick_input.cpp
    src/audio_capture.cpp
    src/whisper_engine.cpp
    src/llm_engine.cpp
    src/input_sender.cpp
    src/app_paths.cpp
    src/app_settings.cpp
    src/glossary.cpp
    src/model_downloader.cpp
    src/screen_capture.cpp
    src/region_selector.cpp
    src/ocr_engine.cpp
    src/ocr_overlay.cpp
    src/chat_parser.cpp
)

target_include_directories(voice_to_chat PRIVATE
    src
    ${MINIAUDIO_DIR}
)

target_link_libraries(voice_to_chat PRIVATE
    whisper
    llama
    user32.lib
    winhttp.lib
    shell32.lib
    windowsapp.lib
)

# windows.h definisce delle macro max/min che confliggono con std::max/std::min
# usati nel nostro codice (errore "token non valido a destra di ::").
# NOMINMAX disattiva quelle macro; WIN32_LEAN_AND_MEAN riduce anche il resto
# di ciò che windows.h include, compilazione un po' più leggera.
target_compile_definitions(voice_to_chat PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)

# Solo il nostro eseguibile ha bisogno di C++20 (per le coroutine di
# C++/WinRT usate dal modulo OCR): llama.cpp/whisper.cpp restano a C++17,
# impostato sopra PRIMA di questa riga, quindi non ne risentono.
set_target_properties(voice_to_chat PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

