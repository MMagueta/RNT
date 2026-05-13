{
  description = "RelationalNT CMake build";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    picosha2 = {
      url = "github:okdshin/PicoSHA2";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, picosha2 }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      forAllSystems = nixpkgs.lib.genAttrs systems;
    in
    {
      packages = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };

          unofficialSqlite3Config = pkgs.writeTextDir
            "share/unofficial-sqlite3/unofficial-sqlite3-config.cmake"
            ''
              include(CMakeFindDependencyMacro)
              find_dependency(SQLite3 REQUIRED)

              if(NOT TARGET unofficial::sqlite3::sqlite3)
                add_library(unofficial::sqlite3::sqlite3 INTERFACE IMPORTED)
                target_link_libraries(unofficial::sqlite3::sqlite3 INTERFACE SQLite::SQLite3)
              endif()
            '';

          picosha2Headers = pkgs.runCommand "picosha2-headers" { } ''
            install -Dm644 ${picosha2}/picosha2.h $out/include/picosha2.h
          '';
        in
        {
          default = pkgs.stdenv.mkDerivation {
            pname = "relationalnt";
            version = "0.1.0";

            src = self;

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.ninja
            ];

            buildInputs = [
              pkgs.catch2_3
              picosha2Headers
              pkgs.sqlite
              unofficialSqlite3Config
            ];

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
            ];

            doCheck = true;
            checkPhase = ''
              runHook preCheck
              ctest --output-on-failure
              runHook postCheck
            '';

            installPhase = ''
              runHook preInstall
              install -Dm755 RelationalNT "$out/bin/RelationalNT"
              install -Dm755 RNT_tests "$out/bin/RNT_tests"
              for lib in libRNT.so libRNT.dylib; do
                if [ -f "$lib" ]; then
                  install -Dm755 "$lib" "$out/lib/$lib"
                fi
              done
              install -Dm644 ../include/RNT_C_API.h "$out/include/RNT_C_API.h"
              runHook postInstall
            '';

            meta = {
              description = "Experimental database kernel inspired by the Windows NT object manager";
              mainProgram = "RelationalNT";
            };
          };
        });

      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };

          unofficialSqlite3Config = pkgs.writeTextDir
            "share/unofficial-sqlite3/unofficial-sqlite3-config.cmake"
            ''
              include(CMakeFindDependencyMacro)
              find_dependency(SQLite3 REQUIRED)

              if(NOT TARGET unofficial::sqlite3::sqlite3)
                add_library(unofficial::sqlite3::sqlite3 INTERFACE IMPORTED)
                target_link_libraries(unofficial::sqlite3::sqlite3 INTERFACE SQLite::SQLite3)
              endif()
            '';

          picosha2Headers = pkgs.runCommand "picosha2-headers" { } ''
            install -Dm644 ${picosha2}/picosha2.h $out/include/picosha2.h
          '';

          clangdRnt = pkgs.writeShellScriptBin "clangd-rnt" ''
            exec ${pkgs.clang-tools}/bin/clangd \
              --query-driver='${pkgs.stdenv.cc}/bin/*,/usr/bin/clang,/usr/bin/clang++' \
              "$@"
          '';
        in
        {
          default = pkgs.mkShell {
            packages = [
              pkgs.catch2_3
              pkgs.clang-tools
              pkgs.cmake
              pkgs.lldb
              pkgs.ninja
              clangdRnt
              picosha2Headers
              pkgs.sqlite
              unofficialSqlite3Config
            ];

            CC = "${pkgs.stdenv.cc}/bin/cc";
            CXX = "${pkgs.stdenv.cc}/bin/c++";

            CMAKE_PREFIX_PATH = pkgs.lib.makeSearchPathOutput "dev" "" [
              picosha2Headers
              pkgs.sqlite
              unofficialSqlite3Config
            ];

            CMAKE_INCLUDE_PATH = pkgs.lib.makeSearchPathOutput "dev" "include" [
              picosha2Headers
              pkgs.sqlite
            ];

            CMAKE_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
              pkgs.sqlite
            ];

            CPATH = pkgs.lib.makeSearchPathOutput "dev" "include" [
              picosha2Headers
              pkgs.sqlite
            ];

            C_INCLUDE_PATH = pkgs.lib.makeSearchPathOutput "dev" "include" [
              pkgs.sqlite
            ];

            CPLUS_INCLUDE_PATH = pkgs.lib.makeSearchPathOutput "dev" "include" [
              picosha2Headers
              pkgs.sqlite
            ];

            LIBRARY_PATH = pkgs.lib.makeLibraryPath [
              pkgs.sqlite
            ];

            DYLD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
              pkgs.sqlite
            ];

            PKG_CONFIG_PATH = pkgs.lib.makeSearchPathOutput "dev" "lib/pkgconfig" [
              pkgs.sqlite
            ];

            shellHook = ''
              export NIX_CFLAGS_COMPILE="-I${picosha2Headers}/include -I${pkgs.sqlite.dev}/include $NIX_CFLAGS_COMPILE"
              export NIX_LDFLAGS="-L${pkgs.sqlite.out}/lib $NIX_LDFLAGS"
            '';
          };
        });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/RelationalNT";
        };
      });
    };
}
