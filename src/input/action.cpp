#include <SDL3/SDL_keyboard.h>
#include <iostream>
#include "SDL3/SDL_scancode.h"
#include "audio/engine.h"
#include "input/frame_input.h"

void AudioPlayback(const bool *key_states){
    
}

static std::string buffer;
void ACTION_HandleInput(FrameInput const & input){
    
    // Given the current keys being pressed, what actions should be taken?

    /* ACTIONS: 
        AudioPlayback()
        UserMovement()
        CameraMovement()
        Interactions()
        etc..
    */
    // Play audio when W key is pressed
    AUDIO_SetPlaying(input.key_states[SDL_SCANCODE_W]);

    // Text stream input
    if (input.text_input.length() > 0) {
        buffer += input.text_input;
        std::cout << buffer << std::endl;
    }

    // Discrete inputs
    if (input.clicked) {
        std::cout << "Clicked at (" << input.click_x << ", " << input.click_y << ")." << std::endl;
    }
}
