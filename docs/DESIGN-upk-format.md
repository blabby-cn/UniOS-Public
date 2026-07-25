# UniOS UPK Application Package Format

## Overview

UPK is the standard application distribution format for UniOS, analogous to `.apk` (Android), `.app` (macOS), or `.msi` (Windows). It is a self-contained archive holding an application's executables, resources, metadata, and dependencies in a single file.

## Container: USTAR

UPK uses the **USTAR** (POSIX.1-1988 tar) archive format as its container. USTAR is chosen because:
- Simple to parse (~512 bytes per header, no compression required)
- Well-defined standard, no patent issues
- Can be read with standard tools on any OS
- Kernel can implement a minimal USTAR reader in ~200 lines of C

A `.upk` file is a valid USTAR archive with the following internal structure:

```
myapp.upk
  manifest           # required: plain text key=value metadata
  bin/               # executable ELF64 files
    myapp
  res/               # resources
    icon.svg
    ...
  data/              # bundled data files
```

## Manifest Format

`manifest` is a plain text file using the same key=value format as UniOS locale files (`.kv`):

```
upk.format=1
upk.id=com.example.myapp
upk.name=My Application
upk.version=1.0.0
upk.arch=x86_64
upk.entry=bin/myapp
upk.exports=myapp,myapp-cli
upk.deps=
```

| Key | Required | Description |
|-----|----------|-------------|
| `upk.format` | Yes | UPK format version (currently 1) |
| `upk.id` | Yes | Reverse-domain application ID, also used as `/apps/<id>` directory |
| `upk.name` | Yes | Human-readable application name |
| `upk.version` | Yes | Semantic version (major.minor.patch) |
| `upk.arch` | Yes | Target architecture: `x86_64` |
| `upk.entry` | Yes | Path to ELF64 executable inside the archive |
| `upk.exports` | No | Comma-separated list of command names to expose in `/etc/pkg/exports` |
| `upk.deps` | No | Comma-separated list of required UPK IDs |

## Installation Layout

When installed (via `upk install <file.upk>`):

```
/apps/com.example.myapp/
  bin/
    myapp           -> ELF64 executable
  res/
    icon.svg
  manifest           -> copy of manifest
  .unimeta           -> owner/group/mode metadata
/etc/pkg/
  db/
    com.example.myapp  -> copy of manifest
  exports
    myapp=com.example.myapp
    myapp-cli=com.example.myapp
```

## /etc/pkg/exports

The exports file maps command names to application IDs:

```
myapp=com.example.myapp
myapp-cli=com.example.myapp
mycomp=com.example.mycomp
```

When the shell encounters an unknown command, it looks up `/etc/pkg/exports` to find the application ID, then executes `/apps/<id>/<entry>`.

## API Design

Applications communicate with the kernel through `int $0x80` syscalls (号 1–19).

### Already Implemented (syscall 1–19)

| 号 | Function | Purpose |
|----|----------|---------|
| 1–5 | `sys_write/read/yield/getpid/exit` | Basic OS primitives |
| 6–13,19 | `Uni_Window/Destroy/Widget/Poll/Show/Text/Label/GetText/SetText` | GUI window, widget & text API |
| 14–18 | `Uni_FOpen/FClose/FRead/FWrite/FList` | File I/O & directory listing |

All functions are callable from ring3 through inlined assembly wrappers in `src/user/Uni.h`. For complete API reference and code examples, see `docs/UniOS-UGC-DevGuide.md`.

### File Dialog

For kernel-space windows (system apps), a shared file dialog is available as a UDLL (`/lib/filedlg.udll`). It exports:

```c
void file_dialog_show(int mode, const char *title, const char *initial_path,
                      const char *ok_text, const char *cancel_text,
                      void (*cb)(int, const char *, int, void *), void *ctx);
```

For ring3 user applications, file I/O is provided through syscall 14–18 (`Uni_FOpen`/`Uni_FRead`/`Uni_FWrite`/`Uni_FClose`/`Uni_FList`).

For UDLL design details, see `docs/DESIGN-udll.md`.

### Future

- `mmap/munmap` — Memory mapping
- `fork/exec/exit/wait` — Process management
- `socket/bind/listen/accept` — Networking
- `ioctl` — Device/driver control

## Verification

The package manager (`upk`) performs these checks before installation:
1. Verify USTAR format integrity
2. Validate manifest required keys
3. Check architecture compatibility
4. Verify ELF64 binary header
5. Check dependency availability
6. Ensure no existing app with same ID (unless upgrading)
