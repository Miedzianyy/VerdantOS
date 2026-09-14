from PIL import Image
import os

def convert_icon(filename, array_name):
    # Szukamy pliku w folderze assets (który jest w głównym katalogu)
    filepath = os.path.join("assets", filename)
    
    if not os.path.exists(filepath):
        print(f"[OSTRZEŻENIE] Nie znaleziono pliku: {filepath}. Tworzę pustą ikonę.")
        return f"const uint32_t {array_name}[64 * 64] = {{0}};\n\n"
    
    # Otwieramy i skalujemy do 64x64
    img = Image.open(filepath).resize((64, 64)).convert("RGBA")
    pixels = img.load()
    w, h = img.size
    
    code = f"const uint32_t {array_name}[{w} * {h}] = {{\n"
    for y in range(h):
        code += "    "
        for x in range(w):
            r, g, b, a = pixels[x, y]
            # Kolor przezroczysty (różowy klucz)
            if a < 128:
                hex_val = 0x00FF00FF
            else:
                hex_val = (r << 16) | (g << 8) | b
            code += f"0x00{hex_val:06X}, "
        code += "\n"
    code += "};\n\n"
    return code

# Upewniamy się, że folder gui/ istnieje, żeby Python sam go nie zgubił
os.makedirs("gui", exist_ok=True)

# Zapisujemy plik ZAWSZE bezpośrednio w folderze gui/icons.h
output_path = os.path.join("gui", "icons.h")

with open(output_path, "w") as f:
    f.write("#ifndef ICONS_H\n#define ICONS_H\n\n#include <stdint.h>\n\n")
    f.write(convert_icon("file.png", "icon_file_data"))
    f.write(convert_icon("prog.png", "icon_prog_data"))
    f.write("#endif\n")

print("Sukces! Plik icons.h został wygenerowany bezpośrednio w folderze gui/.")