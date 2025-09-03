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
void open_dir(const char *path);
void load_review(const char *path);
void advance_review(bool correct);
const FolderMenu *folder_menu();
const ReviewMenu *review_menu();

#endif // LOOM_H_
