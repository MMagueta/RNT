# Install script for directory: C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/src/nf-3530100-fcf7ac0798.clean

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/pkgs/sqlite3_x64-windows")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "OFF")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/x64-windows-rel/sqlite3.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/x64-windows-rel/sqlite3.dll")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
      "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/src/nf-3530100-fcf7ac0798.clean/sqlite3.h"
      "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/src/nf-3530100-fcf7ac0798.clean/sqlite3ext.h"
      "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/src/nf-3530100-fcf7ac0798.clean/sqlite3-vcpkg-config.h"
      )
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/unofficial-sqlite3/unofficial-sqlite3-targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/unofficial-sqlite3/unofficial-sqlite3-targets.cmake"
         "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/x64-windows-rel/CMakeFiles/Export/2ca667fbbca39491a73a2e7f74e53e80/unofficial-sqlite3-targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/unofficial-sqlite3/unofficial-sqlite3-targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/unofficial-sqlite3/unofficial-sqlite3-targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/unofficial-sqlite3" TYPE FILE FILES "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/x64-windows-rel/CMakeFiles/Export/2ca667fbbca39491a73a2e7f74e53e80/unofficial-sqlite3-targets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/unofficial-sqlite3" TYPE FILE FILES "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/x64-windows-rel/CMakeFiles/Export/2ca667fbbca39491a73a2e7f74e53e80/unofficial-sqlite3-targets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/pkgs/sqlite3_x64-windows/lib/pkgconfig/sqlite3.pc")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/pkgs/sqlite3_x64-windows/lib/pkgconfig" TYPE FILE FILES "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/x64-windows-rel/sqlite3.pc")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/x64-windows-rel/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/mague/source/repos/RelationalNT/vcpkg_installed/vcpkg/blds/sqlite3/x64-windows-rel/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
