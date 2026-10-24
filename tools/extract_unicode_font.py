import re

with open('EonSurf/repo/port/work/netsurf-all-3.11/netsurf/frontends/framebuffer/res/fonts/glyph_data', 'r', encoding='latin1') as f:
    content = f.read()

# Buscamos cada sección U+XXXX
sections = re.split(r'U\+([0-9A-Fa-f]{4})', content)

glyphs = {}
for i in range(1, len(sections), 2):
    cp = int(sections[i], 16)
    sec_text = sections[i+1]
    
    # Extraer las 16 líneas del estilo Regular
    # El formato tiene líneas con '.' y '#'
    grid_lines = []
    for line in sec_text.splitlines():
        line = line.strip()
        if re.match(r'^[.# ]{8,}', line):
            # Tomamos los primeros 8 caracteres (columna Regular)
            row_str = line[:8]
            val = 0
            for col_idx, ch in enumerate(row_str):
                if ch == '#':
                    val |= (1 << (7 - col_idx))
            grid_lines.append(val)
            if len(grid_lines) == 16:
                break
    
    if len(grid_lines) == 16:
        glyphs[cp] = grid_lines

print(f"Extracted {len(glyphs)} full 16-row glyphs!")

# Generamos tabla C para ASCII (0..127) y Latin-1 Extendido (160..255)
out_c = """/* EonSurf HD 8x16 Unicode Font (ASCII + Latin-1 Accents: ñ, á, é, í, ó, ú, ¿, ¡)
 * Extraído directamente del Zap Font de NetSurf 3.11 */
#ifndef EONSURF_FONT8X16_H
#define EONSURF_FONT8X16_H

#include <stdint.h>

/* Tabla ASCII base 0..127 */
static const uint8_t font8x16_ascii[128][16] = {
"""

for cp in range(128):
    rows = glyphs.get(cp, [0]*16)
    row_bytes = ', '.join(f"0x{r:02x}" for r in rows)
    out_c += f"    /* {cp:3d} */ {{ {row_bytes} }},\n"

out_c += """};

/* Tabla Latin-1 Suplementario 160..255 (á, é, í, ó, ú, ñ, Á, É, Í, Ó, Ú, Ñ, etc.) */
static const uint8_t font8x16_latin1[96][16] = {
"""

for cp in range(160, 256):
    rows = glyphs.get(cp, [0]*16)
    row_bytes = ', '.join(f"0x{r:02x}" for r in rows)
    out_c += f"    /* {cp:3d} (0x{cp:02x}) */ {{ {row_bytes} }},\n"

out_c += """};

/* Retorna el puntero a los 16 bytes del glifo para cualquier codepoint Unicode */
static inline const uint8_t* font8x16_get_glyph(uint32_t cp) {
    if (cp < 128) {
        return font8x16_ascii[cp];
    }
    if (cp >= 160 && cp <= 255) {
        return font8x16_latin1[cp - 160];
    }
    /* Fallback a signo de interrogación '?' */
    return font8x16_ascii['?'];
}

#endif /* EONSURF_FONT8X16_H */
"""

with open('EonSurf/repo/src/source/font8x16.h', 'w', encoding='utf-8') as f:
    f.write(out_c)

print("Generated full Unicode font8x16.h with tildes and Ñ!")
