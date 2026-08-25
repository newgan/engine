#include <SDL3/SDL_keyboard.h>
#include "audio/engine.h"

void AudioPlayback(const bool *key_states){
    
}

void ACTION_HandleInput(){
    const bool *key_states = SDL_GetKeyboardState(NULL);
    
    // Given the current keys being pressed, what actions should be taken?

    /* ACTIONS: 
        AudioPlayback()
        UserMovement()
        CameraMovement()
        Interactions()
        etc..
    */

    // Play audio when W key is pressed
    AUDIO_SetPlaying(key_states[SDL_SCANCODE_W]);
}
