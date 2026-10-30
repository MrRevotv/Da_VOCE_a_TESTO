cmake_minimum_required(VERSION 3.21)
project(VoiceToChat LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT WIN32)
    message(FATAL_ERROR "Voice to Chat usa le API di Windows (SendInput, DirectInput, OCR di Windows): va compilato su Windows.")
endif()

# --- Un solo .exe, senza DLL -------------------------------------------------
# Librerie ggml/whisper/llama compilate dentro l'exe, runtime di Visual C++
# statico: chi lo scarica non deve installare nulla.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# --- Processori supportati ---------------------------------------------------
# NON ottimizziamo per il processore del PC che compila (GGML_NATIVE): l'exe
# si distribuisce, e su un processore diverso andrebbe in crash ("istruzione
# non valida"). AVX2 + FMA + F16C ci sono su tutti i processori Intel/AMD dal
# 2013-2015 in poi: veloce e compatibile.
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(GGML_AVX    ON  CACHE BOOL "" FORCE)
set(GGML_AVX2   ON  CACHE BOOL "" FORCE)
set(GGML_FMA    ON  CACHE BOOL "" FORCE)
set(GGML_F16C   ON  CACHE BOOL "" FORCE)
set(GGML_BMI2   ON  CACHE BOOL "" FORCE)
set(GGML_AVX512 OFF CACHE BOOL "" FORCE)
# OpenMP vorrebbe una DLL a parte (vcomp140.dll): ggml usa il suo pool di thread.
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)

# --- Scheda video (Vulkan) -------------------------------------------------------
# Traduzione e trascrizione sulla scheda video: molte volte più veloci. Per
# compilare serve il Vulkan SDK (BUILDING.md); chi usa l'exe ha già
# vulkan-1.dll con i driver della scheda video, e senza si va sul processore
# (vulkan-1.dll si carica solo quando serve: /DELAYLOAD più sotto, src/gpu.h).
set(GGML_VULKAN ON CACHE BOOL "" FORCE)

# --- llama.cpp PRIMA di whisper.cpp ---------------------------------------------
# Tutti e due portano la loro copia di ggml: whisper.cpp riusa quella già
# definita ("if (NOT TARGET ggml)"), che con llama.cpp aggiunto prima è la più
# recente delle due.
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TOOLS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_COMMON   OFF CACHE BOOL "" FORCE)
set(LLAMA_CURL           OFF CACHE BOOL "" FORCE)
add_subdirectory(external/llama.cpp)

set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
add_subdirectory(external/whisper.cpp)

# --- Il nostro codice ----------------------------------------------------------
# "vtc_core": tutto tranne le finestre, così lo usano sia il programma sia le
# prove automatiche (vtc_tests).
add_library(vtc_core STATIC
    src/app_paths.cpp
    src/app_settings.cpp
    src/audio_capture.cpp
    src/chat_parser.cpp
    src/engine.cpp
    src/gamer_slang.cpp
    src/glossary.cpp
    src/gpu.cpp
    src/hotkey_listener.cpp
    src/input_sender.cpp
    src/joystick_input.cpp
    src/language_guess.cpp
    src/llm_engine.cpp
    src/log.cpp
    src/model_downloader.cpp
    src/ocr_engine.cpp
    src/screen_capture.cpp
    src/screen_match.cpp
    src/speech_utils.cpp
    src/text_utils.cpp
    src/translation_prompt.cpp
    src/whisper_engine.cpp
)
target_include_directories(vtc_core PUBLIC src external/miniaudio)
target_link_libraries(vtc_core PUBLIC
    whisper llama
    user32 gdi32 winhttp shell32 winmm dinput8 dxguid dxgi ole32 windowsapp
    delayimp
)
# L'exe parte anche su un PC senza vulkan-1.dll: si carica alla prima chiamata.
target_link_options(vtc_core INTERFACE /DELAYLOAD:vulkan-1.dll)
target_compile_definitions(vtc_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
# C++20 solo per il nostro codice (coroutine di C++/WinRT nell'OCR):
# llama.cpp/whisper.cpp restano in C++17.
set_target_properties(vtc_core PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_compile_options(vtc_core PUBLIC /utf-8 /W3 /permissive-)

add_executable(voice_to_chat WIN32
    src/main.cpp
    src/ui/ui.cpp
    src/ui/main_window.cpp
    src/ui/pages.cpp
    src/overlay.cpp
    src/ocr_overlay.cpp
    src/region_selector.cpp
    src/voice_to_chat.rc
    # Temi moderni dei controlli e scala per monitor (125%, 150%...).
    src/voice_to_chat.manifest
)
target_link_libraries(voice_to_chat PRIVATE vtc_core comctl32 uxtheme dwmapi gdiplus shcore)
set_target_properties(voice_to_chat PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(vtc_tests tests/tests.cpp)
# sapi: la voce sintetica di Windows, per le prove complete di whisper.
target_link_libraries(vtc_tests PRIVATE vtc_core sapi)
set_target_properties(vtc_tests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
