#ifndef LOOM_H_
#define LOOM_H_

#include <stdbool.h>
#include <inttypes.h>

typedef struct Card {
    const char *front;
    const char *back;
    const char *path;
} Card;

typedef struct Folder {
    char *path;
    char *display_name;
} Folder;

typedef struct FolderMenu {
    uint64_t folder_count;
    uint64_t folder_capacity;
    Folder *folders;
} FolderMenu;

typedef struct ReviewMenu {
    uint64_t card_count;
    uint64_t card_capacity;
    uint64_t current_card;
    uint64_t correct;
    uint64_t incorrect;
    Card *cards;
    bool showing_front;
} ReviewMenu;

void loom_init(const char *base_path);
void loom_shutdown();

void loom_open_dir(const char *path);

void load_review(const char *path);
void advance_review(bool correct);

const FolderMenu *folder_menu();
const ReviewMenu *review_menu();

#endif // LOOM_H_

#ifdef LOOM_IMPL_

#define _XOPEN_SOURCE 700
#define DEFAULT_REGION_CAPACITY 4096
#define START_FOLDER_CAPACITY 4
#define START_CARD_CAPACITY 4

#include <string.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdio.h>

typedef struct Region {
    char *data;
    uint64_t size;
    uint64_t capacity;
} Region;

typedef enum LogLevel {
    INFO,
    WARNING,
    ERROR,
    NO_LOGS
} LogLevel;

void loom_log(LogLevel level, const char *fmt, ...);
Region *new_lone_Region();
void free_region(Region *region);
void *region_alloc(Region *region, size_t size);
char *region_make_str(Region *region, const char *str);
void chop_newline(char *str);

void add_folder(FolderMenu *menu, const char *path);
Folder *new_folder(const char *full_path);
void grow_folder_menu(FolderMenu *menu);

void recursive_card_search(ReviewMenu *menu, const char *path);
void load_cards(ReviewMenu *menu, const char *path);
void add_card(ReviewMenu *menu, const char *front, const char *back, const char *path);
void grow_review_menu(ReviewMenu *menu);

char *get_head(char *str);

static Region folder_region;
static Region review_region;
static FolderMenu folder_select;
static ReviewMenu card_review;


// public api implementation

void loom_init(const char *base_path) {
    folder_region.data = (char*)malloc(DEFAULT_REGION_CAPACITY);
    if (folder_region.data == NULL) {
        loom_log(ERROR, "failed to allocate folder_region");
    }
    folder_region.capacity = DEFAULT_REGION_CAPACITY;
    folder_region.size = 0;

    review_region.data = (char*)malloc(DEFAULT_REGION_CAPACITY);
    if (review_region.data == NULL) {
        loom_log(ERROR, "failed to allocate review_region");
    }
    review_region.capacity = DEFAULT_REGION_CAPACITY;
    review_region.size = 0;

    folder_select.folders = (Folder*)malloc(sizeof(Folder) * START_FOLDER_CAPACITY);
    if (!folder_select.folders) {
        loom_log(ERROR, "failed to allocate cards");
        exit(1);
    }
    folder_select.folder_capacity = START_FOLDER_CAPACITY;

    card_review.cards = (Card*)malloc(sizeof(Card) * START_CARD_CAPACITY);
    if (!card_review.cards) {
        loom_log(ERROR, "failed to allocate cards");
        exit(1);
    }
    card_review.card_capacity = START_CARD_CAPACITY;
}

void loom_shutdown() {
    free(folder_region.data);
    free(review_region.data);
}

void loom_open_dir(const char *path) {
    DIR *dir = NULL;
    struct dirent *ent = NULL;
    struct stat stat_buff;
    char buff[512];
    folder_select.folder_count = 0;

    dir = opendir(path);
    if (!dir) {
        loom_log(ERROR, "Failed to open directory: %s", path);
        exit(1);
    }
    while ((ent = readdir(dir))) {
        strcpy(buff, path);
        buff[strlen(buff) + 1] = '\0';
        buff[strlen(buff)] = '/';
        strcat(buff, ent->d_name);
        if (stat(buff, &stat_buff) == -1) {
            perror("stat");
            loom_log(ERROR, "stat fail");
            exit(1);
        }

        if (strcmp(ent->d_name, "..") == 0 || strcmp(ent->d_name, ".") == 0) {
            continue;
        }

        if ((stat_buff.st_mode & S_IFMT) == S_IFDIR) {
            strcpy(buff, path);
            buff[strlen(buff) + 1] = '\0';
            buff[strlen(buff)] = '/';
            strcat(buff, ent->d_name);
            add_folder(&folder_select, buff);
        }
    }

    if (dir) {
        closedir(dir);
    }
}

void load_review(const char *path) {
    card_review.showing_front = true;
    card_review.current_card = 0;
    recursive_card_search(&card_review, path);
}

void advance_review(bool correct) {
    if (card_review.current_card + 1 > card_review.card_count) {
        return;
    }
    if (card_review.showing_front) {
        card_review.showing_front = false;
        return;
    }
    if (correct) {
        card_review.correct += 1;
    } else {
        card_review.incorrect += 1;
    }
    if (card_review.current_card + 1 <= card_review.card_count) {
        card_review.current_card += 1;
    }
    card_review.showing_front = true;
}

const FolderMenu *folder_menu() {
    return &folder_select;
}

const ReviewMenu *review_menu() {
    return  &card_review;
}


// private

void loom_log(LogLevel level, const char *fmt, ...) {
    return;
	// TODO: Minimal log level missing for now
    switch (level) {
    case INFO:
        fprintf(stderr, "[INFO] ");
        break;
    case WARNING:
        fprintf(stderr, "[WARNING] ");
        break;
    case ERROR:
        fprintf(stderr, "[ERROR] ");
        break;
    case NO_LOGS: return;
    default:
        // TODO: unreachable stuff?
        fprintf(stderr, "%s:%d: UNREACHABLE: %s\n", __FILE__, __LINE__, "log");
    }

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void recursive_card_search(ReviewMenu *menu, const char *path) {
    DIR *dir = NULL;
    FILE *file = NULL;
    struct dirent *ent = NULL;
    struct stat stat_buff;
    char buff[512];

    dir = opendir(path);
    if (!dir) {
        loom_log(ERROR, "failed to open dir: %s", path);
        exit(1);
    }
    while ((ent = readdir(dir))) {
        strcpy(buff, path);
        buff[strlen(buff)+1] = '\0';
        buff[strlen(buff)] = '/';
        strcat(buff, ent->d_name);
        if (stat(buff, &stat_buff) == -1) {
            perror("stat");
            loom_log(ERROR, "stat fail");
            exit(1);
        }

        if (stat_buff.st_mode == S_IFREG) {
            loom_log(INFO, "reg file");
        }

        if (strcmp(ent->d_name, "..") == 0 || strcmp(ent->d_name, ".") == 0) {
            continue;
        }

        if ((stat_buff.st_mode & S_IFMT) == S_IFREG) {
            strcpy(buff, path);
            buff[strlen(buff)+1] = '\0';
            buff[strlen(buff)] = '/';
            strcat(buff, ent->d_name);

            load_cards(menu, buff);
        } else if ((stat_buff.st_mode & S_IFMT) == S_IFDIR) {
            strcpy(buff, path);
            buff[strlen(buff)+1] = '\0';
            buff[strlen(buff)] = '/';
            strcat(buff, ent->d_name);
            recursive_card_search(menu, buff);
        }
    }
}

void load_cards(ReviewMenu *menu, const char *path) {
    FILE *in = NULL;
    char buff[512];
    char front_buff[512];
    bool made_front = false;

    in = fopen(path, "r");
    if (!in) {
        loom_log(ERROR, "Failed to open file");
        exit(1);
    }

    while (fgets(buff, 512, in) != NULL) {
        if (buff[0] == '#') {
            made_front = true;
            strcpy(front_buff, buff);
            chop_newline(front_buff);
            continue;
        }
        if (made_front
         && buff[0] == ':'
         && buff[1] == ':'
         ) {
            add_card(&card_review, front_buff, buff, path);

            made_front = false;
            continue;
        }
    }
}

void add_card(ReviewMenu *menu, const char *front, const char *back, const char *path) {
    grow_review_menu(menu);
    Card *current = &menu->cards[menu->card_count];
    current->front = region_make_str(&review_region, front);
    current->back= region_make_str(&review_region, back);
    current->path = region_make_str(&review_region, path);
    menu->card_count += 1;
}

void grow_review_menu(ReviewMenu *menu) {
    if (menu->card_count < menu->card_capacity) {
        return;
    }

    Card *alloc = (Card *)realloc(menu->cards, sizeof(Card) * menu->card_capacity * 2);
    if (!alloc) {
        loom_log(ERROR, "failed to realloc cards in ReviewMenu");
        exit(1);
    }
    menu->card_capacity *= 2;
    menu->cards = alloc;
}

void add_folder(FolderMenu *menu, const char *path) {
    grow_folder_menu(menu);

    menu->folders[menu->folder_count].path = region_make_str(&folder_region, path);
    menu->folders[menu->folder_count].display_name = get_head(menu->folders[menu->folder_count].path);
    menu->folder_count += 1;
}

void grow_folder_menu(FolderMenu *menu) {
    if (menu->folder_count < menu->folder_capacity) {
        return;
    }

    Folder *alloc = (Folder*)realloc(menu->folders, sizeof(Folder) * menu->folder_capacity * 2);
    if (!alloc) {
        loom_log(ERROR, "failed to realloc cards in ReviewMenu");
        exit(1);
    }
    menu->folder_capacity*= 2;
    menu->folders = alloc;
}

void chop_newline(char *str) {
    size_t i = 0;
    while (str[i] != '\0') {
        if (str[i] == '\n') {
            str[i] = '\0';
            break;
        }
        ++i;
    }
}

char *get_head(char *str) {
    for (int i = strlen(str); i > 1; i--) {
        if (str[i] == '/') {
            return &str[i+1];
        }
    }
    return NULL;
}

Region *new_lone_Region() {
    char *alloc = (char*)malloc(sizeof(Region) + DEFAULT_REGION_CAPACITY);
    Region *region = (Region*)alloc;
    region->data = alloc + sizeof(Region);
    region->capacity = DEFAULT_REGION_CAPACITY;
    region->size = 0;
    return region;
}

void free_region(Region *region) {
    free(region);
}

void *region_alloc(Region *region, size_t size) {
    if (region->size + size >= region->capacity) {
        loom_log(ERROR, "region out of capacity");
        exit(1);
    }
    void *result = region->data + region->size;
    memset(result, 0, size);
    region->size += size;
    return result;
}

char *region_make_str(Region *region, const char *str) {
    uint64_t len = strlen(str);
    char *alloc = (char*)region_alloc(region, (sizeof(char) * len) + 1);
    strcpy(alloc, str);
    alloc[len] = '\0';

    return alloc;
}

#endif // LOOM_IMPL_
