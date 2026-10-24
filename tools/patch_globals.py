with open('EonSurf/repo/src/source/main.c', 'r', encoding='utf-8') as f:
    text = f.read()

# Buscamos la sección de variables globales
old_globals = '''static char current_url[512] = "https://es.wikipedia.org/wiki/PlayStation_3";
static char page_title[256] = "PlayStation 3 - Wikipedia, la enciclopedia libre";
static char page_body[8192] = "Bienvenido a EonSurf para PlayStation 3.\\nUsa los marcadores superiores para cargar páginas web completas en vivo.";
static int net_initialized = 0;

/* Desplazamiento y Scroll */
static int scroll_y = 0;
static int max_scroll_y = 0;

/* Enlaces extraidos dinamicamente de la pagina actual */
static eon_page_link_t page_links[EONSURF_MAX_LINKS];
static size_t page_links_count = 0;'''

new_globals = '''static char current_url[512] = "https://es.wikipedia.org/wiki/PlayStation_3";
static char page_title[256] = "PlayStation 3 - Wikipedia, la enciclopedia libre";
static char page_body[8192] = "PlayStation 3 (oficialmente abreviada como PS3) es la tercera videoconsola del modelo PlayStation de Sony Computer Entertainment.\\n\\nForma parte de las videoconsolas de séptima generación y compitió contra la Xbox 360 de Microsoft y la Wii de Nintendo. Su procesador central es el Cell Broadband Engine diseñado conjuntamente por Sony, Toshiba e IBM.";
static int net_initialized = 0;

/* Motor de Maquetación Estructurada 70/30 (eonsurf_layout.h) */
static EonPageLayout g_layout;

/* Desplazamiento y Scroll */
static int scroll_y = 0;
static int max_scroll_y = 0;

/* Enlaces extraidos dinamicamente de la pagina actual */
static eon_page_link_t page_links[EONSURF_MAX_LINKS];
static size_t page_links_count = 0;

/* Decodificación de imágenes vía stb_image para el RSX en Big-Endian */
static uint32_t* eonsurf_decode_image_argb(const uint8_t *buffer, int len, int *out_w, int *out_h) {
    int w, h, channels;
    uint8_t *raw = stbi_load_from_memory(buffer, len, &w, &h, &channels, 4);
    if (!raw) return NULL;

    uint32_t *argb = (uint32_t *)malloc(w * h * sizeof(uint32_t));
    if (!argb) { stbi_image_free(raw); return NULL; }

    /* Empaquetado directo a 0xAARRGGBB nativo del RSX en Big-Endian */
    for (int i = 0; i < w * h; i++) {
        uint8_t r = raw[i * 4 + 0];
        uint8_t g = raw[i * 4 + 1];
        uint8_t b = raw[i * 4 + 2];
        uint8_t a = raw[i * 4 + 3];
        argb[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }

    stbi_image_free(raw);
    *out_w = w;
    *out_h = h;
    return argb;
}

/* Inicializa la infobox de prueba para Wikipedia PS3 */
static void init_default_layout(void) {
    memset(&g_layout, 0, sizeof(EonPageLayout));
    strncpy(g_layout.title, "PlayStation 3", sizeof(g_layout.title) - 1);
    strncpy(g_layout.subtitle, "Consola de videojuegos de Sony", sizeof(g_layout.subtitle) - 1);
    
    g_layout.info_row_count = 5;
    strncpy(g_layout.info_rows[0].key, "Fabricante", sizeof(g_layout.info_rows[0].key) - 1);
    strncpy(g_layout.info_rows[0].value, "Sony Computer Ent.", sizeof(g_layout.info_rows[0].value) - 1);

    strncpy(g_layout.info_rows[1].key, "Lanzamiento", sizeof(g_layout.info_rows[1].key) - 1);
    strncpy(g_layout.info_rows[1].value, "11 de nov. de 2006", sizeof(g_layout.info_rows[1].value) - 1);

    strncpy(g_layout.info_rows[2].key, "CPU", sizeof(g_layout.info_rows[2].key) - 1);
    strncpy(g_layout.info_rows[2].value, "Cell Broadband Engine 3.2GHz", sizeof(g_layout.info_rows[2].value) - 1);

    strncpy(g_layout.info_rows[3].key, "GPU", sizeof(g_layout.info_rows[3].key) - 1);
    strncpy(g_layout.info_rows[3].value, "RSX 'Reality Synthesizer' 500MHz", sizeof(g_layout.info_rows[3].value) - 1);

    strncpy(g_layout.info_rows[4].key, "Ventas", sizeof(g_layout.info_rows[4].key) - 1);
    strncpy(g_layout.info_rows[4].value, "87.4 millones de unidades", sizeof(g_layout.info_rows[4].value) - 1);
}'''

if old_globals in text:
    text = text.replace(old_globals, new_globals, 1)
    with open('EonSurf/repo/src/source/main.c', 'w', encoding='utf-8') as f:
        f.write(text)
    print("Globals and layout helper added")
else:
    print("Old globals not found")
