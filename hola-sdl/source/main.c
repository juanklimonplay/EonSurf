/* EonSurf - hola mundo: SDL2 + DualShock 3 en PS3 (PSL1GHT). */
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <sys/process.h>

SYS_PROCESS_PARAM(1001, 0x100000);

/* Fuente 5x7 minima (ASCII 32..90), una columna por byte, bit0 = fila de arriba. */
static const unsigned char F[][5] = {
{0,0,0,0,0},{0,0,0x5f,0,0},{0,7,0,7,0},{0x14,0x7f,0x14,0x7f,0x14},{0x24,0x2a,0x7f,0x2a,0x12},
{0x23,0x13,8,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0,5,3,0,0},{0,0x1c,0x22,0x41,0},{0,0x41,0x22,0x1c,0},
{0x14,8,0x3e,8,0x14},{8,8,0x3e,8,8},{0,0x50,0x30,0,0},{8,8,8,8,8},{0,0x60,0x60,0,0},{0x20,0x10,8,4,2},
{0x3e,0x51,0x49,0x45,0x3e},{0,0x42,0x7f,0x40,0},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4b,0x31},
{0x18,0x14,0x12,0x7f,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3c,0x4a,0x49,0x49,0x30},{1,0x71,9,5,3},
{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1e},{0,0x36,0x36,0,0},{0,0x56,0x36,0,0},{8,0x14,0x22,0x41,0},
{0x14,0x14,0x14,0x14,0x14},{0,0x41,0x22,0x14,8},{2,1,0x51,9,6},{0x32,0x49,0x79,0x41,0x3e},
{0x7e,0x11,0x11,0x11,0x7e},{0x7f,0x49,0x49,0x49,0x36},{0x3e,0x41,0x41,0x41,0x22},{0x7f,0x41,0x41,0x22,0x1c},
{0x7f,0x49,0x49,0x49,0x41},{0x7f,9,9,9,1},{0x3e,0x41,0x49,0x49,0x7a},{0x7f,8,8,8,0x7f},{0,0x41,0x7f,0x41,0},
{0x20,0x40,0x41,0x3f,1},{0x7f,8,0x14,0x22,0x41},{0x7f,0x40,0x40,0x40,0x40},{0x7f,2,0xc,2,0x7f},
{0x7f,4,8,0x10,0x7f},{0x3e,0x41,0x41,0x41,0x3e},{0x7f,9,9,9,6},{0x3e,0x41,0x51,0x21,0x5e},
{0x7f,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7f,1,1},{0x3f,0x40,0x40,0x40,0x3f},
{0x1f,0x20,0x40,0x20,0x1f},{0x3f,0x40,0x38,0x40,0x3f},{0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},
{0x61,0x51,0x49,0x45,0x43}};

static void text(SDL_Renderer *r, int x, int y, int s, const char *t)
{
    for (; *t; t++, x += 6 * s) {
        int c = *t; if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 90) c = 32;
        for (int i = 0; i < 5; i++)
            for (int j = 0; j < 7; j++)
                if (F[c - 32][i] & (1 << j)) {
                    SDL_Rect q = { x + i * s, y + j * s, s, s };
                    SDL_RenderFillRect(r, &q);
                }
    }
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    /* Diagnostico: todo sale por printf (TTY.log). Ver docs/ESTADO.md. */
    printf("EonSurf-sdl: SDL_Init\n");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        printf("EonSurf-sdl: SDL_Init fallo: %s\n", SDL_GetError());
        return 1;
    }
    printf("EonSurf-sdl: SDL_CreateWindow\n");
    SDL_Window *w = SDL_CreateWindow("EonSurf", 0, 0, 1280, 720, 0);
    printf("EonSurf-sdl: window=%p %s\n", (void *)w, w ? "" : SDL_GetError());
    if (!w) return 1;
    printf("EonSurf-sdl: SDL_CreateRenderer\n");
    SDL_Renderer *r = SDL_CreateRenderer(w, -1, 0);
    printf("EonSurf-sdl: renderer=%p %s\n", (void *)r, r ? "" : SDL_GetError());
    if (!r) return 1;
    SDL_Joystick *j = SDL_NumJoysticks() > 0 ? SDL_JoystickOpen(0) : NULL;
    printf("EonSurf-sdl: joysticks=%d\n", SDL_NumJoysticks());
    printf("EonSurf-sdl: primer SDL_RenderClear\n");
    int run = 1, px = 640, py = 460;
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) run = 0;
        char buf[128], btn[160] = "";
        int ax0 = 0, ax1 = 0, nb = 0;
        if (j) {
            SDL_JoystickUpdate();
            ax0 = SDL_JoystickGetAxis(j, 0); ax1 = SDL_JoystickGetAxis(j, 1);
            nb = SDL_JoystickNumButtons(j);
            for (int b = 0; b < nb && strlen(btn) < 140; b++)
                if (SDL_JoystickGetButton(j, b)) { char t[8]; snprintf(t, 8, "%d ", b); strcat(btn, t); }
            if (abs(ax0) > 4000) px += ax0 / 4000;
            if (abs(ax1) > 4000) py += ax1 / 4000;
            if (px < 0) px = 0; if (px > 1270) px = 1270;
            if (py < 0) py = 0; if (py > 710) py = 710;
        }
        SDL_SetRenderDrawColor(r, 16, 24, 48, 255); SDL_RenderClear(r);
        SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        text(r, 60, 60, 8, "EonSurf");
        text(r, 60, 150, 4, "Hola mundo SDL2 diagnostico");
        snprintf(buf, sizeof buf, "Mando: %s  Botones: %d", j ? "SI" : "NO", nb);
        text(r, 60, 230, 3, buf);
        snprintf(buf, sizeof buf, "Palanca izq: %d %d", ax0, ax1);
        text(r, 60, 280, 3, buf);
        snprintf(buf, sizeof buf, "Pulsados: %s", btn);
        text(r, 60, 330, 3, buf);
        text(r, 60, 660, 2, "Mover el cuadrado con la palanca izquierda");
        SDL_SetRenderDrawColor(r, 255, 180, 0, 255);
        SDL_Rect q = { px - 8, py - 8, 16, 16 }; SDL_RenderFillRect(r, &q);
        SDL_RenderPresent(r);
        { static int n; if (n < 3) printf("EonSurf-sdl: frame %d presentado\n", ++n); }
        SDL_Delay(16);
    }
    SDL_Quit();
    return 0;
}
