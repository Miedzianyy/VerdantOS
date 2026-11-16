# VerdantOS Programmer's Handbook
**Note2Self**: Currently, GUI *Was planned to be supported, but it* doesn't work.

## Hi! Meet the basics.
If you are reading this "handbook", you probably tought on making VerdantOS Software, didn't you?
If yes, this book is for you. 

### Basic requirements
You can write in C? Forget about it. You'll need to know one of **these** assemblers:
- YASM, 
- NASM,
- or Flat Assembly (FASM)
AND EVEN if you know how to make binaries with one of them, there are NO:
- ELF Files, 
- PE Files, 
- Standarized Runtime,
- Etc. 
Programs on VerdantOS are PURE BINARY CODE that is loaded DIRECTLY into your RAM.

### The Position-Independent Code (PIC) Trap
Because your binary is loaded dynamically by the CLI from the FAT16 filesystem into an arbitrary RAM location, **absolute addresses for labels will break**.
If you reference a string like `msg: db "Hello", 0`, the assembler calculates its offset from the start of the file (e.g., `0x1A`), not its physical address in RAM.
### Solution:
### The get_eip trick.
To find out where your code actually landed in memory, use this standard position-independent bootstrap at the start of your program:
YASM/NASM:
```
[bits 32]

start:
    ; 1. Dynamically calculate the load offset in RAM
    call get_eip
get_eip:
    pop ecx                 ; ECX = current address of 'get_eip' in RAM
    sub ecx, get_eip        ; ECX = base load offset difference
    
    ; 2. Adjust your string/data pointers using the base offset
    mov edx, ecx
    add edx, msg            ; EDX now holds the TRUE physical RAM address of 'msg'
    mov esi, edx            ; Ready to use!
    ```
    FASM:
    ```
    format binary
use32

start:
    ; 1. Dynamiczne obliczenie adresu bazowego w RAM-ie
    call get_eip
get_eip:
    pop ecx                 ; ECX = aktualny adres etykiety get_eip w pamięci
    sub ecx, get_eip        ; ECX = różnica (bazowy adres załadowania)
    
    ; 2. Przeliczenie wskaźnika do danych
    mov edx, ecx
    add edx, msg            ; EDX = prawdziwy fizyczny adres 'msg' w RAM-ie
    mov esi, edx            ; Przekazujemy do esi

    mov edi, 0xB8000        ; Adres bufora VGA

print_loop:
    mov al, [esi]
    test al, al
    jz hang
    mov ah, 0x0F            ; Biały tekst na czarnym tle
    mov [edi], ax
    inc esi
    add edi, 2
    jmp print_loop

hang:
    cli
    jmp $

msg: db 'FASM BINARIES WORK TOO!', 0
``` 
### VGA TEXT BUFFER
Since you don't have a standard output stream, you write directly to the hardware video memory located at 0xB8000.
- **Screen Layout**: 80x25
- **CELL FORMAT**:
Each Screen Cell is 2 Bytes.
Bit 1 is The ASCII Character, Bit 2 is Color Value (For example: `0x0F` is White text on black background)
- **MATH**: To move to the next character on the screen, increment your video pointer (EDI) by **2**.
### SCREEN CLEANING
YASM/NASM:
```
[bits 32]

clear_screen:
    mov edi, 0xB8000        ; Set EDI to the start of the VGA text mode buffer
    mov ecx, 2000           ; Number of cells on a standard 80x25 screen (80 * 25 = 2000)
    mov ax, 0x0720          ; AH = Attribute/Color (0x07 = light gray text on black background)
                            ; AL = ASCII code for space character (0x20)
    rep stosw               ; Fast string fill: copies AX to [EDI], decrements ECX, advances EDI by 2
    ret                     ; Return from procedure
```
FASM:
```
format binary
use32

clear_screen:
    mov edi, 0xB8000        ; Set EDI to the start of the VGA text mode buffer
    mov ecx, 2000           ; Number of cells on a standard 80x25 screen (80 * 25 = 2000)
    mov ax, 0x0720          ; AH = Attribute/Color (0x07 = light gray text on black background)
                            ; AL = ASCII code for space character (0x20)
    rep stosw               ; Fast string fill: copies AX to [EDI], decrements ECX, advances EDI by 2
    ret                     ; Return from procedure
```
### EXAMPLE VERDANTOS PROGRAM
```
[bits 32]

start:
    ; Position-independent base resolution
    call get_eip
get_eip:
    pop ecx
    sub ecx, get_eip
    
    mov edx, ecx
    add edx, msg            ; Fix string pointer
    mov esi, edx

    mov edi, 0xB8000        ; VGA text buffer base address

print_loop:
    mov al, [esi]           ; Load character
    test al, al             ; Check for null-terminator (0)
    jz hang
    
    mov ah, 0x0F            ; Attribute: White on Black
    mov [edi], ax           ; Write character + attribute to screen
    
    inc esi                 ; Next source character
    add edi, 2              ; Next screen cell (2 bytes per cell)
    jmp print_loop

hang:
    cli
    jmp $                   ; Infinite loop (prevents CLI from overwriting screen)

msg: db 'VERDANTOS YASM BINARY ACTIVE!', 0
```
### Compiling and Running
MAKE SURE THAT YOU HAVE ALREADY DONE `make setfs` BECAUSE YOU'LL NEED A HDA
You need to compile your program to a RAW binary
`yasm -f bin test.asm -o test.bin`

And then, move it to the disk
`mcopy -i dysk.img <your_file> ::/<path>/<program_name>.BIN`

Then, simply run Your binary in the VerdantOS CLI. For Example `TEXTEDIT.BIN` 

## Questions you might ask
When Binaries for GUI?

Version 1.2.0 i guess

When LibC?

I don't know

## GOODBYE!
