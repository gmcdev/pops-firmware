"""
PlatformIO extra script — reads .env.local from the project root and injects
each key as a preprocessor define so source files can reference them via macros.

Uses SCons CPPDEFINES (not BUILD_FLAGS) so the injection takes effect before
compilation nodes are created.
"""
import os
import sys

Import("env")  # noqa: F821 — injected by PlatformIO


def load_env_file(env_path):
    if not os.path.isfile(env_path):
        print(f"[load_env] No .env.local found at {env_path} — skipping", file=sys.stderr)
        return

    print(f"[load_env] Loading {env_path}", file=sys.stderr)

    with open(env_path) as env_file:
        for line in env_file:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip()
            value = value.strip().strip('"').strip("'")
            env.Append(CPPDEFINES=[(key, f'\\"{value}\\"')])  # noqa: F821
            print(f"[load_env] Injected {key}", file=sys.stderr)


project_dir = env.subst("$PROJECT_DIR")  # noqa: F821
load_env_file(os.path.join(project_dir, ".env.local"))
