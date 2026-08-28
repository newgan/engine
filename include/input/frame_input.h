# pragma once
#include <string>

struct FrameInput {
    const bool *key_states;

    bool clicked = false;
    int click_x = 0; int click_y = 0;

    std::string text_input;

    bool quit = false;
};