#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fat.h>

#include "psx.h"
#include "psx_exe.h"
#include "psx_slot2.h"
#include "psx_cdrom.h"
#include "psx_menu.h"
#include "psx_gpu.h"

typedef struct {
    bool fat_ready;
    bool bios_loaded;
    bool exe_loaded;
    bool bin_loaded;
    char source_label[48];
    char status_line[96];
    PsxExeInfo exe_info;
} BootStatus;

typedef struct {
    uint32_t test_id;
    bool passed;
    uint16_t error_code;
    uint32_t cycles_at_error;
    uint32_t pc_at_error;
    uint32_t opcode_at_error;
    char description[128];
} TestResult;

typedef struct {
    TestResult results[100];
    uint32_t total_tests;
    uint32_t passed_tests;
    uint32_t failed_tests;
    uint32_t current_test;
    bool test_mode;
    bool save_on_exit;
    char results_file[128];
} TestSuite;

static PsxState g_psx;
static BootStatus g_boot;
static PrintConsole g_top_console;
static PrintConsole g_bottom_console;
static bool g_auto_run = false;
static uint32_t g_run_batch = 128;
static GameMenu g_menu;
static bool g_emulator_mode = false;

static void draw_trace(const PsxState *psx) {
    uint32_t i;
    uint32_t start;

    iprintf("trace:\n");
    if (psx->trace_count == 0) {
        iprintf("  <empty>\n");
        return;
    }

    start = (psx->trace_pos + PSX_TRACE_LINES - psx->trace_count) % PSX_TRACE_LINES;
    for (i = 0; i < psx->trace_count; i++) {
        uint32_t index = (start + i) % PSX_TRACE_LINES;
        iprintf("%s\n", psx->trace[index]);
    }
}

static void draw_startup_message(const char *message) {
    consoleSelect(&g_top_console);
    consoleClear();
    iprintf("psxnds\n\n");
    iprintf("Starting up...\n");
    iprintf("%s\n", message);

    consoleSelect(&g_bottom_console);
    consoleClear();
    iprintf("PS1 Emulator\n\n");
    iprintf("%s\n", message);
}

static TestSuite g_test_suite;

static void test_init(TestSuite *suite) {
    memset(suite, 0, sizeof(*suite));
    suite->test_mode = true;
    suite->save_on_exit = true;
    snprintf(suite->results_file, sizeof(suite->results_file), "/psx/test_results.txt");
}

static void test_record_result(TestSuite *suite, uint32_t test_id, bool passed, 
                               uint16_t error_code, const char *description) {
    if (suite->current_test >= 100) {
        return;
    }
    
    TestResult *result = &suite->results[suite->current_test];
    result->test_id = test_id;
    result->passed = passed;
    result->error_code = error_code;
    result->cycles_at_error = g_psx.cycles;
    result->pc_at_error = g_psx.halt_pc;
    result->opcode_at_error = g_psx.last_opcode;
    strncpy(result->description, description, sizeof(result->description) - 1);
    result->description[sizeof(result->description) - 1] = '\0';
    
    suite->current_test++;
    suite->total_tests++;
    if (passed) {
        suite->passed_tests++;
    } else {
        suite->failed_tests++;
    }
}

static void test_save_results(TestSuite *suite) {
    if (!suite->save_on_exit) {
        return;
    }
    
    FILE *fp = fopen(suite->results_file, "w");
    if (fp == NULL) {
        return;
    }
    
    fprintf(fp, "PSX Emulator Test Results\n");
    fprintf(fp, "=========================\n");
    fprintf(fp, "Total Tests: %lu\n", (unsigned long)suite->total_tests);
    fprintf(fp, "Passed: %lu\n", (unsigned long)suite->passed_tests);
    fprintf(fp, "Failed: %lu\n", (unsigned long)suite->failed_tests);
    fprintf(fp, "\nDetailed Results:\n");
    fprintf(fp, "----------------\n\n");
    
    for (uint32_t i = 0; i < suite->current_test; i++) {
        TestResult *result = &suite->results[i];
        fprintf(fp, "Test #%lu: %s\n", (unsigned long)result->test_id, 
                result->passed ? "PASS" : "FAIL");
        if (!result->passed) {
            fprintf(fp, "  Error Code: 0x%04X\n", result->error_code);
            fprintf(fp, "  Cycles: %lu\n", (unsigned long)result->cycles_at_error);
            fprintf(fp, "  PC: 0x%08lX\n", (unsigned long)result->pc_at_error);
            fprintf(fp, "  Opcode: 0x%08lX\n", (unsigned long)result->opcode_at_error);
            fprintf(fp, "  Description: %s\n", result->description);
        }
        fprintf(fp, "\n");
    }
    
    fclose(fp);
}

static void test_run_incremental(TestSuite *suite, uint32_t num_instructions) {
    if (!g_psx.halted) {
        psx_run(&g_psx, num_instructions);
        
        if (g_psx.halted) {
            test_record_result(suite, suite->current_test + 1, false, 
                             g_psx.test_result.error_code,
                             g_psx.halt_reason);
        } else {
            test_record_result(suite, suite->current_test + 1, true, 0, "completed");
        }
    } else {
        test_record_result(suite, suite->current_test + 1, false, 0xFFFF, "already halted");
    }
}

static bool read_file(const char *path, uint8_t **out_data, size_t *out_size) {
    FILE *fp;
    long size;
    uint8_t *buffer;

    if (out_data == NULL || out_size == NULL) {
        return false;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return false;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return false;
    }

    size = ftell(fp);
    if (size <= 0) {
        fclose(fp);
        return false;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return false;
    }

    buffer = (uint8_t *)malloc((size_t)size);
    if (buffer == NULL) {
        fclose(fp);
        return false;
    }

    if (fread(buffer, 1, (size_t)size, fp) != (size_t)size) {
        free(buffer);
        fclose(fp);
        return false;
    }

    fclose(fp);
    *out_data = buffer;
    *out_size = (size_t)size;
    return true;
}

static void boot_demo(PsxState *psx, BootStatus *boot) {
    psx_init(psx);
    psx_load_demo(psx);
    boot->exe_loaded = false;
    boot->bin_loaded = false;
    strncpy(boot->source_label, "built-in demo", sizeof(boot->source_label) - 1);
    boot->source_label[sizeof(boot->source_label) - 1] = '\0';
    strncpy(boot->status_line, "No PS-X EXE found, using internal test stream.", sizeof(boot->status_line) - 1);
    boot->status_line[sizeof(boot->status_line) - 1] = '\0';
    memset(&boot->exe_info, 0, sizeof(boot->exe_info));
}

static void try_load_exe(PsxState *psx, BootStatus *boot) {
    static const char *const candidate_paths[] = {
        "/psx/demo.exe",
        "/psx/boot.exe",
        "/PSX/BOOT.EXE",
    };
    static const char *const raw_bin_path = "/psx/demo.bin";
    static const char *const bios_paths[] = {
        "/psx/scph1001.bin",
        "/psx/bios.bin",
    };
    size_t i;

    draw_startup_message("Mounting FAT...");
    boot->fat_ready = fatInitDefault();
    boot->bios_loaded = false;
    boot->exe_loaded = false;
    boot->bin_loaded = false;
    if (!boot->fat_ready) {
        boot_demo(psx, boot);
        strncpy(boot->status_line, "FAT init failed, staying on built-in demo.", sizeof(boot->status_line) - 1);
        boot->status_line[sizeof(boot->status_line) - 1] = '\0';
        return;
    }

    draw_startup_message("Scanning BIOS...");
    for (i = 0; i < sizeof(bios_paths) / sizeof(bios_paths[0]); i++) {
        uint8_t *buffer = NULL;
        size_t buffer_size = 0;

        if (!read_file(bios_paths[i], &buffer, &buffer_size)) {
            continue;
        }

        if (psx_load_bios(psx, buffer, buffer_size)) {
            boot->bios_loaded = true;
            free(buffer);
            break;
        }

        free(buffer);
    }

    draw_startup_message("Scanning PS-X EXE...");
    for (i = 0; i < sizeof(candidate_paths) / sizeof(candidate_paths[0]); i++) {
        uint8_t *buffer = NULL;
        size_t buffer_size = 0;

        if (!read_file(candidate_paths[i], &buffer, &buffer_size)) {
            continue;
        }

        psx_reset(psx);
        if (psx_exe_load(psx, buffer, buffer_size, &boot->exe_info)) {
            boot->exe_loaded = true;
            strncpy(boot->source_label, candidate_paths[i], sizeof(boot->source_label) - 1);
            boot->source_label[sizeof(boot->source_label) - 1] = '\0';
            snprintf(
                boot->status_line,
                sizeof(boot->status_line),
                "Loaded PS-X EXE, %lu bytes at %08lx.",
                (unsigned long)boot->exe_info.load_size,
                (unsigned long)boot->exe_info.load_addr
            );
            free(buffer);
            return;
        }

        free(buffer);
    }

    draw_startup_message("Scanning raw bin...");
    {
        uint8_t *buffer = NULL;
        size_t buffer_size = 0;

        if (read_file(raw_bin_path, &buffer, &buffer_size)) {
            psx_reset(psx);
            psx_load_raw_bin(psx, buffer, buffer_size, 0x00010000, 0x80010000);
            boot->exe_loaded = false;
            boot->bin_loaded = !psx->halted;
            strncpy(boot->source_label, raw_bin_path, sizeof(boot->source_label) - 1);
            boot->source_label[sizeof(boot->source_label) - 1] = '\0';
            snprintf(
                boot->status_line,
                sizeof(boot->status_line),
                "Loaded raw bin, %lu bytes at 00010000.",
                (unsigned long)buffer_size
            );
            free(buffer);
            return;
        }
    }

    if (boot->bios_loaded) {
        psx_boot_bios(psx);
        strncpy(boot->source_label, "external bios", sizeof(boot->source_label) - 1);
        boot->source_label[sizeof(boot->source_label) - 1] = '\0';
        strncpy(boot->status_line, "Booting from external PS1 BIOS.", sizeof(boot->status_line) - 1);
        boot->status_line[sizeof(boot->status_line) - 1] = '\0';
        return;
    }

    boot_demo(psx, boot);
}

static void draw_state(const PsxState *psx, const BootStatus *boot, int steps) {
    consoleSelect(&g_top_console);
    consoleClear();

    iprintf("psxnds proof of concept\n");
    iprintf("A:1  B:%lu  X:auto  Y:x8\n", (unsigned long)g_run_batch);
    iprintf("L/R: batch  START: reload\n");
    iprintf("SELECT: test mode  L: save tests\n\n");

    iprintf("source: %s\n", boot->source_label);
    iprintf("ram: %s\n", psx->ram_backend_name);
    iprintf("fat: %s\n", boot->fat_ready ? "ready" : "failed");
    iprintf("bios: %s\n", boot->bios_loaded ? "loaded" : "none");
    iprintf(
        "mode: %s\n",
        boot->exe_loaded ? "ps-x exe" : (boot->bin_loaded ? "raw bin" : (boot->bios_loaded ? "bios" : "demo"))
    );
    iprintf("auto: %s\n", g_auto_run ? "on" : "off");
    iprintf("halted: %s\n", psx->halted ? "yes" : "no");
    iprintf("%s\n", psx->halt_reason);

    if (g_test_suite.test_mode) {
        iprintf("TEST MODE:\n");
        iprintf("  passed: %lu/%lu\n", 
                (unsigned long)g_test_suite.passed_tests,
                (unsigned long)g_test_suite.total_tests);
        iprintf("  failed: %lu\n", (unsigned long)g_test_suite.failed_tests);
        iprintf("  current: %lu\n", (unsigned long)g_test_suite.current_test);
    }

    consoleSelect(&g_bottom_console);
    consoleClear();

    iprintf("Detailed State\n\n");
    iprintf("steps: %d\n", steps);
    iprintf("cycles: %lu\n", (unsigned long)psx->cycles);
    iprintf("pc: %08lx\n", (unsigned long)psx->cpu.pc);
    iprintf("next: %08lx\n", (unsigned long)psx->cpu.next_pc);
    iprintf("lastpc: %08lx\n", (unsigned long)psx->last_pc);
    iprintf("op: %08lx\n", (unsigned long)psx->last_opcode);
    iprintf("asm: %s\n", psx->last_disasm);
    iprintf("haltpc: %08lx\n", (unsigned long)psx->halt_pc);
    iprintf("gp: %08lx\n", (unsigned long)psx->cpu.gpr[28]);
    iprintf("sp: %08lx\n", (unsigned long)psx->cpu.gpr[29]);
    iprintf("t0: %08lx\n", (unsigned long)psx->cpu.gpr[8]);
    iprintf("t1: %08lx\n", (unsigned long)psx->cpu.gpr[9]);
    iprintf("irqs: 0x%04X\n", psx->pending_irqs);
    if (psx->test_mode && psx->test_result.error_code != 0) {
        iprintf("test err: 0x%04X\n", psx->test_result.error_code);
        iprintf("test desc: %s\n", psx->test_result.error_description);
    }
    iprintf(
        "io: %c %08lx=%08lx\n",
        psx->last_io_write ? 'W' : 'R',
        (unsigned long)psx->last_io_addr,
        (unsigned long)psx->last_io_value
    );
    draw_trace(psx);
    iprintf("%s\n", boot->status_line);
}

static void draw_video_output(void) {
    if (g_psx.gpu == NULL) return;
    
    uint16_t *vram = g_psx.gpu->vram;
    uint16_t display_x = g_psx.gpu->display_x;
    uint16_t display_y = g_psx.gpu->display_y;
    
    uint16_t *top_screen = (uint16_t*)BG_GFX;
    
    for (int y = 0; y < 192; y++) {
        for (int x = 0; x < 256; x++) {
            int src_y = (display_y + y) % PSX_GPU_VRAM_HEIGHT;
            int src_x = (display_x + x) % PSX_GPU_VRAM_WIDTH;
            uint16_t pixel = vram[src_y * PSX_GPU_VRAM_WIDTH + src_x];
            
            top_screen[y * 256 + x] = pixel;
        }
    }
}

static void run_menu_mode(void) {
    consoleSelect(&g_bottom_console);
    consoleClear();
    
    menu_init(&g_menu);
    menu_scan_games(&g_menu);
    menu_draw(&g_menu);
    
    while (g_menu.menu_active && !g_emulator_mode) {
        scanKeys();
        uint32_t keys = keysDown();
        
        if (keys & KEY_UP) {
            menu_navigate(&g_menu, -1);
            menu_draw(&g_menu);
        }
        if (keys & KEY_DOWN) {
            menu_navigate(&g_menu, 1);
            menu_draw(&g_menu);
        }
        
        if (keys & KEY_A) {
            const char *selected = menu_get_selected(&g_menu);
            if (selected) {
                consoleSelect(&g_top_console);
                consoleClear();
                iprintf("Loading: %s\n", selected);
                consoleSelect(&g_bottom_console);
                
                cdrom_load_image(g_psx.cdrom, selected);
                
                psx_reset(&g_psx);
                psx_boot_bios(&g_psx);
                
                videoSetMode(MODE_5_2D);
                vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
                
                g_emulator_mode = true;
                return;
            }
        }
        
        if (keys & KEY_B) {
            g_menu.menu_active = false;
            return;
        }
        
        swiWaitForVBlank();
    }
}

int main(void) {
    int total_steps = 0;

    powerOn(POWER_ALL_2D);
    defaultExceptionHandler();

    videoSetMode(MODE_0_2D);
    videoSetModeSub(MODE_0_2D);
    vramSetBankA(VRAM_A_MAIN_BG);
    vramSetBankC(VRAM_C_SUB_BG);
    consoleInit(&g_top_console, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleInit(&g_bottom_console, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

    draw_startup_message("Video initialized. FAT init...");

    if (!fatInitDefault()) {
        draw_startup_message("FAT init failed!");
        while (1) swiWaitForVBlank();
    }
    
    psx_init(&g_psx);
    g_boot.fat_ready = true;
    g_boot.bios_loaded = psx_load_bios(&g_psx, NULL, 0);
    
    run_menu_mode();
    
    if (!g_emulator_mode) {
        memset(&g_boot, 0, sizeof(g_boot));
        g_auto_run = false;
        g_run_batch = 128;
        try_load_exe(&g_psx, &g_boot);
    }
    
    draw_state(&g_psx, &g_boot, total_steps);

    while (1) {
        int keys_pressed;
        bool redraw = false;

        scanKeys();
        keys_pressed = keysDown();

        if (keys_pressed & KEY_A) {
            psx_step(&g_psx);
            total_steps++;
            redraw = true;
        }

        if (keys_pressed & KEY_B) {
            total_steps += (int)psx_run(&g_psx, g_run_batch);
            redraw = true;
        }

        if (keys_pressed & KEY_Y) {
            total_steps += (int)psx_run(&g_psx, g_run_batch * 8U);
            redraw = true;
        }

        if (keys_pressed & KEY_X) {
            g_auto_run = !g_auto_run;
            redraw = true;
        }

        if (keys_pressed & KEY_L) {
            if (g_test_suite.test_mode && g_test_suite.total_tests > 0) {
                test_save_results(&g_test_suite);
                snprintf(g_boot.status_line, sizeof(g_boot.status_line),
                        "Saved %lu test results",
                        (unsigned long)g_test_suite.total_tests);
                redraw = true;
            }
        }

        if (keys_pressed & KEY_R) {
            if (g_run_batch > 1) {
                g_run_batch >>= 1;
            }
            redraw = true;
        }

        if (keys_pressed & KEY_START) {
            if (g_test_suite.total_tests > 0) {
                test_save_results(&g_test_suite);
            }
            run_menu_mode();
            if (!g_emulator_mode) {
                try_load_exe(&g_psx, &g_boot);
            }
            total_steps = 0;
            g_auto_run = false;
            redraw = true;
        }

        if (keys_pressed & KEY_SELECT) {
            g_test_suite.test_mode = !g_test_suite.test_mode;
            if (g_test_suite.test_mode) {
                psx_init_test_mode(&g_psx);
                g_psx.test_mode = true;
                test_init(&g_test_suite);
            } else {
                if (g_test_suite.total_tests > 0) {
                    test_save_results(&g_test_suite);
                }
                g_psx.test_mode = false;
            }
            redraw = true;
        }
        
        if (g_auto_run && !g_psx.halted) {
            total_steps += (int)psx_run(&g_psx, g_run_batch);
            redraw = true;
        }

        if (g_emulator_mode && !g_test_suite.test_mode) {
            draw_video_output();
        }

        if (redraw) {
            draw_state(&g_psx, &g_boot, total_steps);
        }

        swiWaitForVBlank();
    }
}
