#ifndef PSX_MENU_H
#define PSX_MENU_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_GAMES 32
#define MAX_GAME_NAME 64
#define GAME_SCAN_PATH "/psx/roms"

typedef struct {
    char path[256];
    char name[MAX_GAME_NAME];
    char format[8];
    uint32_t size;
} GameEntry;

typedef struct {
    GameEntry games[MAX_GAMES];
    uint32_t count;
    uint32_t selected;
    bool menu_active;
    bool scanning;
    char status[128];
} GameMenu;

void menu_init(GameMenu *menu);
void menu_scan_games(GameMenu *menu);
void menu_draw(GameMenu *menu);
void menu_navigate(GameMenu *menu, int direction);
const char* menu_get_selected(GameMenu *menu);

#endif
