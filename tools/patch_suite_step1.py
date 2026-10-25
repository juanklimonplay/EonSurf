# Script to integrate remaining 30 items suite into EonSurf PS3 codebase
import os

repo_root = r"J:\EonPS3\EonSurf\repo"
src_dir = os.path.join(repo_root, "src", "source")

# 1. Patch eonsurf_net_bypass.h for file:// protocol support
bypass_path = os.path.join(src_dir, "eonsurf_net_bypass.h")
with open(bypass_path, "r", encoding="utf-8") as f:
    bypass_content = f.read()

old_bypass_str = 'strncmp(user_input, "https://", 8) == 0 ||'
new_bypass_str = 'strncmp(user_input, "https://", 8) == 0 ||\n        strncmp(user_input, "file://", 7) == 0 ||'
if old_bypass_str in bypass_content and 'file://' not in bypass_content:
    bypass_content = bypass_content.replace(old_bypass_str, new_bypass_str, 1)
    with open(bypass_path, "w", encoding="utf-8") as f:
        f.write(bypass_content)
    print("Patched eonsurf_net_bypass.h with file:// support")
else:
    print("eonsurf_net_bypass.h already has file:// support or target not found")

# 2. Patch eonsurf_mockup_details.h to draw beautiful glowing ES logo instead of plain text
details_path = os.path.join(src_dir, "eonsurf_mockup_details.h")
with open(details_path, "r", encoding="utf-8") as f:
    details_content = f.read()

# Let's inspect eonsurf_draw_hex_es_emblem in details_content and make sure it has 'E' and 'S' dynamic logo styling
# Let's replace eonsurf_draw_hex_es_emblem with a sleek futuristic dual-glyph logo with reactive glow:
old_hex_func_start = "static inline void eonsurf_draw_hex_es_emblem(uint32_t *fb, int pitch, int cx, int cy, int is_hover) {"
new_hex_func = """static inline void eonsurf_draw_hex_es_emblem(uint32_t *fb, int pitch, int cx, int cy, int is_hover) {
    /* Emblema Vectorial Procedural Sci-Fi Eon Core con Reactor Luminoso */
    int r = 21;
    uint32_t ring_col  = is_hover ? 0x00FFFF : 0x38BDF8;
    uint32_t glow_col  = is_hover ? 0x00E5FF : 0x0284C7;
    uint32_t core_col  = is_hover ? 0x0A263D : 0x071524;
    uint32_t spark_col = is_hover ? 0xFFFFFF : 0x67E8F9;
    uint32_t accent_col = is_hover ? 0x38BDF8 : 0x00E5FF;

    for (int dy = -r - 4; dy <= r + 4; dy++) {
        for (int dx = -r - 4; dx <= r + 4; dx++) {
            float dist = sqrtf((float)(dx * dx + dy * dy));

            /* Aura difusa / Glow pulsante */
            if (dist > (float)r && dist <= (float)(r + 4)) {
                float t = (float)(r + 4 - dist) / 4.0f;
                int alpha = (int)(t * (is_hover ? 200.0f : 90.0f));
                mk_blend(fb, pitch, cx + dx, cy + dy, glow_col, alpha);
            }
            /* Anillo exterior cromado Sci-Fi */
            else if (dist <= (float)r && dist >= (float)(r - 2.5f)) {
                mk_blend(fb, pitch, cx + dx, cy + dy, ring_col, 255);
            }
            /* Cavidad interna Dark Glass */
            else if (dist < (float)(r - 2.5f)) {
                mk_blend(fb, pitch, cx + dx, cy + dy, core_col, 230);
            }
        }
    }

    /* Letra 'E' estilizada a la izquierda (cx - 9 a cx - 1) */
    for (int dy = -9; dy <= 9; dy++) {
        /* Columna vertical E */
        for (int dx = -9; dx <= -7; dx++) mk_blend(fb, pitch, cx + dx, cy + dy, spark_col, 255);
        /* Barra superior E */
        if (dy >= -9 && dy <= -7) {
            for (int dx = -7; dx <= -1; dx++) mk_blend(fb, pitch, cx + dx, cy + dy, spark_col, 255);
        }
        /* Barra media E con tono acento */
        if (dy >= -1 && dy <= 1) {
            for (int dx = -7; dx <= -3; dx++) mk_blend(fb, pitch, cx + dx, cy + dy, accent_col, 255);
        }
        /* Barra inferior E */
        if (dy >= 7 && dy <= 9) {
            for (int dx = -7; dx <= -1; dx++) mk_blend(fb, pitch, cx + dx, cy + dy, spark_col, 255);
        }
    }

    /* Letra 'S' estilizada a la derecha (cx + 2 a cx + 10) */
    for (int dy = -9; dy <= 9; dy++) {
        /* Barra superior S */
        if (dy >= -9 && dy <= -7) {
            for (int dx = 2; dx <= 9; dx++) mk_blend(fb, pitch, cx + dx, cy + dy, spark_col, 255);
        }
        /* Columna superior izquierda S */
        if (dy >= -7 && dy <= -1) {
            for (int dx = 2; dx <= 4; dx++) mk_blend(fb, pitch, cx + dx, cy + dy, spark_col, 255);
        }
        /* Barra media S */
        if (dy >= -1 && dy <= 1) {
            for (int dx = 2; dx <= 9; dx++) mk_blend(fb, pitch, cx + dx, cy + dy, accent_col, 255);
        }
        /* Columna inferior derecha S */
        if (dy >= 1 && dy <= 7) {
            for (int dx = 7; dx <= 9; dx++) mk_blend(fb, pitch, cx + dx, cy + dy, spark_col, 255);
        }
        /* Barra inferior S */
        if (dy >= 7 && dy <= 9) {
            for (int dx = 2; dx <= 9; dx++) mk_blend(fb, pitch, cx + dx, cy + dy, spark_col, 255);
        }
    }
}"""

if old_hex_func_start in details_content:
    end_marker = "/* Emblema PlayStation retrocompatible si se requiere */"
    p1 = details_content.find(old_hex_func_start)
    p2 = details_content.find(end_marker, p1)
    if p1 != -1 and p2 != -1:
        details_content = details_content[:p1] + new_hex_func + "\n\n" + details_content[p2:]
        with open(details_path, "w", encoding="utf-8") as f:
            f.write(details_content)
        print("Updated eonsurf_mockup_details.h with dual E+S sleek glowing emblem")

print("Done step 1 and 2.")
