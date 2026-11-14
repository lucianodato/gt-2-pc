// Minimal deterministic SDL audio test double. Never used by the game target.
#pragma once
#include <cstdint>
using Uint8 = uint8_t;
using Uint16 = uint16_t;
using Uint32 = uint32_t;
using SDL_AudioDeviceID = Uint32;
constexpr Uint32 SDL_INIT_AUDIO = 16;
constexpr Uint16 AUDIO_S16SYS = 0x8010;
#define SDL_AUDIO_BITSIZE(format) ((format) & 0xff)
struct SDL_AudioSpec { int freq; Uint16 format; Uint8 channels; Uint16 samples; };
struct SDL_AudioStream { int rate; int available = 0; };
int SDL_InitSubSystem(Uint32);
void SDL_QuitSubSystem(Uint32);
const char* SDL_GetError();
int SDL_GetDefaultAudioInfo(char**, SDL_AudioSpec*, int);
const char* SDL_getenv(const char*);
int SDL_atoi(const char*);
SDL_AudioDeviceID SDL_OpenAudioDevice(const char*, int, const SDL_AudioSpec*, SDL_AudioSpec*, int);
void SDL_CloseAudioDevice(SDL_AudioDeviceID);
void SDL_PauseAudioDevice(SDL_AudioDeviceID, int);
const char* SDL_GetCurrentAudioDriver();
SDL_AudioStream* SDL_NewAudioStream(Uint16, Uint8, int, Uint16, Uint8, int);
void SDL_FreeAudioStream(SDL_AudioStream*);
int SDL_AudioStreamPut(SDL_AudioStream*, const void*, int);
int SDL_AudioStreamAvailable(SDL_AudioStream*);
int SDL_AudioStreamGet(SDL_AudioStream*, void*, int);
Uint32 SDL_GetQueuedAudioSize(SDL_AudioDeviceID);
int SDL_QueueAudio(SDL_AudioDeviceID, const void*, Uint32);
void SDL_Delay(Uint32);
