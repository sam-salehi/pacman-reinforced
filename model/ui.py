"""Import helper for the SFML front-end module `pacman` (../build/pacman*.so).

Conda Pythons ship an older libstdc++ than the system SFML libraries need. If importing
fails for that reason, re-exec the current script with the system libstdc++ preloaded.
"""
import glob
import os
import sys

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build"))


def import_pacman_ui():
    try:
        import pacman
        return pacman
    except ImportError as e:
        if "GLIBCXX" not in str(e) or os.environ.get("_PACMAN_UI_PRELOAD"):
            raise
        candidates = glob.glob("/usr/lib/x86_64-linux-gnu/libstdc++.so.6") + glob.glob("/usr/lib64/libstdc++.so.6")
        if not candidates:
            raise
        env = dict(os.environ, LD_PRELOAD=candidates[0], _PACMAN_UI_PRELOAD="1")
        os.execve(sys.executable, [sys.executable] + sys.argv, env)
