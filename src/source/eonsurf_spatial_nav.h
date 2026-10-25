#ifndef EONSURF_SPATIAL_NAV_H
#define EONSURF_SPATIAL_NAV_H

#include <math.h>
#include "eonsurf_md_clean.h"

/*
 * Navegación Espacial 2D con Cruceta (Spatial D-Pad Jump):
 * Busca el enlace interactivo geométricamente más cercano en la dirección solicitada (0=Arriba, 1=Abajo, 2=Izquierda, 3=Derecha).
 */
static inline int eonsurf_spatial_find_nearest(int cur_x, int cur_y, int dir, const EonCleanLink *links, int link_count) {
    if (!links || link_count <= 0) return -1;

    int best_idx = -1;
    float best_dist = 9999999.0f;

    for (int i = 0; i < link_count; i++) {
        if (!links[i].visible) continue;
        int target_x = links[i].x + (links[i].w / 2);
        int target_y = links[i].y + (links[i].h / 2);

        int dx = target_x - cur_x;
        int dy = target_y - cur_y;

        int valid_direction = 0;
        if (dir == 0 && dy < -4) valid_direction = 1;       /* Arriba */
        else if (dir == 1 && dy > 4) valid_direction = 1;  /* Abajo */
        else if (dir == 2 && dx < -6) valid_direction = 1; /* Izquierda */
        else if (dir == 3 && dx > 6) valid_direction = 1;  /* Derecha */

        if (valid_direction) {
            /* Ponderar dirección principal para evitar saltos oblicuos erráticos */
            float dist = 0.0f;
            if (dir == 0 || dir == 1) {
                dist = sqrtf((float)(dy * dy * 1.0f + dx * dx * 3.5f));
            } else {
                dist = sqrtf((float)(dx * dx * 1.0f + dy * dy * 4.0f));
            }

            if (dist < best_dist) {
                best_dist = dist;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

#endif /* EONSURF_SPATIAL_NAV_H */
