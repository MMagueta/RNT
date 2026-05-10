# CMakeList.txt : CMake project for RelationalNT, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("RelationalNT")

find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_path(PICOSHA2_INCLUDE_DIRS "picosha2.h")

# Add source to this project's executable.
add_executable (RelationalNT
    "src/RelationalNT.cpp"
    "src/ObjectManager.cpp"
    "src/PermissionsManager.cpp"
    "src/IdentityManager.cpp"
    "src/LifecycleManager.cpp"
    "src/HandlerManager.cpp"
    "src/CursorManager.cpp"
    "src/TupleCodec.cpp"
    "src/InMemoryBackend.cpp"
    "src/SqliteBackend.cpp"
    "src/NamespaceReferenceManager.cpp"
    "src/VM.cpp"
)

target_include_directories(RelationalNT PRIVATE "include")
target_compile_definitions(RelationalNT PRIVATE NT_CONSOLE_APP)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET RelationalNT PROPERTY CXX_STANDARD 20)
endif()

target_link_libraries(RelationalNT PRIVATE unofficial::sqlite3::sqlite3)
target_include_directories(RelationalNT PRIVATE ${PICOSHA2_INCLUDE_DIRS})

# TODO: Add tests and install targets if needed.
