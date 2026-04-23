#include "psx_menu.h"

#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <nds.h>

static bool has_extension(const char *filename, const char *ext) {
    size_t len = strlen(filename);
    size_t ext_len = strlen(ext);
    if (len < ext_len) return false;
    return strcasecmp(filename + len - ext_len, ext) == 0;
}

static void extract_game_name(const char *path, char *name_out, size_t name_size) {
    const char *filename = path;
    
    for (const char *p = path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            filename = p + 1;
        }
    }
    
    strncpy(name_out, filename, name_size - 1);
    name_out[name_size - 1] = '\0';
    
    char *dot = strrchr(name_out, '.');
    if (dot) {
        *dot = '\0';
    }
    
    for (char *p = name_out; *p; p++) {
        if (*p == '_') *p = ' ';
    }
}

static const char* get_format(const char *path) {
    if (has_extension(path, ".iso")) return "ISO";
    if (has_extension(path, ".bin")) return "BIN";
    if (has_extension(path, ".cue")) return "CUE";
    if (has_extension(path, ".mdf")) return "MDF";
    if (has_extension(path, ".mds")) return "MDS";
    if (has_extension(path, ".nrg")) return "NRG";
    if (has_extension(path, ".img")) return "IMG";
    return "???";
}

static bool is_cd_image(const char *filename) {
    return has_extension(filename, ".iso") ||
           has_extension(filename, ".bin") ||
           has_extension(filename, ".mdf") ||
           has_extension(filename, ".nrg") ||
           has_extension(filename, ".img");
}

void menu_init(GameMenu *menu) {
    memset(menu, 0, sizeof(*menu));
    menu->selected = 0;
    menu->menu_active = true;
    menu->scanning = false;
    strcpy(menu->status, "Ready");
}

void menu_scan_games(GameMenu *menu) {
    menu->scanning = true;
    menu->count = 0;
    strcpy(menu->status, "Scanning...");
    
    DIR *dir = opendir(GAME_SCAN_PATH);
    if (dir == NULL) {
        strcpy(menu->status, "No /psx/roms folder");
        menu->scanning = false;
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && menu->count < MAX_GAMES) {
        if (entry->d_type != DT_REG) continue;
        
        if (!is_cd_image(entry->d_name)) continue;
        
        if (has_extension(entry->d_name, ".cue") ||
            has_extension(entry->d_name, ".mds") ||
            has_extension(entry->d_name, ".sub") ||
            has_extension(entry->d_name, ".ccd")) {
            continue;
        }
        
        GameEntry *game = &menu->games[menu->count];
        
        snprintf(game->path, sizeof(game->path), "%s/%s", GAME_SCAN_PATH, entry->d_name);
        extract_game_name(game->path, game->name, MAX_GAME_NAME);
        strncpy(game->format, get_format(game->path), 8);
        game->size = 0;
        
        FILE *f = fopen(game->path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            game->size = (uint32_t)size;
            fclose(f);
        }
        
        menu->count++;
    }
    
    closedir(dir);
    
    if (menu->count == 0) {
        strcpy(menu->status, "No games found");
    } else {
        snprintf(menu->status, sizeof(menu->status), "Found %u games", menu->count);
    }
    
    menu->scanning = false;
}

void menu_navigate(GameMenu *menu, int direction) {
    if (menu->count == 0) return;
    
    if (direction > 0 && menu->selected < menu->count - 1) {
        menu->selected++;
    } else if (direction < 0 && menu->selected > 0) {
        menu->selected--;
    }
}

const char* menu_get_selected(GameMenu *menu) {
    if (menu->count == 0 || menu->selected >= menu->count) {
        return NULL;
    }
    return menu->games[menu->selected].path;
}

void menu_draw(GameMenu *menu) {
    consoleClear();
    
    iprintf("\n");
    iprintf("  ===============================\n");
    iprintf("  PSX EMULATOR - Game Selection\n");
    iprintf("  ===============================\n");
    iprintf("\n");
    
    if (menu->scanning) {
        iprintf("  Scanning for games...\n");
        iprintf("  %s\n", menu->status);
        return;
    }
    
    if (menu->count == 0) {
        iprintf("  No games found!\n");
        iprintf("\n");
        iprintf("  Put PS1 ISOs in:\n");
        iprintf("  /psx/roms/\n");
        iprintf("\n");
        iprintf("  Supported formats:\n");
        iprintf("  .iso, .bin, .mdf, .nrg\n");
        return;
    }
    
    iprintf("  %s\n\n", menu->status);
    
    int start = 0;
    int end = menu->count;
    
    if (menu->count > 8) {
        if (menu->selected < 3) {
            start = 0;
            end = 8;
        } else if (menu->selected > menu->count - 4) {
            start = menu->count - 8;
            end = menu->count;
        } else {
            start = menu->selected - 3;
            end = start + 8;
        }
    }
    
    for (int i = start; i < end; i++) {
        GameEntry *game = &menu->games[i];
        
        if (i == (int)menu->selected) {
            iprintf(" > %-20s [%s]\n", game->name, game->format);
        } else {
            iprintf("   %-20s [%s]\n", game->name, game->format);
        }
    }
    
    iprintf("\n");
    iprintf("  UP/DOWN: Navigate\n");
    iprintf("  A: Start game\n");
    iprintf("  B: Back to BIOS test\n");
    iprintf("\n");
    iprintf("  Game %u of %u\n", menu->selected + 1, menu->count);
}
