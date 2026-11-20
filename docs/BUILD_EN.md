# Building De-RAS

## Linux (development and tests)
```
sudo apt install cmake ninja-build g++ qt6-base-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build && ctest --test-dir build
```
Headless UI screenshots: `QT_QPA_PLATFORM=offscreen build/ui/deras-ui-snapshot <output-folder> <project.prj> id|en` (this also checks that opening a project does not change any file).

## Windows (cross-compile from Linux with MinGW-w64)
1. Build Qt 6.4 qtbase for Windows: see `third_party/qt/BUILD-QT-WINDOWS.md`.
2. Build De-RAS:
```
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=<mingw-toolchain.cmake> -DCMAKE_PREFIX_PATH=<qt6-win> -DQT_HOST_PATH=/usr
ninja -C build-win
```
3. Package and installer: `tools/package/make_windows_dist.sh build-win <qt6-win> <dist-folder>` produces a portable folder, a ZIP, and `De-RAS-<version>-setup-x64.exe` (NSIS).

The release label is set with `-DDERAS_RELEASE_LABEL=ALPHA` (shown on the splash screen, Home and About). The version comes from `project(DeRAS VERSION …)` in `CMakeLists.txt`.

## ⓘ help and the manual
Edit the text in `tools/help/help_source.py`, then run `python3 tools/help/build_help.py`. This produces `resources/help/help.json` (used by the application) and `docs/manual/help-topics.id.md` / `.en.md` (manual chapters) from the same source.
