section .rodata

global svg_data_computer
global svg_data_computer_end
global svg_data_folder
global svg_data_folder_end
global svg_data_gear
global svg_data_gear_end
global svg_data_task
global svg_data_task_end
global svg_data_file_normal
global svg_data_file_normal_end
global svg_data_file_document
global svg_data_file_document_end

align 4
svg_data_computer:
incbin "assets/computer.svg"
db 0
svg_data_computer_end:

align 4
svg_data_folder:
incbin "assets/folder.svg"
db 0
svg_data_folder_end:

align 4
svg_data_gear:
incbin "assets/gear.svg"
db 0
svg_data_gear_end:

align 4
svg_data_task:
incbin "assets/task.svg"
db 0
svg_data_task_end:

align 4
svg_data_file_normal:
incbin "assets/file_normal.svg"
db 0
svg_data_file_normal_end:

align 4
svg_data_file_document:
incbin "assets/file_document.svg"
db 0
svg_data_file_document_end:

section .rodata

global lc_data_en
global lc_data_en_end
global lc_data_zh
global lc_data_zh_end

align 4
lc_data_en:
incbin "assets/locale_en.kv"
db 0
lc_data_en_end:

align 4
lc_data_zh:
incbin "assets/locale_zh.kv"
db 0
lc_data_zh_end:
