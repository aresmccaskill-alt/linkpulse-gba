#include <gba.h>
#include <stdbool.h>
#include <stdint.h>

#define SCREEN_W 240
#define SCREEN_H 160
#define VRAM16 ((volatile uint16_t *)0x06000000)
#define REG_KEYINPUT (*(volatile uint16_t *)0x04000130)
#define REG_VCOUNT (*(volatile uint16_t *)0x04000006)
#define KEY_A      (1u << 0)
#define KEY_B      (1u << 1)
#define KEY_START  (1u << 3)
#define KEY_RIGHT  (1u << 4)
#define KEY_LEFT   (1u << 5)
#define KEY_UP     (1u << 6)
#define KEY_DOWN   (1u << 7)
#define KEY_MASK (KEY_A | KEY_B | KEY_START | KEY_RIGHT | KEY_LEFT | KEY_UP | KEY_DOWN)

#define C_BLACK RGB5(1, 2, 8)
#define C_NAVY RGB5(3, 7, 18)
#define C_PANEL RGB5(7, 13, 27)
#define C_BLUE RGB5(5, 17, 29)
#define C_CYAN RGB5(7, 27, 30)
#define C_PINK RGB5(29, 8, 21)
#define C_YELLOW RGB5(31, 25, 8)
#define C_WHITE RGB5(30, 30, 31)
#define C_MUTED RGB5(17, 20, 25)
#define C_GREEN RGB5(8, 27, 16)

static const char glyph_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-./!?'+";
static const uint8_t glyphs[][7] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
    {0,0,4,0,0,4,0}, {0,0,0,0,0,0,31}, {0,0,0,31,0,0,0},
    {0,1,2,4,8,16,0}, {4,10,17,17,31,17,17}, {0,4,4,0,4,4,0},
    {0,0,0,0,0,0,0}
};

typedef enum { PAGE_HOME, PAGE_SEARCH, PAGE_DIRECTORY, PAGE_SITE, PAGE_CREATE, PAGE_GAME, PAGE_EGGS } Page;

typedef struct { const char *name; const char *url; const char *line1; const char *line2; uint16_t accent; } Site;

static const Site builtins[] = {
    {"PIXEL GARDEN", "garden.link", "A tiny garden for", "curious pixel fans.", C_GREEN},
    {"MOON RADIO", "moon.link", "Music from the", "other side of night.", C_PINK},
    {"GBA ARCHIVE", "archive.link", "Stories, sprites,", "and friendly pixels.", C_CYAN},
    {"STAR ATLAS", "stars.link", "Find constellations", "in a pocket sky.", C_YELLOW}
};
#define BUILTIN_COUNT 4
#define MAX_SITES 5

static Page page = PAGE_HOME;
static int menu_selection = 0;
static int site_selection = 0;
static int open_site = 0;
static int custom_site_exists = 0;
static char custom_title[13] = "MY PIXEL WEB";
static int edit_cursor = 0;
static int game_x = 112;
static int game_y = 120;
static int star_x = 55;
static int star_y = 62;
static unsigned int score = 0;
static int easter_egg = 0;
static unsigned int combo_position = 0;
static const uint16_t combo[] = { KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_LEFT, KEY_RIGHT, KEY_B, KEY_A };
static const char alphabet[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";

static void wait_vblank(void) {
    while (REG_VCOUNT >= 160) { }
    while (REG_VCOUNT < 160) { }
}

static void fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;
    for (int py = y; py < y + h; py++)
        for (int px = x; px < x + w; px++) VRAM16[py * SCREEN_W + px] = color;
}

static int char_index(char ch) {
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
    for (int i = 0; glyph_chars[i]; i++) if (glyph_chars[i] == ch) return i;
    return -1;
}

static void draw_char(int x, int y, char ch, uint16_t color, int scale) {
    if (ch == ' ') return;
    int idx = char_index(ch);
    if (idx < 0) return;
    for (int row = 0; row < 7; row++) {
        uint8_t bits = glyphs[idx][row];
        for (int col = 0; col < 5; col++) {
            if (bits & (1u << (4 - col))) fill_rect(x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

static void draw_text(int x, int y, const char *text, uint16_t color, int scale) {
    for (; *text; text++, x += 6 * scale) draw_char(x, y, *text, color, scale);
}

static void draw_shell(const char *url) {
    fill_rect(0, 0, SCREEN_W, SCREEN_H, C_BLACK);
    fill_rect(0, 0, SCREEN_W, 20, C_NAVY);
    fill_rect(8, 6, 8, 8, easter_egg ? C_YELLOW : C_CYAN);
    draw_text(21, 6, "LINKPULSE", C_WHITE, 1);
    draw_text(166, 7, "LOCAL WEB", C_MUTED, 1);
    fill_rect(7, 25, 226, 16, C_PANEL);
    fill_rect(7, 25, 3, 16, easter_egg ? C_YELLOW : C_CYAN);
    draw_text(15, 30, url, C_WHITE, 1);
    fill_rect(0, 150, SCREEN_W, 10, C_NAVY);
    draw_text(8, 152, "A:OPEN  B:BACK  START:MENU", C_MUTED, 1);
}

static int site_count(void) { return BUILTIN_COUNT + custom_site_exists; }

static const char *site_name(int index) {
    if (index >= BUILTIN_COUNT) return custom_title;
    return builtins[index].name;
}

static const char *site_url(int index) {
    if (index >= BUILTIN_COUNT) return "my.link";
    return builtins[index].url;
}

static uint16_t site_color(int index) {
    if (index >= BUILTIN_COUNT) return C_PINK;
    return builtins[index].accent;
}

static void draw_site_list(const char *heading) {
    draw_shell("linkpulse://search");
    draw_text(10, 49, heading, C_CYAN, 1);
    for (int i = 0; i < site_count(); i++) {
        int y = 62 + i * 16;
        uint16_t bg = (i == site_selection) ? C_BLUE : C_PANEL;
        fill_rect(9, y, 222, 13, bg);
        fill_rect(9, y, 3, 13, site_color(i));
        draw_text(17, y + 3, site_name(i), C_WHITE, 1);
        draw_text(159, y + 3, site_url(i), C_MUTED, 1);
    }
}

static void open_selected_site(void) {
    if (site_selection < 0) site_selection = 0;
    if (site_selection >= site_count()) site_selection = site_count() - 1;
    open_site = site_selection;
    page = PAGE_SITE;
}

static void draw_home(void) {
    draw_shell("linkpulse://home");
    fill_rect(9, 48, 222, 33, C_PANEL);
    draw_text(17, 55, easter_egg ? "YOU FOUND THE SECRET!" : "WELCOME TO YOUR LOCAL WEB", easter_egg ? C_YELLOW : C_CYAN, 1);
    draw_text(17, 68, "SMALL SITES. BIG IMAGINATION.", C_WHITE, 1);
    const char *items[] = { "SEARCH LOCAL SITES", "BROWSE DIRECTORY", "MAKE A WEBSITE", "PLAY STAR CATCH", "EASTER EGGS" };
    for (int i = 0; i < 5; i++) {
        int y = 86 + i * 12;
        fill_rect(12, y, 216, 10, i == menu_selection ? C_BLUE : C_PANEL);
        fill_rect(12, y, 3, 10, i == menu_selection ? C_CYAN : C_MUTED);
        draw_text(20, y + 2, items[i], i == menu_selection ? C_WHITE : C_MUTED, 1);
    }
}

static void draw_site(void) {
    draw_shell(site_url(open_site));
    fill_rect(9, 49, 222, 89, C_PANEL);
    fill_rect(9, 49, 222, 3, site_color(open_site));
    draw_text(17, 59, site_name(open_site), C_WHITE, 1);
    if (open_site >= BUILTIN_COUNT) {
        draw_text(17, 77, "A WEBSITE MADE IN LINKPULSE.", C_CYAN, 1);
        draw_text(17, 91, "YOUR LITTLE CORNER OF THE WEB.", C_WHITE, 1);
        draw_text(17, 119, "LINKS: HOME  GAMES  ABOUT", C_MUTED, 1);
    } else {
        draw_text(17, 79, builtins[open_site].line1, C_CYAN, 1);
        draw_text(17, 92, builtins[open_site].line2, C_WHITE, 1);
        draw_text(17, 119, "LINKS: HOME  GAMES  ABOUT", C_MUTED, 1);
    }
    draw_text(17, 131, "THIS IS A FICTIONAL OFFLINE SITE.", C_MUTED, 1);
}

static void draw_create(void) {
    draw_shell("linkpulse://create");
    fill_rect(9, 49, 222, 88, C_PANEL);
    draw_text(17, 58, "MAKE A LOCAL WEBSITE", C_CYAN, 1);
    draw_text(17, 74, "EDIT TITLE WITH THE D-PAD", C_WHITE, 1);
    fill_rect(17, 90, 206, 19, C_BLUE);
    draw_text(23, 96, custom_title, C_WHITE, 1);
    int cursor_x = 23 + edit_cursor * 6;
    fill_rect(cursor_x, 106, 5, 2, C_YELLOW);
    draw_text(17, 116, "UP/DOWN: CHARACTER", C_MUTED, 1);
    draw_text(17, 127, "LEFT/RIGHT: CURSOR  B: SPACE", C_MUTED, 1);
    draw_text(17, 137, "START: PUBLISH SITE", C_CYAN, 1);
}

static void draw_game(void) {
    draw_shell("linkpulse://games/star-catch");
    fill_rect(8, 48, 224, 94, C_NAVY);
    draw_text(14, 52, "STAR CATCH", C_YELLOW, 1);
    draw_text(165, 52, "SCORE", C_MUTED, 1);
    char score_text[8] = { '0' + (char)((score / 10) % 10), '0' + (char)(score % 10), 0 };
    draw_text(207, 52, score_text, C_WHITE, 1);
    fill_rect(star_x, star_y, 5, 5, C_YELLOW);
    fill_rect(game_x, game_y, 9, 5, C_CYAN);
    fill_rect(game_x + 2, game_y - 3, 5, 3, C_WHITE);
    draw_text(14, 131, "D-PAD MOVE  A: CATCH  START: RESET", C_MUTED, 1);
}

static void draw_eggs(void) {
    draw_shell("linkpulse://easter-eggs");
    fill_rect(9, 49, 222, 88, C_PANEL);
    draw_text(17, 60, "A LITTLE BROWSER SECRET", C_CYAN, 1);
    draw_text(17, 79, "TRY THE CLASSIC BUTTON", C_WHITE, 1);
    draw_text(17, 91, "SEQUENCE ON THE D-PAD.", C_WHITE, 1);
    draw_text(17, 113, easter_egg ? "SECRET MODE UNLOCKED!" : "THERE IS MORE THAN MEETS THE EYE.", easter_egg ? C_YELLOW : C_MUTED, 1);
}

static void publish_site(void) {
    custom_site_exists = 1;
    site_selection = BUILTIN_COUNT;
    page = PAGE_SITE;
    open_site = BUILTIN_COUNT;
}

static void update_create(unsigned int pressed) {
    int length = 0;
    while (length < 12 && custom_title[length]) length++;
    if (pressed & KEY_LEFT) { if (edit_cursor > 0) edit_cursor--; }
    if (pressed & KEY_RIGHT) { if (edit_cursor < 11) edit_cursor++; }
    if (pressed & KEY_UP || pressed & KEY_DOWN) {
        char current = custom_title[edit_cursor];
        int idx = 0;
        for (; alphabet[idx] && alphabet[idx] != current; idx++) { }
        if (!alphabet[idx]) idx = 0;
        if (pressed & KEY_UP) idx = (idx + 1) % (int)(sizeof(alphabet) - 1);
        else idx = (idx + (int)sizeof(alphabet) - 2) % (int)(sizeof(alphabet) - 1);
        custom_title[edit_cursor] = alphabet[idx];
    }
    if (pressed & KEY_B) custom_title[edit_cursor] = ' ';
    if (pressed & KEY_START) {
        while (length > 0 && custom_title[length - 1] == ' ') custom_title[--length] = '\0';
        if (length == 0) {
            custom_title[0] = 'M'; custom_title[1] = 'Y'; custom_title[2] = ' '; custom_title[3] = 'S';
            custom_title[4] = 'I'; custom_title[5] = 'T'; custom_title[6] = 'E'; custom_title[7] = '\0';
        }
        publish_site();
    }
}

static void update_game(unsigned int pressed) {
    if (pressed & KEY_LEFT && game_x > 12) game_x -= 3;
    if (pressed & KEY_RIGHT && game_x < 219) game_x += 3;
    if (pressed & KEY_UP && game_y > 64) game_y -= 3;
    if (pressed & KEY_DOWN && game_y < 130) game_y += 3;
    int dx = game_x - star_x;
    int dy = game_y - star_y;
    if (dx > -8 && dx < 8 && dy > -8 && dy < 8) {
        score++;
        star_x = 20 + (int)((score * 37u + 19u) % 196u);
        star_y = 66 + (int)((score * 23u + 11u) % 61u);
    }
}

static void update_combo(unsigned int pressed) {
    if (pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B)) {
        unsigned int relevant = pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B);
        if (relevant == combo[combo_position]) {
            combo_position++;
            if (combo_position == sizeof(combo) / sizeof(combo[0])) {
                easter_egg = 1;
                combo_position = 0;
            }
        } else {
            combo_position = (relevant == combo[0]) ? 1u : 0u;
        }
    }
}

int main(void) {
    REG_DISPCNT = 3 | (1u << 10); /* Mode 3 bitmap, BG2 enabled. */
    unsigned int previous = 0;
    while (1) {
        wait_vblank();
        unsigned int held = (unsigned int)(~REG_KEYINPUT) & KEY_MASK;
        unsigned int pressed = held & ~previous;
        previous = held;
        update_combo(pressed);

        if (page == PAGE_CREATE) {
            if (pressed & KEY_B && (pressed & KEY_START)) page = PAGE_HOME;
            update_create(pressed);
        } else if (page == PAGE_GAME) {
            update_game(pressed);
            if (pressed & KEY_START) { score = 0; game_x = 112; game_y = 120; }
            if (pressed & KEY_B) page = PAGE_HOME;
        } else if (page == PAGE_HOME) {
            if (pressed & KEY_UP) menu_selection = (menu_selection + 4) % 5;
            if (pressed & KEY_DOWN) menu_selection = (menu_selection + 1) % 5;
            if (pressed & KEY_A) {
                if (menu_selection == 0) { page = PAGE_SEARCH; site_selection = 0; }
                if (menu_selection == 1) { page = PAGE_DIRECTORY; site_selection = 0; }
                if (menu_selection == 2) { page = PAGE_CREATE; edit_cursor = 0; }
                if (menu_selection == 3) page = PAGE_GAME;
                if (menu_selection == 4) page = PAGE_EGGS;
            }
        } else if (page == PAGE_SEARCH || page == PAGE_DIRECTORY) {
            if (pressed & KEY_UP) site_selection = (site_selection + site_count() - 1) % site_count();
            if (pressed & KEY_DOWN) site_selection = (site_selection + 1) % site_count();
            if (pressed & KEY_A) open_selected_site();
            if (pressed & KEY_B) page = PAGE_HOME;
        } else if (page == PAGE_SITE || page == PAGE_EGGS) {
            if (pressed & KEY_B) page = PAGE_HOME;
            if (page == PAGE_SITE && pressed & KEY_A) page = PAGE_DIRECTORY;
        }

        switch (page) {
            case PAGE_HOME: draw_home(); break;
            case PAGE_SEARCH: draw_site_list("LOCAL SEARCH RESULTS"); break;
            case PAGE_DIRECTORY: draw_site_list("SITE DIRECTORY"); break;
            case PAGE_SITE: draw_site(); break;
            case PAGE_CREATE: draw_create(); break;
            case PAGE_GAME: draw_game(); break;
            case PAGE_EGGS: draw_eggs(); break;
        }
    }
}
