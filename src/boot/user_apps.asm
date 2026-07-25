section .rodata

global upk_calculator_start
global upk_calculator_end
global upk_notepad_start
global upk_notepad_end
global upk_terminal_start
global upk_terminal_end
global upk_clock_start
global upk_clock_end

global upk_calculator_size
global upk_notepad_size
global upk_terminal_size
global upk_clock_size

align 4
upk_calculator_start:
incbin "build/calculator.bin"
upk_calculator_end:
upk_calculator_size dq upk_calculator_end - upk_calculator_start

align 4
upk_notepad_start:
incbin "build/notepad.bin"
upk_notepad_end:
upk_notepad_size dq upk_notepad_end - upk_notepad_start

align 4
upk_terminal_start:
incbin "build/terminal.bin"
upk_terminal_end:
upk_terminal_size dq upk_terminal_end - upk_terminal_start

align 4
upk_clock_start:
incbin "build/clock.bin"
upk_clock_end:
upk_clock_size dq upk_clock_end - upk_clock_start
