with open('EonSurf/repo/src/source/main.c', 'r', encoding='utf-8') as f:
    text = f.read()

target = 'init_screen();'
replacement = '''init_screen();
    init_default_layout();'''

if target in text:
    text = text.replace(target, replacement, 1)
    with open('EonSurf/repo/src/source/main.c', 'w', encoding='utf-8') as f:
        f.write(text)
    print("init_default_layout called in main")
else:
    print("init_screen not found")
