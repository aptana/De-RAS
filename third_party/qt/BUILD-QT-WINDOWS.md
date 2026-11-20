# Qt 6 for Windows used by De-RAS

De-RAS links **Qt 6.4.2 (qtbase)** dynamically under the **GNU LGPL v3** (see `LGPL-3.0-only.txt`, which builds on `GPL-3.0-only.txt`).
The Qt DLLs shipped next to `De-RAS.exe` (Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll, platforms/qwindows.dll,
styles/qwindowsvistastyle.dll) can be replaced by the user with any compatible Qt 6.4 build.

## Source
- Qt source: Debian/Ubuntu source package `qt6-base 6.4.2+dfsg` (upstream `qtbase-everywhere-src-6.4.2`), available from
  https://download.qt.io/archive/qt/6.4/6.4.2/submodules/ and from the Ubuntu archive.
- Modification made by De-RAS: `qt6-base-6.4.2-mingw-no-opengl.patch` (one line; lets the Windows platform plugin compile with OpenGL disabled).

## How it was built (cross-compiled on Linux with MinGW-w64 GCC 13, posix threads)
```
cmake -G Ninja <qtbase-src> -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake -DQT_HOST_PATH=/usr \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF \
  -DFEATURE_opengl=OFF -DFEATURE_dbus=OFF -DFEATURE_network=OFF -DFEATURE_sql=OFF \
  -DFEATURE_tabletevent=OFF -DFEATURE_directwrite3=OFF -DFEATURE_zstd=OFF
ninja && ninja install
```
`mingw-toolchain.cmake` sets `CMAKE_SYSTEM_NAME=Windows` and the `x86_64-w64-mingw32-*-posix` compilers.

The MinGW runtime DLLs (libstdc++-6.dll, libgcc_s_seh-1.dll: GPL v3 with the GCC Runtime Library Exception;
libwinpthread-1.dll: MIT/BSD-style) come from the Ubuntu `gcc-mingw-w64` / `mingw-w64` packages.
