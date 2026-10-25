#ifndef EONSURF_MEDIA_PLAYER_H
#define EONSURF_MEDIA_PLAYER_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* Estado del reproductor de audio integrado */
static int g_music_playing = 0;
static char g_music_title[64] = "Sin Reproduccion";

/* Alternar estado de musica de fondo */
static inline void eonsurf_toggle_background_music(void) {
    g_music_playing = !g_music_playing;
    if (g_music_playing) {
        strncpy(g_music_title, "EonSurf Ambient Chill (Synthwave)", sizeof(g_music_title) - 1);
    } else {
        strncpy(g_music_title, "Pausado", sizeof(g_music_title) - 1);
    }
}

#endif /* EONSURF_MEDIA_PLAYER_H */
