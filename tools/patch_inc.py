with open('EonSurf/repo/src/source/main.c', 'r', encoding='utf-8') as f:
    text = f.read()

target = '#include "eonsurf_fsm.h"'
replacement = '''#include "eonsurf_fsm.h"
#include "eonsurf_layout.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"'''

if target in text:
    text = text.replace(target, replacement, 1)
    with open('EonSurf/repo/src/source/main.c', 'w', encoding='utf-8') as f:
        f.write(text)
    print("Includes added successfully")
else:
    print("Target not found")
