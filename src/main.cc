#include <cmath>
#include <filesystem>
#include <iterator>
#include <string>
#include <unistd.h>

#define LOOM_IMPL_
#include "loom.h"

#include <imtui/imtui-impl-text.h>
#include <imtui/imtui.h>
#include <imtui/imtui-impl-ncurses.h>

typedef enum View {
    REVIEWING_CARDS,
    BROWSING_FOLDERS,
} View;

typedef struct State {
    View view;
    bool run;
    uint64_t selected_folder;
    std::string current_dir;
    ImTui::TScreen *screen = nullptr;
    ImVec2 screen_size;
} State;

void render();
void render_end();
void update();
void shutdown();

void select_next_folder();
void select_last_folder();
void open_selected_folder();
void open_parent_folder();
void open_dir(const char *path);

State state = {
    .run = true,
    .screen = nullptr,
};

int main() {
    char cwd_buff[512];

    loom_init("/home/sydney/dev/loom++/");

    getcwd(cwd_buff, 512);
    state.current_dir = std::string(cwd_buff);
    open_dir(state.current_dir.c_str());
    state.view = BROWSING_FOLDERS;

    ImGui::CreateContext();
    state.screen = ImTui_ImplNcurses_Init(true);
    ImTui_ImplText_Init();
    while (state.run) {
        render();
        render_end();
        update();

    }

    shutdown();
    return 0;
}

void render() {
    char window_title[512];
    ImTui_ImplText_NewFrame();
    ImTui_ImplNcurses_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    state.screen_size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowSize(state.screen_size, ImGuiCond_Always);

    const FolderMenu *menu = folder_menu();
    const ReviewMenu *review = review_menu();
    const Card *current_card = NULL;
    char buff[512];
    char buff2[512];

    switch (state.view) {
    case BROWSING_FOLDERS:
        strcpy(window_title, state.current_dir.c_str());
        break;
    case REVIEWING_CARDS:
        strcpy(window_title, "Reviewing: ");
        strcat(window_title, state.current_dir.c_str());
        break;;
    }

    ImGui::Begin(window_title, nullptr, 
                 ImGuiWindowFlags_NoResize | 
                 ImGuiWindowFlags_NoMove | 
                 ImGuiWindowFlags_NoScrollbar | 
                 ImGuiWindowFlags_NoCollapse );
    switch (state.view) {
    case BROWSING_FOLDERS:
        for (int i = 0; i < menu->folder_count; i++) {
            if (ImGui::Selectable(menu->folders[i].path, state.selected_folder == i)) {
                state.selected_folder = i;
            }
        }
        if (ImGui::Button("open dir")) {
            open_selected_folder();
        }
        break;
    case REVIEWING_CARDS:

        if (review->current_card + 1 > review->card_count) {
            ImGui::Text("Review Complete");
            ImGui::Text("%lu/%lu correct", review->correct, review->card_count);
            if (ImGui::SmallButton("Exit (e)") || ImGui::IsKeyPressed('e')) {
                state.view = BROWSING_FOLDERS;
            }
            break;
        }

        current_card = &review->cards[review->current_card];

        ImGui::SetCursorPos(ImVec2(2, 2));
        ImGui::TextColored(ImVec4(1.0, 0.2, 0.15, 1.0), "%lu", review->incorrect);

        sprintf(buff, "%lu", review->current_card);
        strcat(buff, " -> ");
        sprintf(buff2, "%lu", review->card_count);
        strcat(buff, buff2);
        ImGui::SetCursorPos(ImVec2((int32_t)(state.screen_size.x / 2) - (int32_t)(strlen(buff) / 2), 2));
        ImGui::Text("%s", buff);

        ImGui::SetCursorPos(ImVec2(state.screen_size.x - 5, 2.0));
        ImGui::TextColored(ImVec4(0.2, 1.0, 0.1, 1.0), "%lu", review->correct);
        //fprintf(stderr, "PROBE: %lu", );

        ImGui::SetCursorPos(ImVec2(2, 6));
        if (review->showing_front) {
            ImGui::Text("%s", current_card->front);
        } else {
            ImGui::Text("%s", current_card->back);
        }

        break;
    }
}

void render_end() {
    ImGui::End();
    ImGui::Render();
    ImTui_ImplText_RenderDrawData(ImGui::GetDrawData(), state.screen);
    ImTui_ImplNcurses_DrawScreen();
}

void update() {
    if (ImGui::IsKeyDown(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
        state.run = false;
    }

    switch (state.view) {
    case REVIEWING_CARDS:
        if (review_menu()->showing_front) {
            if (ImGui::IsKeyPressed('j')) {
                advance_review(false);
            }
        } else {
            if (ImGui::IsKeyPressed('l')) {
                advance_review(true);
            }
            else if (ImGui::IsKeyPressed('h')) {
                advance_review(false);
            }
        }
        break;

    case BROWSING_FOLDERS:
        if (ImGui::IsKeyPressed('j')) {
            select_next_folder();
        }
        if (ImGui::IsKeyPressed('k')) {
            select_last_folder();
        }
        if (ImGui::IsKeyPressed('l')) {
            open_selected_folder();
        }
        if (ImGui::IsKeyPressed('h')) {
            open_parent_folder();
        }
        if (ImGui::IsKeyPressed('r')) {
            load_review(state.current_dir.c_str());
            state.view = REVIEWING_CARDS;
        }
        break;

    }
}

void select_next_folder() {
    if (state.selected_folder + 1 >= folder_menu()->folder_count) {
        state.selected_folder = 0;
        return;
    } else {
        state.selected_folder += 1;
    }
}

void select_last_folder() {
    if (state.selected_folder <= 0) {
        state.selected_folder = folder_menu()->folder_count - 1;
    } else {
        state.selected_folder -= 1;
    }
}

void open_selected_folder() {
    open_dir(folder_menu()->folders[state.selected_folder].path);
}

void open_parent_folder() {
    char path[512];
    strcpy(path, state.current_dir.c_str());
    for (int i = strlen(path); i > 0; i--) {
        if (path[i] == '/') {
            path[i] = '\0';
            open_dir(path);
            return;
        }
    }
}

void shutdown() {
    ImTui_ImplText_Shutdown();
    ImTui_ImplNcurses_Shutdown();

    loom_shutdown();
}

void open_dir(const char *path) {
    loom_open_dir(path);
    state.current_dir = path;
}

