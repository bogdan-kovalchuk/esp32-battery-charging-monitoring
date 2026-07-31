"""Let `pio test -e native` work on Windows without a system-wide compiler.

PlatformIO already ships a MinGW toolchain (`toolchain-gccmingw32`) for its own
debug tooling, but the `native` platform only looks for gcc on PATH. Without
this, the host tests fail with "'gcc' is not recognized" on an otherwise
perfectly capable machine.

If a compiler is already on PATH, this does nothing.
"""

import os
import platform
import shutil

Import("env")  # noqa: F821 - injected by PlatformIO


def _bundled_mingw_bin() -> str | None:
    core_dir = env.subst("$PROJECT_CORE_DIR")  # noqa: F821
    candidate = os.path.join(core_dir, "packages", "toolchain-gccmingw32", "bin")
    return candidate if os.path.isfile(os.path.join(candidate, "gcc.exe")) else None


if platform.system() == "Windows" and shutil.which("gcc") is None:
    mingw_bin = _bundled_mingw_bin()
    if mingw_bin:
        env.PrependENVPath("PATH", mingw_bin)  # noqa: F821
        # The bundled toolchain's libgcc/libstdc++/libwinpthread DLLs are not
        # on the system PATH, so a dynamically linked test binary starts and
        # immediately dies with 0xC0000135 (DLL not found). Link them in.
        env.Append(  # noqa: F821
            LINKFLAGS=["-static", "-static-libgcc", "-static-libstdc++"]
        )
        print(f"native: using PlatformIO's bundled MinGW at {mingw_bin}")
    else:
        print(
            "native: no gcc on PATH and no bundled MinGW found. Install MinGW-w64 "
            "or MSVC, or run `pio pkg install -g -t toolchain-gccmingw32`."
        )
