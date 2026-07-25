#!/bin/bash
set -e

ROOT="$(cygpath -m "$(cd "$(dirname "$0")" && pwd)")"
cd "$ROOT"

BUILD="$ROOT/build"
ISODIR="$BUILD/isodir"
CLANG="/c/Program Files/LLVM/bin/clang.exe"
LLD="/c/Program Files/LLVM/bin/ld.lld.exe"
OBJCOPY="/c/Program Files/LLVM/bin/llvm-objcopy.exe"
NASM="nasm"
PYTHON="C:/Users/bl/.workbuddy/binaries/python/versions/3.13.12/python.exe"
GRUB_DIR="$ROOT/tools/grub/grub-2.06-for-windows"
GRUB_I386="$GRUB_DIR/i386-pc"
XORRISO="$(cygpath -u "$ROOT/tools/xorriso/xorriso.exe")"

towin() { echo "$1" | sed 's|^\([A-Za-z]\):|/cygdrive/\L\1|'; }

CFLAGS="--target=x86_64-unknown-none-elf -ffreestanding -fno-stack-protector \
    -fno-builtin -nostdlib -mno-red-zone \
    -mcmodel=kernel -O2 -Wall -Wextra -std=c11 \
    -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable \
    -Wno-sign-compare -Wno-missing-field-initializers"

rm -rf "$ISODIR"
mkdir -p "$BUILD" "$ISODIR/boot/grub/i386-pc"

if [ ! -f "$BUILD/unifont_glyphs.bin" ] || [ ! -f "$BUILD/unifont_widths.bin" ]; then
    "$PYTHON" "$ROOT/tools/mkfont.py" "$ROOT/tools/dl/unifont.hex" \
        "$BUILD/unifont_glyphs.bin" "$BUILD/unifont_widths.bin"
fi

RES_W=$(head -1 "$ROOT/config/boot_resolution.cfg" | tr -d '\r')
RES_H=$(head -2 "$ROOT/config/boot_resolution.cfg" | tail -1 | tr -d '\r')
RES_D=$(tail -1 "$ROOT/config/boot_resolution.cfg" | tr -d '\r')

"$NASM" -f elf64 "$ROOT/src/boot/boot.asm" -o "$BUILD/boot.o" \
    -DBOOT_RES_W="$RES_W" -DBOOT_RES_H="$RES_H" -DBOOT_RES_D="$RES_D"
"$NASM" -f elf64 "$ROOT/src/boot/font_data.asm" -o "$BUILD/font_data.o"
"$NASM" -f elf64 "$ROOT/src/boot/isr.asm" -o "$BUILD/isr.o"
"$NASM" -f elf64 "$ROOT/src/boot/assets.asm" -o "$BUILD/assets.o"
"$NASM" -f elf64 "$ROOT/src/boot/user_blob.asm" -o "$BUILD/user_blob.o"

USER_CFLAGS="--target=x86_64-unknown-none-elf -ffreestanding -fno-stack-protector \
    -fno-builtin -nostdlib -mno-red-zone -mno-sse -mno-sse2 -mno-sse3 -mno-ssse3 -mno-sse4 \
    -mcmodel=large -O2 -Wall -Wextra -std=c11 \
    -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable \
    -Wno-sign-compare -Wno-missing-field-initializers \
    -Isrc/user"

CFILES="kmain serial console kprintf font fbdump util gdt idt pic pit keyboard panic pmm vmm kheap sched syscall ata fat32 vfs pci svga mouse gfx ui wm tooltip desktop svg math_impl locale rtc guisys udll"
OBJS="$BUILD/boot.o $BUILD/font_data.o $BUILD/isr.o $BUILD/assets.o $BUILD/user_blob.o"

echo "--- building user apps ---"
for app in calculator notepad terminal clock; do
    case $app in
        calculator) APP_BASE=0x8000000000 ;;
        notepad)    APP_BASE=0x8000010000 ;;
        terminal)   APP_BASE=0x8000020000 ;;
        clock)      APP_BASE=0x8000030000 ;;
    esac
    "$CLANG" $USER_CFLAGS -c "$ROOT/src/apps/$app/bin/main.c" -o "$BUILD/${app}_u.o"
    "$LLD" --image-base=$APP_BASE -T "$ROOT/src/user/linker.ld" -o "$BUILD/$app.elf" -nostdlib "$BUILD/${app}_u.o"
    "$OBJCOPY" -O binary "$BUILD/$app.elf" "$BUILD/$app.bin"
    echo "  $app.bin  ($(stat -c%s "$BUILD/$app.bin" 2>/dev/null || wc -c < "$BUILD/$app.bin") bytes)"
    "$PYTHON" "$ROOT/tools/upk_pack.py" "$ROOT/src/apps/$app" "$BUILD/$app.bin" "$BUILD/$app.upk"
done

"$NASM" -f elf64 "$ROOT/src/boot/user_apps.asm" -o "$BUILD/user_apps.o"
OBJS="$OBJS $BUILD/user_apps.o"
echo "--- user apps done ---"

echo "--- building UDLLs ---"
UDLL_CFLAGS="--target=x86_64-unknown-none-elf -ffreestanding -fno-stack-protector \
    -fno-builtin -nostdlib -mno-red-zone \
    -mcmodel=large -O2 -Wall -Wextra -std=c11 \
    -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable \
    -Wno-sign-compare -Wno-missing-field-initializers \
    -Isrc/kernel"
"$CLANG" $UDLL_CFLAGS -c "$ROOT/src/udll/filedlg/filedlg.c" -o "$BUILD/filedlg.o"
"$PYTHON" "$ROOT/tools/udll_pack.py" "$BUILD/filedlg.o" "$BUILD/filedlg.udll"
echo "  filedlg.udll ready"
echo "--- UDLLs done ---"

for c in $CFILES; do
    "$CLANG" $CFLAGS -c "$ROOT/src/kernel/$c.c" -o "$BUILD/$c.o"
    OBJS="$OBJS $BUILD/$c.o"
done

"$LLD" -T "$ROOT/linker.ld" -o "$BUILD/kernel.elf" -nostdlib $OBJS

cp "$BUILD/kernel.elf" "$ISODIR/boot/kernel.elf"
cp "$ROOT/src/grub.cfg" "$ISODIR/boot/grub/grub.cfg"
cp "$GRUB_I386"/*.mod "$ISODIR/boot/grub/i386-pc/"
cp "$GRUB_I386"/*.lst "$ISODIR/boot/grub/i386-pc/" 2>/dev/null || true

"$GRUB_DIR/grub-mkimage.exe" \
    -O i386-pc-eltorito \
    -d "$GRUB_I386" \
    -p /boot/grub \
    -o "$ISODIR/boot/grub/i386-pc/eltorito.img" \
    biosdisk iso9660 multiboot2 normal configfile part_msdos ls cat echo boot all_video

"$XORRISO" -as mkisofs \
    -R -J -V UNIOS \
    -b boot/grub/i386-pc/eltorito.img \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    -o "$(towin "$BUILD/UniOS.iso")" \
    "$(towin "$ISODIR")"

echo "ISO ready: $BUILD/UniOS.iso"
