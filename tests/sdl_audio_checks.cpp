// Exercise the production SDL worker with a paused device and injected conversion errors.
#include "SDL.h"
#include "game/audio/audio_device.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {
int rate = 44100, failure = 0;
std::atomic<Uint32> queued{0};
std::atomic<bool> throttled{false};
void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}
int SDL_InitSubSystem(Uint32) { return 0; }
void SDL_QuitSubSystem(Uint32) {}
const char* SDL_GetError() { return "injected stream failure"; }
int SDL_GetDefaultAudioInfo(char**, SDL_AudioSpec* spec, int) { spec->freq = rate; return 0; }
const char* SDL_getenv(const char*) { return nullptr; }
int SDL_atoi(const char* s) { return std::atoi(s); }
SDL_AudioDeviceID SDL_OpenAudioDevice(const char*, int, const SDL_AudioSpec* in, SDL_AudioSpec* out, int) { *out = *in; return 1; }
void SDL_CloseAudioDevice(SDL_AudioDeviceID) {}
void SDL_PauseAudioDevice(SDL_AudioDeviceID, int) {}
const char* SDL_GetCurrentAudioDriver() { return "test"; }
SDL_AudioStream* SDL_NewAudioStream(Uint16, Uint8, int, Uint16, Uint8, int frequency) { return new SDL_AudioStream{frequency}; }
void SDL_FreeAudioStream(SDL_AudioStream* s) { delete s; }
int SDL_AudioStreamPut(SDL_AudioStream* s, const void*, int bytes) { s->available += int(int64_t(bytes) * s->rate / 44100); return failure == 1 ? -1 : 0; }
int SDL_AudioStreamAvailable(SDL_AudioStream* s) { return failure == 2 ? -1 : s->available; }
int SDL_AudioStreamGet(SDL_AudioStream* s, void* out, int bytes) { if (failure == 3) return -1; std::memset(out, 0, bytes); s->available -= bytes; return bytes; }
Uint32 SDL_GetQueuedAudioSize(SDL_AudioDeviceID) { return queued; }
int SDL_QueueAudio(SDL_AudioDeviceID, const void*, Uint32 bytes) { if (failure == 4) return -1; queued += bytes; return 0; }
void SDL_Delay(Uint32 ms) { throttled = true; std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
int main() {
    try {
        gt2::audio::Mixer mixer;
        for (int frequency : {44100, 48000, 96000, 192000}) {
            for (int fault = 0; fault <= 4; ++fault) {
                if (frequency == 44100 && fault > 0 && fault < 4) continue;
                rate = frequency; failure = fault; queued = 0; throttled = false;
                gt2::audio::AudioDevice audio;
                std::string error;
                Require(audio.Open(mixer, error), "open failed");
                const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(2);
                while (audio.IsOpen() && !throttled && std::chrono::steady_clock::now() < end)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                const bool stopped = !audio.IsOpen();
                audio.Close();
                if (fault) {
                    Require(stopped, "stream/queue error must stop the worker");
                    Require(queued == 0, "failed conversion must not queue data");
                } else {
                    Require(throttled, "worker must throttle a full device queue");
                    Require(queued == Uint32(frequency * 4 * 40 / 1000), "queue must contain 40 ms at the obtained rate");
                }
            }
        }
        std::puts("PASS: SDL queue duration at 44.1/48/96/192 kHz and conversion/queue failures");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}
