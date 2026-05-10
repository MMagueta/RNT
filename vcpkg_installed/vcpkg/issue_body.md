Package: sqlite3[core,json1]:x64-windows@3.53.1#1

**Host Environment**

- Host: x64-windows
- Compiler: MSVC 19.51.36223.2
- CMake Version: 3.31.10
-    vcpkg-tool version: 2026-02-21-82f3841f4f91e8389b7c7ddf61e6733683b2f67b
    vcpkg-readonly: true
    vcpkg-scripts version: e5a1490e1409d175932ef6014519e9ae149ddb7c

**To Reproduce**

`vcpkg install `

**Failure logs**

```
Downloading https://sqlite.org/2026/sqlite-autoconf-3530100.tar.gz -> sqlite-autoconf-3530100.tar.gz
Successfully downloaded sqlite-autoconf-3530100.tar.gz
-- Extracting source C:/Users/mague/AppData/Local/vcpkg/downloads/sqlite-autoconf-3530100.tar.gz
-- Applying patch fix-arm-uwp.patch
-- Applying patch add-config-include.patch
CMake Error at scripts/cmake/z_vcpkg_apply_patches.cmake:34 (message):
  Applying patch failed: Checking patch sqlite3.c...

  Checking patch sqlite3.h...

  warning: unable to unlink 'sqlite3.c': Invalid argument

  error: unable to write file 'sqlite3.c' mode 100644: Permission denied

Call Stack (most recent call first):
  scripts/cmake/vcpkg_extract_source_archive.cmake:147 (z_vcpkg_apply_patches)
  C:/Users/mague/AppData/Local/vcpkg/registries/git-trees/2026ad457a41f41b28699ef66ee4f9431d4a86f1/portfile.cmake:10 (vcpkg_extract_source_archive)
  scripts/ports.cmake:206 (include)



```

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "builtin-baseline": "99a97de2cb371449d4fb9dc970f2ac562d689ec2",
  "dependencies": [
    "sqlite3",
    "picosha2"
  ]
}

```
</details>
