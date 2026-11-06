#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h" [1]

#include <stdio.h>

// This callback function runs on a background thread. 
// The sound card continuously requests chunks of audio to play out loud.
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    drmp3* pMp3 = (drmp3*)pDevice->pUserData;
    if (pMp3 == NULL) return;

    // Read frames directly from the MP3 decoder stream into the speaker output buffer [1]
    ma_uint64 framesRead = drmp3_read_pcm_frames_s16(pMp3, frameCount, (drmp3_int16*)pOutput);
    
    // Optional: Loop the audio if it reaches the end of the file
    if (framesRead < frameCount) {
        drmp3_seek_to_pcm_frame(pMp3, 0); // Reset pointer back to the start
        drmp3_read_pcm_frames_s16(pMp3, frameCount - framesRead, ((drmp3_int16*)pOutput) + (framesRead * pDevice->playback.channels));
    }

    (void)pInput; // Unused in a playback-only setup
}

int main() {
    drmp3 mp3;
    ma_device_config deviceConfig;
    ma_device device;

    // 1. Initialize the MP3 decoder and open the file stream
    if (!drmp3_init_file(&mp3, "music.mp3", NULL)) {
        printf("Error: Could not open or parse the MP3 file.\n");
        return 1;
    }

    // 2. Configure the hardware playback engine to perfectly match the MP3 data [1]
    // miniaudio will automatically handle mixing and hardware routing [1]
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = ma_format_s16;      // Signed 16-bit PCM integer data [1]
    deviceConfig.playback.channels = mp3.channels;       // Match the MP3 channels (1 for Mono, 2 for Stereo)
    deviceConfig.sampleRate        = mp3.sampleRate;     // Match the MP3 sample rate (e.g., 44100Hz)
    deviceConfig.dataCallback      = data_callback;      // Provide our audio-filling function [1]
    deviceConfig.pUserData         = &mp3;               // Pass the decoder object to the callback thread [1]

    // 3. Initialize the physical audio device hardware interface [1]
    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
        printf("Error: Failed to initialize the audio playback hardware.\n");
        drmp3_uninit(&mp3);
        return -2;
    }

    // 4. Start the background thread loop to begin pushing audio frames out [1]
    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Error: Failed to start the audio playback device thread.\n");
        ma_device_uninit(&device);
        drmp3_uninit(&mp3);
        return -3;
    }

    printf("Playing 'music.mp3'... Press ENTER to quit.\n");
    getchar(); // Keep the main app alive while the background thread plays audio

    // 5. Clean up allocations and gracefully close the audio thread [1]
    ma_device_uninit(&device);
    drmp3_uninit(&mp3);
    
    return 0;
}
