# UniOS

A hobby operating system written from scratch for x86_64. Kernel, drivers, window manager, and GUI toolkit are all built from the ground up — no third-party OS code.

## Architecture

- **Kernel**: C + NASM assembly, monolithic design
- **Boot**: Multiboot2 via GRUB
- **Memory**: 4-level paging (PML4), physical page-frame bitmap allocator, kernel heap with free-list
- **Filesystem**: FAT32 with VFS layer, synthetic UNIX-style permissions via `.unimeta`
- **Graphics**: VMware SVGA II driver with FIFO command acceleration, SVG vector icons via nanosvg
- **GUI**: Custom window manager (`wm`), widget toolkit (`UiWidget` / `UiButton` / `UiToggle` / `UiTextInput` / `UiWindow`), 14-preset anchor layout system
- **Dynamic linking**: UDLL (UniOS Dynamic Link Library) — loadable kernel-mode shared libraries
- **Applications**: Notepad, Terminal, File Manager, Calculator, Clock
- **Package format**: UPK (USTAR container + key-value manifest)

## Building

Requirements: LLVM/Clang, NASM, GRUB tools, xorriso (for ISO creation).

```bash
./build.sh
```

Output: `build/UniOS.iso`

Tested on VMware. Boot the ISO via GRUB with Multiboot2 support.

## Release

This repository contains the public source code for UniOS releases. Development happens in a private repository; only stripped release snapshots are published here.

## License

Source code in this repository is provided for reference and educational purposes.
