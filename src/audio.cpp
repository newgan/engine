#include <numbers>
#include <cmath>
#include "SDL3/SDL_stdinc.h"
#include "audio.h"

const int BUFFER_SIZE = 4096;
const int SAMPLE_SIZE = 44100;

float computeSineWave(float phase){
  return SDL_sinf(phase * 2 * SDL_PI_F);
}

void oscillator_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount){
    additional_amount /= sizeof (float);  // convert from bytes to samples 
    int current_sine_sample = 0.0f;
    while (additional_amount > 0) {
        float samples[128];  // feed 128 frames at a time
        const int total = SDL_min(additional_amount, SDL_arraysize(samples));
        int i;

        // generate a 440Hz sine wave
        for (i = 0; i < total; i++) {
            const int freq = 440;
            const float phase = current_sine_sample * freq / 8000.0f;
            float output_wave = computeSineWave(phase); // this can be swapped for any wave generation function
            samples[i] = output_wave;
            current_sine_sample++;
        }

        // wrapping around to avoid floating-point errors 
        current_sine_sample %= 8000;

        // feed the new data to the stream.
        SDL_PutAudioStreamData(stream, samples, total * sizeof (float));
        additional_amount -= total;
    }
}