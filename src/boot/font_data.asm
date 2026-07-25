section .rodata
align 16
global unifont_glyphs
unifont_glyphs:
    incbin "build/unifont_glyphs.bin"
align 16
global unifont_widths
unifont_widths:
    incbin "build/unifont_widths.bin"
