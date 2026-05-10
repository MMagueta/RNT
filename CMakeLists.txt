cmake_minimum_required(VERSION 3.20)
project(RelationalNT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Toggle the "Press Enter to exit" prompt. ON by default for interactive runs;
# set -DNT_WAIT_ON_EXIT=OFF on the cmake command line to disable (e.g. in CI).
option(NT_WAIT_ON_EXIT "Wait for Enter key before exiting the console app" ON)

set(SOURCES
    src/CursorManager.cpp
    src/HandlerManager.cpp
    src/IdentityManager.cpp
    src/LifecycleManager.cpp
    src/NamespaceReferenceManager.cpp
    src/NT.cpp
    src/ObjectManager.cpp
    src/PermissionsManager.cpp
    src/VM.cpp
)

add_executable(RNT ${SOURCES})

target_include_directories(RNT PRIVATE include)

target_compile_definitions(RNT PRIVATE NT_CONSOLE_APP)

if(NT_WAIT_ON_EXIT)
    target_compile_definitions(RNT PRIVATE NT_WAIT_ON_EXIT)
endif()

if(MSVC)
    target_compile_options(RNT PRIVATE /W4)
else()
    target_compile_options(RNT PRIVATE -Wall -Wextra)
endif()
