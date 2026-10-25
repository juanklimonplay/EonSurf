# Patch main.c and eonsurf_mockup_engine.h with the comprehensive suite features
import os

repo_root = r"J:\EonPS3\EonSurf\repo"
src_dir = os.path.join(repo_root, "src", "source")
main_c_path = os.path.join(src_dir, "main.c")
mockup_engine_path = os.path.join(src_dir, "eonsurf_mockup_engine.h")

# 1. Update main.c to include the 6 new headers
with open(main_c_path, "r", encoding="utf-8") as f:
    main_text = f.read()

includes_target = '#include "eonsurf_ps3_ergonomics.h"'
new_includes = '''#include "eonsurf_ps3_ergonomics.h"
#include "eonsurf_bookmarks.h"
#include "eonsurf_net_progress.h"
#include "eonsurf_spatial_nav.h"
#include "eonsurf_file_explorer.h"
#include "eonsurf_media_player.h"
#include "eonsurf_page_search.h"'''

if includes_target in main_text and 'eonsurf_bookmarks.h' not in main_text:
    main_text = main_text.replace(includes_target, new_includes, 1)
    print("Added 6 new headers to main.c")

# 2. In navigate_to: support file:/// local file explorer and bookmarks/history logging and curl progress
# Let's inspect navigate_to in main_text
nav_intercept_target = '/* 1. Intercepcion local instantanea de eonsurf://portal y eonsurf://home (Cero Red) */'
file_intercept_code = '''/* 0. Intercepcion de explorador de archivos local file:/// */
    if (strncmp(target_url, "file://", 7) == 0) {
        snprintf(page_title, sizeof(page_title), "Explorador: %s", target_url);
        static char local_dir_md[16384];
        if (eonsurf_generate_local_directory_listing(target_url, local_dir_md, sizeof(local_dir_md))) {
            eonsurf_format_markdown_and_links(local_dir_md, page_body, sizeof(page_body), g_links, &g_link_count, MAX_PAGE_LINKS);
        } else {
            snprintf(page_body, sizeof(page_body), "No se pudo acceder al directorio local %s", target_url);
        }
        eonsurf_update_dynamic_infobox("file:///local", NULL, 0, 200, g_link_count);
        strncpy(g_tabs[g_current_tab].url, current_url, sizeof(g_tabs[g_current_tab].url) - 1);
        strncpy(g_tabs[g_current_tab].title, page_title, sizeof(g_tabs[g_current_tab].title) - 1);
        strncpy(g_tabs[g_current_tab].body, page_body, sizeof(g_tabs[g_current_tab].body) - 1);
        g_tabs[g_current_tab].scroll_y = 0;
        g_tabs[g_current_tab].http_code = 200;
        eonsurf_history_push(current_url, page_body, 0, 200, strlen(page_body));
        return;
    }

    ''' + nav_intercept_target

if nav_intercept_target in main_text and 'file:///' not in main_text:
    main_text = main_text.replace(nav_intercept_target, file_intercept_code, 1)
    print("Integrated file:/// local directory explorer into navigate_to")

# 3. In navigate_to curl options: attach progress callback and accept gzip/deflate
curl_opt_target = 'curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);'
curl_opt_replacement = '''curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, eonsurf_curl_progress_cb);
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");'''

if curl_opt_target in main_text and 'CURLOPT_ACCEPT_ENCODING' not in main_text:
    main_text = main_text.replace(curl_opt_target, curl_opt_replacement, 1)
    print("Attached CURLOPT_PROGRESSFUNCTION and CURLOPT_ACCEPT_ENCODING to libcurl in main.c")

# 4. In navigate_to success: log history visit and reset load percent
hist_push_target = 'eonsurf_history_push(current_url, page_body, 0, (int)response_code, chunk.size);'
hist_push_replacement = '''eonsurf_history_push(current_url, page_body, 0, (int)response_code, chunk.size);
            eonsurf_history_log_visit(page_title, current_url);
            g_load_percent = 100.0f;'''

if hist_push_target in main_text and 'eonsurf_history_log_visit' not in main_text:
    main_text = main_text.replace(hist_push_target, hist_push_replacement, 1)
    print("Added persistent history logging to navigate_to")

# 5. In main loop: initialize bookmarks and wire up L1/R1 tab switching and D-Pad spatial nav
init_layout_target = 'init_default_layout();'
init_layout_replacement = '''init_default_layout();
    eonsurf_bookmarks_init();'''

if init_layout_target in main_text and 'eonsurf_bookmarks_init' not in main_text:
    main_text = main_text.replace(init_layout_target, init_layout_replacement, 1)
    print("Initialized bookmarks in main.c")

with open(main_c_path, "w", encoding="utf-8") as f:
    f.write(main_text)

print("Finished updating main.c")
