#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef CreateWindow
#undef DestroyWindow
typedef HMODULE dynlib_t;
#define DYNLIB_OPEN(path) LoadLibraryA(path)
#define DYNLIB_SYM(handle, name) GetProcAddress((HMODULE)(handle), name)
#define DYNLIB_CLOSE(handle) FreeLibrary((HMODULE)(handle))
#define DYNLIB_ERROR() "LoadLibrary failed"
#else
#include <dlfcn.h>
typedef void* dynlib_t;
#define DYNLIB_OPEN(path) dlopen(path, RTLD_NOW | RTLD_GLOBAL)
#define DYNLIB_SYM(handle, name) dlsym(handle, name)
#define DYNLIB_CLOSE(handle) dlclose(handle)
#define DYNLIB_ERROR() dlerror()
#endif

#include "topgear2.h"
#include "topgear2_menu.h"
#include "topgear2_platform.h"
#include "topgear2_sha256.h"

/* SDL2 minimal definitions for dynload */
#define SDL_INIT_TIMER          0x00000001u
#define SDL_INIT_AUDIO          0x00000010u
#define SDL_INIT_VIDEO          0x00000020u
#define SDL_INIT_JOYSTICK       0x00000200u
#define SDL_INIT_GAMECONTROLLER 0x00002000u

#define SDL_WINDOWPOS_CENTERED        0x2FFF0000u
#define SDL_WINDOW_SHOWN              0x00000004u
#define SDL_WINDOW_RESIZABLE          0x00000020u
#define SDL_WINDOW_ALLOW_HIGHDPI      0x00002000u
#define SDL_WINDOW_FULLSCREEN_DESKTOP 0x00001001u

#define SDL_RENDERER_SOFTWARE      0x00000001u
#define SDL_RENDERER_ACCELERATED   0x00000002u
#define SDL_RENDERER_PRESENTVSYNC  0x00000004u

#define SDL_TEXTUREACCESS_STREAMING 1
#define SDL_PIXELFORMAT_ARGB8888    0x16362004u

#define AUDIO_S16SYS 0x8010u

#define SDL_QUIT                     0x100u
#define SDL_KEYDOWN                  0x300u
#define SDL_KEYUP                    0x301u
#define SDL_MOUSEMOTION              0x400u
#define SDL_MOUSEBUTTONDOWN          0x401u
#define SDL_MOUSEBUTTONUP            0x402u
#define SDL_JOYDEVICEADDED           0x605u
#define SDL_JOYDEVICEREMOVED         0x606u
#define SDL_CONTROLLERAXISMOTION     0x650u
#define SDL_CONTROLLERBUTTONDOWN     0x651u
#define SDL_CONTROLLERBUTTONUP       0x652u
#define SDL_CONTROLLERDEVICEADDED    0x653u
#define SDL_CONTROLLERDEVICEREMOVED  0x654u
#define SDL_CONTROLLERDEVICEREMAPPED 0x655u
#define SDL_DROPFILE                 0x1000u

#define SDL_SCANCODE_O      18
#define SDL_SCANCODE_P      19
#define SDL_SCANCODE_R      21
#define SDL_SCANCODE_RETURN 40
#define SDL_SCANCODE_ESCAPE 41

#define SDL_SCANCODE_F1  58
#define SDL_SCANCODE_F2  59
#define SDL_SCANCODE_F3  60
#define SDL_SCANCODE_F4  61
#define SDL_SCANCODE_F5  62
#define SDL_SCANCODE_F6  63
#define SDL_SCANCODE_F7  64
#define SDL_SCANCODE_F8  65
#define SDL_SCANCODE_F11 68

#define SDL_CONTROLLER_BUTTON_GUIDE 5

typedef void SDL_Window;
typedef void SDL_Renderer;
typedef void SDL_Texture;
typedef void SDL_GameController;
typedef uint32_t SDL_AudioDeviceID;

typedef struct SDL_Rect {
    int x, y;
    int w, h;
} SDL_Rect;

typedef struct SDL_AudioSpec {
    int freq;
    uint16_t format;
    uint8_t channels;
    uint8_t silence;
    uint16_t samples;
    uint16_t padding;
    uint32_t size;
    void (*callback)(void *userdata, uint8_t *stream, int len);
    void *userdata;
} SDL_AudioSpec;

typedef struct SDL_Keysym {
    int32_t  scancode;
    int32_t  sym;
    uint16_t mod;
    uint32_t unused;
} SDL_Keysym;

typedef struct SDL_KeyboardEvent {
    uint32_t   type;
    uint32_t   timestamp;
    uint32_t   windowID;
    uint8_t    state;
    uint8_t    repeat;
    uint8_t    padding2;
    uint8_t    padding3;
    SDL_Keysym keysym;
} SDL_KeyboardEvent;

typedef struct SDL_MouseMotionEvent {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint32_t which;
    uint32_t state;
    int32_t  x;
    int32_t  y;
    int32_t  xrel;
    int32_t  yrel;
} SDL_MouseMotionEvent;

typedef struct SDL_MouseButtonEvent {
    uint32_t type;
    uint32_t timestamp;
    uint32_t windowID;
    uint32_t which;
    uint8_t  button;
    uint8_t  state;
    uint8_t  clicks;
    uint8_t  padding1;
    int32_t  x;
    int32_t  y;
} SDL_MouseButtonEvent;

typedef struct SDL_ControllerButtonEvent {
    uint32_t type;
    uint32_t timestamp;
    int32_t  which;
    uint8_t  button;
    uint8_t  state;
    uint8_t  padding1;
    uint8_t  padding2;
} SDL_ControllerButtonEvent;

typedef struct SDL_ControllerDeviceEvent {
    uint32_t type;
    uint32_t timestamp;
    int32_t which;
} SDL_ControllerDeviceEvent;

typedef struct SDL_JoyDeviceEvent {
    uint32_t type;
    uint32_t timestamp;
    int32_t which;
} SDL_JoyDeviceEvent;

typedef struct SDL_DropEvent {
    uint32_t type;
    uint32_t timestamp;
    char    *file;
    uint32_t windowID;
} SDL_DropEvent;

typedef union SDL_Event {
    uint32_t type;
    SDL_KeyboardEvent key;
    SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button;
    SDL_ControllerButtonEvent cbutton;
    SDL_ControllerDeviceEvent cdevice;
    SDL_JoyDeviceEvent jdevice;
    SDL_DropEvent drop;
    uint64_t _align;
    uint8_t padding[56];
} SDL_Event;

/* Function pointer types */
typedef int (*PFN_SDL_Init)(uint32_t flags);
typedef void (*PFN_SDL_Quit)(void);
typedef const char* (*PFN_SDL_GetError)(void);
typedef SDL_Window* (*PFN_SDL_CreateWindow)(const char *title, int x, int y, int w, int h, uint32_t flags);
typedef void (*PFN_SDL_DestroyWindow)(SDL_Window *window);
typedef SDL_Renderer* (*PFN_SDL_CreateRenderer)(SDL_Window *window, int index, uint32_t flags);
typedef void (*PFN_SDL_DestroyRenderer)(SDL_Renderer *renderer);
typedef int (*PFN_SDL_RenderSetLogicalSize)(SDL_Renderer *renderer, int w, int h);
typedef SDL_Texture* (*PFN_SDL_CreateTexture)(SDL_Renderer *renderer, uint32_t format, int access, int w, int h);
typedef void (*PFN_SDL_DestroyTexture)(SDL_Texture *texture);
typedef int (*PFN_SDL_UpdateTexture)(SDL_Texture *texture, const SDL_Rect *rect, const void *pixels, int pitch);
typedef int (*PFN_SDL_RenderClear)(SDL_Renderer *renderer);
typedef int (*PFN_SDL_RenderCopy)(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_Rect *dstrect);
typedef void (*PFN_SDL_RenderPresent)(SDL_Renderer *renderer);
typedef int (*PFN_SDL_PollEvent)(SDL_Event *event);
typedef const uint8_t* (*PFN_SDL_GetKeyboardState)(int *numkeys);
typedef int (*PFN_SDL_SetWindowFullscreen)(SDL_Window *window, uint32_t flags);
typedef void (*PFN_SDL_SetWindowSize)(SDL_Window *window, int w, int h);
typedef SDL_AudioDeviceID (*PFN_SDL_OpenAudioDevice)(const char *device, int iscapture, const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int allowed_changes);
typedef void (*PFN_SDL_CloseAudioDevice)(SDL_AudioDeviceID dev);
typedef void (*PFN_SDL_PauseAudioDevice)(SDL_AudioDeviceID dev, int pause_on);
typedef int (*PFN_SDL_QueueAudio)(SDL_AudioDeviceID dev, const void *data, uint32_t len);
typedef uint32_t (*PFN_SDL_GetQueuedAudioSize)(SDL_AudioDeviceID dev);
typedef void (*PFN_SDL_ClearQueuedAudio)(SDL_AudioDeviceID dev);
typedef uint64_t (*PFN_SDL_GetPerformanceCounter)(void);
typedef uint64_t (*PFN_SDL_GetPerformanceFrequency)(void);
typedef void (*PFN_SDL_Delay)(uint32_t ms);
typedef int (*PFN_SDL_NumJoysticks)(void);
typedef int (*PFN_SDL_IsGameController)(int joystick_index);
typedef SDL_GameController* (*PFN_SDL_GameControllerOpen)(int joystick_index);
typedef void (*PFN_SDL_GameControllerClose)(SDL_GameController *gamecontroller);
typedef uint8_t (*PFN_SDL_GameControllerGetButton)(SDL_GameController *gamecontroller, int button);
typedef int16_t (*PFN_SDL_GameControllerGetAxis)(SDL_GameController *gamecontroller, int axis);
typedef SDL_GameController* (*PFN_SDL_GameControllerFromInstanceID)(int32_t joyid);
typedef int (*PFN_SDL_GameControllerGetAttached)(SDL_GameController *gamecontroller);
typedef const char* (*PFN_SDL_GameControllerName)(SDL_GameController *gamecontroller);
typedef void* (*PFN_SDL_RWFromFile)(const char *file, const char *mode);
typedef int (*PFN_SDL_GameControllerAddMappingsFromRW)(void *rw, int freerw);
typedef int (*PFN_SDL_GameControllerRumble)(SDL_GameController *gamecontroller, uint16_t low_frequency_rumble, uint16_t high_frequency_rumble, uint32_t duration_ms);
typedef int (*PFN_SDL_SetHint)(const char *name, const char *value);
typedef int (*PFN_SDL_ShowCursor)(int toggle);
typedef void (*PFN_SDL_free)(void *mem);

static struct {
    dynlib_t handle;
    PFN_SDL_Init Init;
    PFN_SDL_Quit Quit;
    PFN_SDL_GetError GetError;
    PFN_SDL_CreateWindow CreateWindow;
    PFN_SDL_DestroyWindow DestroyWindow;
    PFN_SDL_CreateRenderer CreateRenderer;
    PFN_SDL_DestroyRenderer DestroyRenderer;
    PFN_SDL_RenderSetLogicalSize RenderSetLogicalSize;
    PFN_SDL_CreateTexture CreateTexture;
    PFN_SDL_DestroyTexture DestroyTexture;
    PFN_SDL_UpdateTexture UpdateTexture;
    PFN_SDL_RenderClear RenderClear;
    PFN_SDL_RenderCopy RenderCopy;
    PFN_SDL_RenderPresent RenderPresent;
    PFN_SDL_PollEvent PollEvent;
    PFN_SDL_GetKeyboardState GetKeyboardState;
    PFN_SDL_SetWindowFullscreen SetWindowFullscreen;
    PFN_SDL_SetWindowSize SetWindowSize;
    PFN_SDL_OpenAudioDevice OpenAudioDevice;
    PFN_SDL_CloseAudioDevice CloseAudioDevice;
    PFN_SDL_PauseAudioDevice PauseAudioDevice;
    PFN_SDL_QueueAudio QueueAudio;
    PFN_SDL_GetQueuedAudioSize GetQueuedAudioSize;
    PFN_SDL_ClearQueuedAudio ClearQueuedAudio;
    PFN_SDL_GetPerformanceCounter GetPerformanceCounter;
    PFN_SDL_GetPerformanceFrequency GetPerformanceFrequency;
    PFN_SDL_Delay Delay;
    PFN_SDL_NumJoysticks NumJoysticks;
    PFN_SDL_IsGameController IsGameController;
    PFN_SDL_GameControllerOpen GameControllerOpen;
    PFN_SDL_GameControllerClose GameControllerClose;
    PFN_SDL_GameControllerGetButton GameControllerGetButton;
    PFN_SDL_GameControllerGetAxis GameControllerGetAxis;
    PFN_SDL_GameControllerFromInstanceID GameControllerFromInstanceID;
    PFN_SDL_GameControllerGetAttached GameControllerGetAttached;
    PFN_SDL_GameControllerName GameControllerName;
    PFN_SDL_RWFromFile RWFromFile;
    PFN_SDL_GameControllerAddMappingsFromRW GameControllerAddMappingsFromRW;
    PFN_SDL_GameControllerRumble GameControllerRumble;
    PFN_SDL_SetHint SetHint;
    PFN_SDL_ShowCursor ShowCursor;
    PFN_SDL_free sdl_free;
} sdl;

static bool load_sdl2(const char *exe_dir) {
    char exe_sdl[1024];
#ifdef _WIN32
    tg2_join_path(exe_sdl, sizeof(exe_sdl), exe_dir, "SDL2.dll");
    const char *paths[] = {
        exe_sdl,
        "SDL2.dll",
        "bin/SDL2.dll",
        NULL
    };
#elif defined(__APPLE__)
    tg2_join_path(exe_sdl, sizeof(exe_sdl), exe_dir, "libSDL2-2.0.0.dylib");
    const char *paths[] = {
        exe_sdl,
        "/opt/homebrew/lib/libSDL2-2.0.0.dylib",
        "/usr/local/lib/libSDL2-2.0.0.dylib",
        "libSDL2-2.0.0.dylib",
        "libSDL2.dylib",
        NULL
    };
#else
    tg2_join_path(exe_sdl, sizeof(exe_sdl), exe_dir, "libSDL2-2.0.so.0");
    const char *paths[] = {
        exe_sdl,
        "/usr/lib64/libSDL2-2.0.so.0",
        "/usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0",
        "/usr/lib/libSDL2-2.0.so.0",
        "libSDL2-2.0.so.0",
        "libSDL2.so",
        NULL
    };
#endif

    for (int i = 0; paths[i]; i++) {
        sdl.handle = DYNLIB_OPEN(paths[i]);
        if (sdl.handle) break;
    }

    if (!sdl.handle) {
        fprintf(stderr, "Could not load SDL2 dynamic library: %s\n", DYNLIB_ERROR());
        return false;
    }

#define LOAD_SYM(name) do { \
        sdl.name = (PFN_SDL_##name)(uintptr_t)DYNLIB_SYM(sdl.handle, "SDL_" #name); \
        if (!sdl.name) { \
            fprintf(stderr, "Missing SDL symbol: SDL_%s\n", #name); \
            return false; \
        } \
    } while (0)

#define LOAD_SYM_OPT(name) do { \
        sdl.name = (PFN_SDL_##name)(uintptr_t)DYNLIB_SYM(sdl.handle, "SDL_" #name); \
    } while (0)

    LOAD_SYM(Init);
    LOAD_SYM(Quit);
    LOAD_SYM(GetError);
    LOAD_SYM(CreateWindow);
    LOAD_SYM(DestroyWindow);
    LOAD_SYM(CreateRenderer);
    LOAD_SYM(DestroyRenderer);
    LOAD_SYM(RenderSetLogicalSize);
    LOAD_SYM(CreateTexture);
    LOAD_SYM(DestroyTexture);
    LOAD_SYM(UpdateTexture);
    LOAD_SYM(RenderClear);
    LOAD_SYM(RenderCopy);
    LOAD_SYM(RenderPresent);
    LOAD_SYM(PollEvent);
    LOAD_SYM(GetKeyboardState);
    LOAD_SYM(SetWindowFullscreen);
    LOAD_SYM_OPT(SetWindowSize);
    LOAD_SYM(OpenAudioDevice);
    LOAD_SYM(CloseAudioDevice);
    LOAD_SYM(PauseAudioDevice);
    LOAD_SYM(QueueAudio);
    LOAD_SYM(GetQueuedAudioSize);
    LOAD_SYM(ClearQueuedAudio);
    LOAD_SYM(GetPerformanceCounter);
    LOAD_SYM(GetPerformanceFrequency);
    LOAD_SYM(Delay);
    LOAD_SYM(NumJoysticks);
    LOAD_SYM(IsGameController);
    LOAD_SYM(GameControllerOpen);
    LOAD_SYM(GameControllerClose);
    LOAD_SYM(GameControllerGetButton);
    LOAD_SYM(GameControllerGetAxis);

    LOAD_SYM_OPT(GameControllerFromInstanceID);
    LOAD_SYM_OPT(GameControllerGetAttached);
    LOAD_SYM_OPT(GameControllerName);
    LOAD_SYM_OPT(RWFromFile);
    LOAD_SYM_OPT(GameControllerAddMappingsFromRW);
    LOAD_SYM_OPT(GameControllerRumble);
    LOAD_SYM_OPT(SetHint);
    LOAD_SYM_OPT(ShowCursor);
    sdl.sdl_free = (PFN_SDL_free)(uintptr_t)DYNLIB_SYM(sdl.handle, "SDL_free");

#undef LOAD_SYM
#undef LOAD_SYM_OPT
    return true;
}

#ifdef _WIN32
static void enable_windows_high_dpi(void) {
    HMODULE u32 = GetModuleHandleA("user32.dll");
    if (u32) {
        typedef BOOL (WINAPI *PFN_SetProcessDpiAwarenessContext)(void*);
        PFN_SetProcessDpiAwarenessContext set_ctx = (PFN_SetProcessDpiAwarenessContext)(uintptr_t)GetProcAddress(u32, "SetProcessDpiAwarenessContext");
        if (set_ctx) {
            set_ctx((void*)(intptr_t)-4);
        } else {
            typedef BOOL (WINAPI *PFN_SetProcessDPIAware)(void);
            PFN_SetProcessDPIAware set_aware = (PFN_SetProcessDPIAware)(uintptr_t)GetProcAddress(u32, "SetProcessDPIAware");
            if (set_aware) set_aware();
        }
    }
}
#endif

static void format_rom_error(Tg2RomStatus st, char *out, size_t cap) {
    switch (st) {
        case TG2_ROM_ERR_IO:
            snprintf(out, cap, "Erro ao abrir arquivo ROM.");
            break;
        case TG2_ROM_ERR_SIZE:
            snprintf(out, cap, "Tamanho invalido (esperado 1 MB / 1048576 bytes).");
            break;
        case TG2_ROM_ERR_HASH:
            snprintf(out, cap, "ROM invalida (use Top Gear 2 USA SHA-256 76b2702c...).");
            break;
        case TG2_ROM_ERR_ALLOC:
            snprintf(out, cap, "Erro de memoria ao carregar ROM.");
            break;
        default:
            snprintf(out, cap, "Falha ao verificar ROM.");
            break;
    }
}

static bool recreate_sdl_texture(SDL_Renderer *renderer, SDL_Texture **texture,
                                 int new_w, int new_h, int *cur_w, int *cur_h) {
    if (!renderer || !texture || new_w <= 0 || new_h <= 0) return false;
    SDL_Texture *next = sdl.CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_STREAMING, new_w, new_h);
    if (!next) {
        fprintf(stderr, "SDL_CreateTexture failed (%dx%d): %s\n", new_w, new_h, sdl.GetError());
        return false;
    }
    if (*texture) sdl.DestroyTexture(*texture);
    *texture = next;
    *cur_w = new_w;
    *cur_h = new_h;
    sdl.RenderSetLogicalSize(renderer, new_w, new_h);
    return true;
}

static bool is_scancode_bound(const Tg2MenuState *menu, int sc) {
    if (!menu || sc < 0) return false;
    for (int i = 0; i < TG2_ACTION_COUNT; i++) {
        if (menu->keys[i].scancode == sc || menu->keys[i].alt_scancode == sc) return true;
    }
    return false;
}

static uint16_t poll_controller_input(SDL_GameController *pad, const Tg2MenuState *menu, bool use_keyboard) {
    uint16_t input = 0;

    if (use_keyboard && menu) {
        const uint8_t *keys = sdl.GetKeyboardState(NULL);
        input |= tg2_menu_poll_keys(menu, keys);
    }

    if (pad && (!sdl.GameControllerGetAttached || sdl.GameControllerGetAttached(pad))) {
        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_DPAD_UP))    input |= TG2_KEY_UP;
        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_DPAD_DOWN))  input |= TG2_KEY_DOWN;
        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_DPAD_LEFT))  input |= TG2_KEY_LEFT;
        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_DPAD_RIGHT)) input |= TG2_KEY_RIGHT;

        int16_t ax = sdl.GameControllerGetAxis(pad, 0);
        int16_t ay = sdl.GameControllerGetAxis(pad, 1);
        if (ax < -16000) input |= TG2_KEY_LEFT;
        if (ax >  16000) input |= TG2_KEY_RIGHT;
        if (ay < -16000) input |= TG2_KEY_UP;
        if (ay >  16000) input |= TG2_KEY_DOWN;

        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_X)) input |= TG2_KEY_Y;
        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_A)) input |= TG2_KEY_B;
        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_B)) input |= TG2_KEY_A;
        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_Y)) input |= TG2_KEY_X;

        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_LEFTSHOULDER))  input |= TG2_KEY_L;
        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_RIGHTSHOULDER)) input |= TG2_KEY_R;

        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_START)) input |= TG2_KEY_START;
        if (sdl.GameControllerGetButton(pad, TG2_PAD_BTN_BACK))  input |= TG2_KEY_SELECT;
    }

    return input;
}

static uint16_t scale_rumble_amp(uint16_t base, int intensity) {
    uint32_t v = base;
    if (intensity == 0) v = base / 2u;
    else if (intensity == 2) v = (base * 3u) / 2u;
    return (v > 0xFFFFu) ? 0xFFFFu : (uint16_t)v;
}

static void update_gamepad_rumble(SDL_GameController *pad, TopGear2Recomp *tg2,
                                  uint16_t p1_input, uint16_t *last_speed,
                                  const Tg2MenuState *menu) {
    if (!pad || !sdl.GameControllerRumble || !menu || !menu->rumble_enabled) return;

    const uint8_t *ppu = tg2_get_ppu_registers(tg2);
    if (!ppu || ppu[0x07] != 0x78u || ppu[0x08] != 0x71u || (ppu[0x00] & 0x80u) != 0u) {
        *last_speed = 0u;
        return;
    }

    const uint8_t *wram = tg2_get_wram(tg2);
    if (!wram) return;

    uint16_t speed = (uint16_t)(wram[0x04F0] | ((uint16_t)wram[0x04F1] << 8));
    uint16_t lateral_x = (uint16_t)(wram[0x0028] | ((uint16_t)wram[0x0029] << 8));
    int intensity = menu->rumble_intensity;

    if (*last_speed > speed + 12u && speed > 10u) {
        uint16_t amp = scale_rumble_amp(0xFFFFu, intensity);
        sdl.GameControllerRumble(pad, amp, amp, 250u);
    } else if ((p1_input & TG2_KEY_A) && speed > 50u) {
        uint16_t amp = scale_rumble_amp(0x6000u, intensity);
        sdl.GameControllerRumble(pad, amp, amp, 100u);
    } else if (speed > 25u && (lateral_x < 0x2000u || lateral_x > 0xE000u)) {
        sdl.GameControllerRumble(pad, scale_rumble_amp(0x3000u, intensity),
                                 scale_rumble_amp(0x7000u, intensity), 80u);
    }

    *last_speed = speed;
}

static void open_controller_slot(int index, SDL_GameController **pad1, SDL_GameController **pad2) {
    if (!sdl.IsGameController(index)) return;
    if (*pad1 && *pad2) return;

    SDL_GameController *c = sdl.GameControllerOpen(index);
    if (!c) return;
    if (c == *pad1 || c == *pad2) {
        sdl.GameControllerClose(c);
        return;
    }

    const char *name = sdl.GameControllerName ? sdl.GameControllerName(c) : "Controller";
    if (!*pad1) {
        *pad1 = c;
        printf("Gamepad P1 connected: %s (slot %d)\n", name, index);
    } else if (!*pad2) {
        *pad2 = c;
        printf("Gamepad P2 connected: %s (slot %d)\n", name, index);
    }
}

static void handle_controller_removed(int32_t instance_id, SDL_GameController **pad1, SDL_GameController **pad2) {
    if (sdl.GameControllerFromInstanceID) {
        SDL_GameController *c = sdl.GameControllerFromInstanceID(instance_id);
        if (c && c == *pad1) {
            printf("Gamepad P1 disconnected.\n");
            sdl.GameControllerClose(*pad1);
            *pad1 = NULL;
            return;
        }
        if (c && c == *pad2) {
            printf("Gamepad P2 disconnected.\n");
            sdl.GameControllerClose(*pad2);
            *pad2 = NULL;
            return;
        }
    }
    if (*pad1 && sdl.GameControllerGetAttached && !sdl.GameControllerGetAttached(*pad1)) {
        printf("Gamepad P1 disconnected.\n");
        sdl.GameControllerClose(*pad1);
        *pad1 = NULL;
    }
    if (*pad2 && sdl.GameControllerGetAttached && !sdl.GameControllerGetAttached(*pad2)) {
        printf("Gamepad P2 disconnected.\n");
        sdl.GameControllerClose(*pad2);
        *pad2 = NULL;
    }
}

typedef struct Tg2RunnerSync {
    TopGear2Recomp *tg2;
    Tg2MenuState *menu;
    SDL_Window *window;
    SDL_AudioDeviceID audio_dev;
    const char *config_path;
    const char *quicksave_path;
    bool *fullscreen;
    int *cur_visual_mode;
    int *cur_aspect_mode;
    int *cur_aspect_w;
    int cur_aspect_h;
    int *cur_window_scale;
    int *cur_p1_paint;
    int *cur_p2_paint;
    bool *career_synced;
    uint16_t *last_circuit;
    uint16_t *last_speed;
    bool *wait_input_release;
    bool *recreate_texture;
    uint64_t *next_frame_tick;
} Tg2RunnerSync;

static void sync_menu_changes(Tg2RunnerSync *s, bool prev_active) {
    Tg2MenuState *m = s->menu;

    if (*s->cur_visual_mode != m->visual_mode) {
        *s->cur_visual_mode = m->visual_mode;
        tg2_set_visual_mode(s->tg2, (Tg2VisualMode)*s->cur_visual_mode);
        if (sdl.SetHint) {
            sdl.SetHint("SDL_RENDER_SCALE_QUALITY", (*s->cur_visual_mode == 0) ? "0" : "1");
        }
        *s->recreate_texture = true;
    }

    if (*s->cur_aspect_mode != m->aspect_mode) {
        *s->cur_aspect_mode = m->aspect_mode;
        tg2_set_aspect_mode(s->tg2, (Tg2AspectMode)*s->cur_aspect_mode);
        *s->cur_aspect_w = (*s->cur_aspect_mode == TG2_ASPECT_16_9) ? TOPGEAR2_FRAME_WIDTH_16_9 : TOPGEAR2_FRAME_WIDTH_4_3;
        if (!*s->fullscreen && sdl.SetWindowSize) {
            sdl.SetWindowSize(s->window, (*s->cur_aspect_w) * m->window_scale, s->cur_aspect_h * m->window_scale);
        }
        *s->recreate_texture = true;
    }

    if (m->fullscreen != *s->fullscreen) {
        *s->fullscreen = m->fullscreen;
        sdl.SetWindowFullscreen(s->window, *s->fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
        if (!*s->fullscreen && sdl.SetWindowSize) {
            sdl.SetWindowSize(s->window, (*s->cur_aspect_w) * m->window_scale, s->cur_aspect_h * m->window_scale);
        }
    }

    if (*s->cur_window_scale != m->window_scale) {
        *s->cur_window_scale = m->window_scale;
        if (!*s->fullscreen && sdl.SetWindowSize) {
            sdl.SetWindowSize(s->window, (*s->cur_aspect_w) * (*s->cur_window_scale), s->cur_aspect_h * (*s->cur_window_scale));
        }
    }

    if (*s->cur_p1_paint != m->p1_paint || m->p1_paint == 7) {
        *s->cur_p1_paint = m->p1_paint;
        tg2_set_car_paint(s->tg2, 1, *s->cur_p1_paint, m->p1_rgb[0], m->p1_rgb[1], m->p1_rgb[2]);
    }
    if (*s->cur_p2_paint != m->p2_paint || m->p2_paint == 7) {
        *s->cur_p2_paint = m->p2_paint;
        tg2_set_car_paint(s->tg2, 2, *s->cur_p2_paint, m->p2_rgb[0], m->p2_rgb[1], m->p2_rgb[2]);
    }

    switch (m->pending_action) {
        case TG2_MENU_ACT_APPLY_GAME:
            tg2_menu_apply_game_settings(m, s->tg2);
            break;
        case TG2_MENU_ACT_QUICK_SAVE:
            tg2_menu_apply_game_settings(m, s->tg2);
            if (tg2_save_state_file(s->tg2, s->quicksave_path))
                printf("Quick Save State created (%s).\n", s->quicksave_path);
            break;
        case TG2_MENU_ACT_QUICK_LOAD:
            if (tg2_load_state_file(s->tg2, s->quicksave_path)) {
                tg2_menu_apply_game_settings(m, s->tg2);
                if (s->audio_dev && sdl.ClearQueuedAudio) sdl.ClearQueuedAudio(s->audio_dev);
                printf("Quick Save State restored (%s).\n", s->quicksave_path);
            }
            break;
        case TG2_MENU_ACT_RESET_GAME:
            tg2_reset(s->tg2);
            *s->career_synced = false;
            *s->last_circuit = 0xFFFFu;
            *s->last_speed = 0u;
            m->active = false;
            if (s->audio_dev && sdl.ClearQueuedAudio) sdl.ClearQueuedAudio(s->audio_dev);
            printf("Game reset.\n");
            break;
        case TG2_MENU_ACT_SELECT_ROM: {
            char picked[512];
            if (tg2_open_rom_dialog(picked, sizeof(picked))) {
                char sha[65];
                Tg2RomStatus st = tg2_load_rom_file_checked(s->tg2, picked, sha);
                if (st == TG2_ROM_OK) {
                    snprintf(m->rom_path, sizeof(m->rom_path), "%s", picked);
                    tg2_reset(s->tg2);
                    *s->career_synced = false;
                    *s->last_circuit = 0xFFFFu;
                    *s->last_speed = 0u;
                    m->active = false;
                    if (s->audio_dev && sdl.ClearQueuedAudio) sdl.ClearQueuedAudio(s->audio_dev);
                    tg2_menu_save_config(m, s->config_path);
                    printf("Switched ROM to: %s\n", picked);
                } else {
                    char err[128];
                    format_rom_error(st, err, sizeof(err));
                    fprintf(stderr, "Rejected ROM '%s': %s\n", picked, err);
                }
            }
            break;
        }
        default:
            break;
    }
    m->pending_action = TG2_MENU_ACT_NONE;

    bool now_active = tg2_menu_is_active(m);
    if (now_active != prev_active && sdl.ShowCursor) {
        sdl.ShowCursor(now_active ? 1 : 0);
    }

    if (prev_active && !now_active) {
        tg2_menu_apply_game_settings(m, s->tg2);
        *s->wait_input_release = true;
        *s->recreate_texture = true;
        if (s->audio_dev && sdl.ClearQueuedAudio) sdl.ClearQueuedAudio(s->audio_dev);
        *s->next_frame_tick = sdl.GetPerformanceCounter();
        tg2_menu_save_config(m, s->config_path);
        printf("Settings saved to %s\n", s->config_path);
    }
}

static void print_usage(const char *prog) {
    printf("Top Gear 2: Definitive Edition (Native Static Recompilation Port)\n");
    printf("Usage: %s [options] [rom_path]\n\n", prog ? prog : "topgear2");
    printf("Options:\n");
    printf("  --rom <path>       Load a specific Top Gear 2 (USA) ROM (.sfc/.smc)\n");
    printf("  --portable         Store config/saves next to the executable\n");
    printf("  --fullscreen, -f   Start in desktop fullscreen mode\n");
    printf("  --scale <1-4>      Window integer scale factor (default: 2)\n");
    printf("  --1080p / --1440p / --4k  Target window resolution\n");
    printf("  --no-autoload      Skip automatic loading of savegame.bin\n");
    printf("  --headless         Run without SDL window/audio\n");
    printf("  --frames <N>       Frame count for headless mode (default: 300)\n");
    printf("  --version, -v      Show version information\n");
    printf("  --help, -h         Show this help text\n\n");
    printf("Required ROM: Top Gear 2 (USA), 1 MB LoROM, SHA-256 %s\n", TOPGEAR2_USA_ROM_SHA256);
}

int main(int argc, char **argv) {
    const char *cli_rom_path = NULL;
    bool headless = false;
    uint64_t target_frames = 0;
    int window_scale = 2;
    bool scale_explicit = false;
    bool fullscreen = false;
    bool no_autoload = false;
    bool portable_mode = false;
    bool mode_4k = false;
    bool mode_1440p = false;
    bool mode_1080p = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Top Gear 2: Definitive Edition (Native Static Recompilation Port) v2.0\n");
            return 0;
        } else if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            cli_rom_path = argv[++i];
        } else if (strcmp(argv[i], "--portable") == 0) {
            portable_mode = true;
        } else if (strcmp(argv[i], "--headless") == 0) {
            headless = true;
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            target_frames = strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--fullscreen") == 0 || strcmp(argv[i], "-f") == 0) {
            fullscreen = true;
        } else if (strcmp(argv[i], "--4k") == 0 || strcmp(argv[i], "-4k") == 0) {
            mode_4k = true;
        } else if (strcmp(argv[i], "--1440p") == 0 || strcmp(argv[i], "--2k") == 0) {
            mode_1440p = true;
        } else if (strcmp(argv[i], "--1080p") == 0) {
            mode_1080p = true;
        } else if (strcmp(argv[i], "--no-autoload") == 0) {
            no_autoload = true;
        } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            window_scale = atoi(argv[++i]);
            if (window_scale < 1) window_scale = 1;
            if (window_scale > 4) window_scale = 4;
            scale_explicit = true;
        } else if (argv[i][0] != '-') {
            cli_rom_path = argv[i];
        }
    }

#ifdef _WIN32
    enable_windows_high_dpi();
#endif

#if defined(__linux__)
    if (!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY")) {
        headless = true;
    }
#endif

    char exe_dir[1024], data_dir[1024];
    tg2_get_exe_dir(exe_dir, sizeof(exe_dir));
    tg2_get_data_dir(data_dir, sizeof(data_dir), portable_mode);

    char config_path[1024], autosave_path[1024], quicksave_path[1024], savejson_path[1024], controllerdb_path[1024];
    tg2_join_path(config_path, sizeof(config_path), data_dir, "tg2_config.json");
    tg2_join_path(autosave_path, sizeof(autosave_path), data_dir, "savegame.bin");
    tg2_join_path(quicksave_path, sizeof(quicksave_path), data_dir, "quicksave.bin");
    tg2_join_path(savejson_path, sizeof(savejson_path), data_dir, "savegame.json");
    tg2_join_path(controllerdb_path, sizeof(controllerdb_path), exe_dir, "gamecontrollerdb.txt");

    Tg2MenuState menu;
    tg2_menu_init(&menu);
    tg2_menu_load_config(&menu, config_path);
    if (fullscreen) menu.fullscreen = true;
    if (scale_explicit) menu.window_scale = window_scale;

    printf("=================================================================\n");
    printf("   Top Gear 2 (SNES) - Definitive Edition (100%% Native Port)     \n");
    printf("=================================================================\n");
    printf("Data Dir: %s\n", data_dir);
    printf("Mode:     %s\n", headless ? "Headless / Benchmark" : "Interactive Desktop");

    TopGear2Recomp *tg2 = tg2_create();
    if (!tg2) {
        fprintf(stderr, "Error: Failed to instantiate TopGear2Recomp.\n");
        return 1;
    }

    /* ROM discovery & SHA-256 verification */
    char resolved_rom[512];
    resolved_rom[0] = '\0';
    char startup_err[128];
    startup_err[0] = '\0';
    char sha_hex[65];
    bool rom_loaded = false;

    const char *env_rom = getenv("TG2_ROM_PATH");
    if (cli_rom_path && cli_rom_path[0]) {
        Tg2RomStatus st = tg2_load_rom_file_checked(tg2, cli_rom_path, sha_hex);
        if (st == TG2_ROM_OK) {
            snprintf(resolved_rom, sizeof(resolved_rom), "%s", cli_rom_path);
            rom_loaded = true;
        } else {
            format_rom_error(st, startup_err, sizeof(startup_err));
            fprintf(stderr, "ROM '%s': %s\n", cli_rom_path, startup_err);
        }
    } else if (env_rom && env_rom[0]) {
        Tg2RomStatus st = tg2_load_rom_file_checked(tg2, env_rom, sha_hex);
        if (st == TG2_ROM_OK) {
            snprintf(resolved_rom, sizeof(resolved_rom), "%s", env_rom);
            rom_loaded = true;
        } else {
            format_rom_error(st, startup_err, sizeof(startup_err));
            fprintf(stderr, "TG2_ROM_PATH '%s': %s\n", env_rom, startup_err);
        }
    } else {
        if (menu.rom_path[0] && tg2_load_rom_file_checked(tg2, menu.rom_path, sha_hex) == TG2_ROM_OK) {
            snprintf(resolved_rom, sizeof(resolved_rom), "%s", menu.rom_path);
            rom_loaded = true;
        } else {
            char scanned[512];
            if (tg2_find_valid_rom_in_dir(exe_dir, scanned, sizeof(scanned)) &&
                tg2_load_rom_file_checked(tg2, scanned, sha_hex) == TG2_ROM_OK) {
                snprintf(resolved_rom, sizeof(resolved_rom), "%s", scanned);
                rom_loaded = true;
            } else if (tg2_find_valid_rom_in_dir(".", scanned, sizeof(scanned)) &&
                       tg2_load_rom_file_checked(tg2, scanned, sha_hex) == TG2_ROM_OK) {
                snprintf(resolved_rom, sizeof(resolved_rom), "%s", scanned);
                rom_loaded = true;
            }
        }
    }

    if (rom_loaded) {
        printf("ROM verified: %s (SHA-256 %s)\n", resolved_rom, sha_hex);
        if (strcmp(menu.rom_path, resolved_rom) != 0) {
            snprintf(menu.rom_path, sizeof(menu.rom_path), "%s", resolved_rom);
            tg2_menu_save_config(&menu, config_path);
        }
    } else if (headless) {
        fprintf(stderr, "Error: No valid Top Gear 2 (USA) ROM found.\n");
        if (startup_err[0]) fprintf(stderr, "Reason: %s\n", startup_err);
        fprintf(stderr, "Pass --rom <path> or set TG2_ROM_PATH (expected SHA-256: %s).\n", TOPGEAR2_USA_ROM_SHA256);
        tg2_destroy(tg2);
        return 1;
    }

    /* Headless execution */
    if (headless) {
        tg2_reset(tg2);
        printf("Cold reset complete. Starting authentic execution...\n");

        if (!no_autoload) {
            FILE *fchk = tg2_fopen_utf8(autosave_path, "rb");
            if (fchk) {
                fclose(fchk);
                if (tg2_load_state_file(tg2, autosave_path)) {
                    printf("Career save game loaded automatically from '%s'.\n", autosave_path);
                }
            }
        }

        if (target_frames == 0) target_frames = 300;
        printf("Running %llu frames headless...\n", (unsigned long long)target_frames);
        clock_t t0 = clock();

        for (uint64_t f = 1; f <= target_frames; f++) {
            if (!tg2_step_frame(tg2, 0, 0)) {
                fprintf(stderr, "Execution halted at frame %llu!\n", (unsigned long long)f);
                break;
            }
            if (f % 60 == 0 || f == target_frames) {
                printf("  Frame %4llu / %llu completed (Total insns: %llu)\n",
                       (unsigned long long)f, (unsigned long long)target_frames,
                       (unsigned long long)tg2_get_instruction_count(tg2));
            }
        }

        clock_t t1 = clock();
        double elapsed = (double)(t1 - t0) / CLOCKS_PER_SEC;
        double fps = target_frames / (elapsed > 0 ? elapsed : 0.001);
        printf("\nHeadless execution finished in %.2f seconds (%.1f FPS)\n", elapsed, fps);
        tg2_destroy(tg2);
        return 0;
    }

    /* Interactive Desktop Mode via SDL2 */
    if (!load_sdl2(exe_dir)) {
        fprintf(stderr, "Failed to load SDL2 library. Exiting.\n");
        tg2_destroy(tg2);
        return 1;
    }

    if (sdl.Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", sdl.GetError());
        tg2_destroy(tg2);
        return 1;
    }

    if (sdl.RWFromFile && sdl.GameControllerAddMappingsFromRW) {
        void *rw = sdl.RWFromFile(controllerdb_path, "rb");
        if (!rw) rw = sdl.RWFromFile("gamecontrollerdb.txt", "rb");
        if (rw) {
            int mappings = sdl.GameControllerAddMappingsFromRW(rw, 1);
            if (mappings > 0) {
                printf("Loaded %d gamepad mapping definitions\n", mappings);
            }
        }
    }

    int cur_visual_mode = menu.visual_mode;
    int cur_aspect_mode = menu.aspect_mode;
    int cur_window_scale = menu.window_scale;
    int cur_p1_paint = menu.p1_paint;
    int cur_p2_paint = menu.p2_paint;

    tg2_set_aspect_mode(tg2, (Tg2AspectMode)cur_aspect_mode);
    tg2_set_visual_mode(tg2, (Tg2VisualMode)cur_visual_mode);
    tg2_set_car_paint(tg2, 1, cur_p1_paint, menu.p1_rgb[0], menu.p1_rgb[1], menu.p1_rgb[2]);
    tg2_set_car_paint(tg2, 2, cur_p2_paint, menu.p2_rgb[0], menu.p2_rgb[1], menu.p2_rgb[2]);

    int cur_aspect_w = (cur_aspect_mode == TG2_ASPECT_16_9) ? TOPGEAR2_FRAME_WIDTH_16_9 : TOPGEAR2_FRAME_WIDTH_4_3;
    int cur_aspect_h = TOPGEAR2_FRAME_HEIGHT;
    int win_w = cur_aspect_w * cur_window_scale;
    int win_h = cur_aspect_h * cur_window_scale;

    if (mode_4k) {
        win_w = 3840;
        win_h = 2160;
    } else if (mode_1440p) {
        win_w = 2560;
        win_h = 1440;
    } else if (mode_1080p) {
        win_w = 1920;
        win_h = 1080;
    }

    if (sdl.SetHint) {
        sdl.SetHint("SDL_RENDER_SCALE_QUALITY", (cur_visual_mode == 0) ? "0" : "1");
    }

    SDL_Window *window = sdl.CreateWindow(
        "Top Gear 2: Definitive Edition (16:9 HD Native Port)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", sdl.GetError());
        sdl.Quit();
        tg2_destroy(tg2);
        return 1;
    }

    fullscreen = menu.fullscreen;
    if (fullscreen) {
        sdl.SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    SDL_Renderer *renderer = sdl.CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = sdl.CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", sdl.GetError());
        sdl.DestroyWindow(window);
        sdl.Quit();
        tg2_destroy(tg2);
        return 1;
    }

    int tex_w = 0;
    int tex_h = 0;
    SDL_Texture *texture = NULL;
    if (!recreate_sdl_texture(renderer, &texture, TOPGEAR2_MAX_FB_WIDTH, TOPGEAR2_MAX_FB_HEIGHT, &tex_w, &tex_h)) {
        sdl.DestroyRenderer(renderer);
        sdl.DestroyWindow(window);
        sdl.Quit();
        tg2_destroy(tg2);
        return 1;
    }

    static uint32_t argb_hd[TOPGEAR2_MAX_FB_WIDTH * TOPGEAR2_MAX_FB_HEIGHT];
    static uint32_t overlay_buf[TOPGEAR2_MAX_FB_WIDTH * TOPGEAR2_MAX_FB_HEIGHT];

    /* Interactive ROM selection prompt if no valid ROM was located on startup */
    if (!rom_loaded) {
        bool prompt_hover = false;
        if (sdl.ShowCursor) sdl.ShowCursor(1);
        while (!rom_loaded) {
            SDL_Event ev;
            while (sdl.PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) {
                    sdl.DestroyTexture(texture);
                    sdl.DestroyRenderer(renderer);
                    sdl.DestroyWindow(window);
                    sdl.Quit();
                    DYNLIB_CLOSE(sdl.handle);
                    tg2_destroy(tg2);
                    return 0;
                } else if (ev.type == SDL_MOUSEMOTION) {
                    prompt_hover = tg2_menu_rom_prompt_hit_button(ev.motion.x, ev.motion.y, tex_w, tex_h);
                } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == 1 &&
                           tg2_menu_rom_prompt_hit_button(ev.button.x, ev.button.y, tex_w, tex_h)) {
                    char picked[512];
                    if (tg2_open_rom_dialog(picked, sizeof(picked))) {
                        Tg2RomStatus st = tg2_load_rom_file_checked(tg2, picked, sha_hex);
                        if (st == TG2_ROM_OK) {
                            snprintf(menu.rom_path, sizeof(menu.rom_path), "%s", picked);
                            tg2_menu_save_config(&menu, config_path);
                            rom_loaded = true;
                            printf("ROM verified: %s (SHA-256 %s)\n", picked, sha_hex);
                        } else {
                            format_rom_error(st, startup_err, sizeof(startup_err));
                        }
                    }
                } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                    int sc = ev.key.keysym.scancode;
                    if (sc == SDL_SCANCODE_ESCAPE) {
                        sdl.DestroyTexture(texture);
                        sdl.DestroyRenderer(renderer);
                        sdl.DestroyWindow(window);
                        sdl.Quit();
                        DYNLIB_CLOSE(sdl.handle);
                        tg2_destroy(tg2);
                        return 0;
                    } else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_O) {
                        char picked[512];
                        if (tg2_open_rom_dialog(picked, sizeof(picked))) {
                            Tg2RomStatus st = tg2_load_rom_file_checked(tg2, picked, sha_hex);
                            if (st == TG2_ROM_OK) {
                                snprintf(menu.rom_path, sizeof(menu.rom_path), "%s", picked);
                                tg2_menu_save_config(&menu, config_path);
                                rom_loaded = true;
                                printf("ROM verified: %s (SHA-256 %s)\n", picked, sha_hex);
                            } else {
                                format_rom_error(st, startup_err, sizeof(startup_err));
                            }
                        }
                    }
                } else if (ev.type == SDL_DROPFILE && ev.drop.file) {
                    Tg2RomStatus st = tg2_load_rom_file_checked(tg2, ev.drop.file, sha_hex);
                    if (st == TG2_ROM_OK) {
                        snprintf(menu.rom_path, sizeof(menu.rom_path), "%s", ev.drop.file);
                        tg2_menu_save_config(&menu, config_path);
                        rom_loaded = true;
                        printf("ROM verified (drop): %s (SHA-256 %s)\n", ev.drop.file, sha_hex);
                    } else {
                        format_rom_error(st, startup_err, sizeof(startup_err));
                    }
                    if (sdl.sdl_free) sdl.sdl_free(ev.drop.file);
                }
            }
            tg2_menu_render_rom_prompt(overlay_buf, tex_w, tex_h, startup_err, prompt_hover);
            sdl.UpdateTexture(texture, NULL, overlay_buf, tex_w * (int)sizeof(uint32_t));
            sdl.RenderClear(renderer);
            sdl.RenderCopy(renderer, texture, NULL, NULL);
            sdl.RenderPresent(renderer);
            sdl.Delay(16);
        }
    }

    tg2_reset(tg2);
    printf("Cold reset complete. Starting authentic execution...\n");

    if (!no_autoload) {
        FILE *fchk = tg2_fopen_utf8(autosave_path, "rb");
        if (fchk) {
            fclose(fchk);
            if (tg2_load_state_file(tg2, autosave_path)) {
                printf("Career save game loaded automatically from '%s'.\n", autosave_path);
            }
        }
    }

    /* Initialize Audio */
    SDL_AudioSpec desired, obtained;
    memset(&desired, 0, sizeof(desired));
    desired.freq = TOPGEAR2_AUDIO_SAMPLE_RATE;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = NULL;

    SDL_AudioDeviceID audio_dev = sdl.OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
    if (audio_dev) {
        sdl.PauseAudioDevice(audio_dev, 0);
        printf("Audio initialized: %d Hz stereo 16-bit PCM (buffer %d samples)\n",
               obtained.freq, obtained.samples);
    }

    /* Open up to 2 Gamepads (P1 + P2) */
    SDL_GameController *pad1 = NULL;
    SDL_GameController *pad2 = NULL;
    int num_joysticks = sdl.NumJoysticks();
    for (int i = 0; i < num_joysticks; i++) {
        open_controller_slot(i, &pad1, &pad2);
    }

    printf("\n=== Definitive Edition Controls & Hotkeys ===\n");
    printf("  Arrow Keys / D-Pad:  Steer / Navigate Menus\n");
    printf("  C or A / Gamepad X:  Accelerate\n");
    printf("  Z or D / Gamepad A:  Brake\n");
    printf("  X / Gamepad B:       Nitro\n");
    printf("  S / Gamepad Y:       Horn / Gear Shift\n");
    printf("  Return / Start:      Start Game / Confirm\n");
    printf("  Space / Select:      Select / Options\n");
    printf("  F1 / F2 / F3:        Visual Filter (Pixel Puro / CRT / Cycle HD Filters)\n");
    printf("  F4:                  Toggle Aspect Ratio (16:9 Widescreen <-> 4:3)\n");
    printf("  F5 / F8:             Quick Save State / Quick Load State\n");
    printf("  F6 / F7:             Cycle Car Paint (Player 1 / Player 2)\n");
    printf("  F11:                 Toggle Fullscreen (Native Monitor Resolution / 4K)\n");
    printf("  P / R:               Pause / Reset Game\n");
    printf("  Escape / Guide:      Settings Menu (Mouse / Gamepad / Keyboard)\n\n");

    int16_t audio_buffer[4096];
    bool running = true;
    bool paused = false;
    bool wait_input_release = false;
    bool career_synced = false;
    uint64_t frame_index = 0;
    uint16_t last_speed = 0;
    uint16_t last_circuit = 0xFFFFu;

    if (sdl.ShowCursor) {
        sdl.ShowCursor(menu.active ? 1 : 0);
    }

    /* Align texture dimensions with active mode immediately */
    {
        int init_w = 0, init_h = 0;
        tg2_render_hd_frame(tg2, argb_hd, &init_w, &init_h);
        if (init_w > 0 && init_h > 0 && (init_w != tex_w || init_h != tex_h)) {
            recreate_sdl_texture(renderer, &texture, init_w, init_h, &tex_w, &tex_h);
        }
        const uint8_t *winit = tg2_get_wram(tg2);
        if (winit && winit[0x1CD7] == 0x65u && winit[0x1CD8] == 0x7Eu) {
            tg2_menu_apply_game_settings(&menu, tg2);
            career_synced = true;
        }
    }

    const double snes_fps = 60.0988;
    uint64_t perf_freq = sdl.GetPerformanceFrequency();
    double frame_ticks = (double)perf_freq / snes_fps;
    uint64_t next_frame_tick = sdl.GetPerformanceCounter();

    while (running) {
        SDL_Event event;
        bool recreate_texture = false;

        Tg2RunnerSync sync_ctx = {
            tg2, &menu, window, audio_dev,
            config_path, quicksave_path,
            &fullscreen,
            &cur_visual_mode, &cur_aspect_mode, &cur_aspect_w, cur_aspect_h,
            &cur_window_scale, &cur_p1_paint, &cur_p2_paint,
            &career_synced, &last_circuit, &last_speed,
            &wait_input_release, &recreate_texture, &next_frame_tick
        };

        while (sdl.PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_DROPFILE && event.drop.file) {
                char drop_sha[65];
                Tg2RomStatus st = tg2_load_rom_file_checked(tg2, event.drop.file, drop_sha);
                if (st == TG2_ROM_OK) {
                    snprintf(menu.rom_path, sizeof(menu.rom_path), "%s", event.drop.file);
                    tg2_reset(tg2);
                    career_synced = false;
                    last_circuit = 0xFFFFu;
                    last_speed = 0u;
                    menu.active = false;
                    paused = false;
                    if (audio_dev && sdl.ClearQueuedAudio) sdl.ClearQueuedAudio(audio_dev);
                    tg2_menu_save_config(&menu, config_path);
                    printf("Loaded dropped ROM: %s\n", event.drop.file);
                } else {
                    char err[128];
                    format_rom_error(st, err, sizeof(err));
                    fprintf(stderr, "Rejected dropped ROM '%s': %s\n", event.drop.file, err);
                }
                if (sdl.sdl_free) sdl.sdl_free(event.drop.file);
            } else if (event.type == SDL_MOUSEMOTION && tg2_menu_is_active(&menu)) {
                int mx = event.motion.x;
                int my = event.motion.y;
                if (mx < 0) mx = 0;
                if (mx >= tex_w) mx = tex_w - 1;
                if (my < 0) my = 0;
                if (my >= tex_h) my = tex_h - 1;
                bool prev_active = tg2_menu_is_active(&menu);
                tg2_menu_handle_mouse(&menu, mx, my, 0, false, true, tex_w, tex_h);
                sync_menu_changes(&sync_ctx, prev_active);
            } else if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && tg2_menu_is_active(&menu)) {
                int mx = event.button.x;
                int my = event.button.y;
                if (mx < 0) mx = 0;
                if (mx >= tex_w) mx = tex_w - 1;
                if (my < 0) my = 0;
                if (my >= tex_h) my = tex_h - 1;
                bool is_down = (event.type == SDL_MOUSEBUTTONDOWN);
                bool prev_active = tg2_menu_is_active(&menu);
                tg2_menu_handle_mouse(&menu, mx, my, event.button.button, is_down, false, tex_w, tex_h);
                sync_menu_changes(&sync_ctx, prev_active);
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                int btn = event.cbutton.button;
                if (!tg2_menu_is_active(&menu)) {
                    if (btn == SDL_CONTROLLER_BUTTON_GUIDE) {
                        menu.active = true;
                        menu.blink_timer = 0;
                        if (sdl.ShowCursor) sdl.ShowCursor(1);
                    }
                } else {
                    bool prev_active = tg2_menu_is_active(&menu);
                    tg2_menu_handle_pad(&menu, btn, true);
                    sync_menu_changes(&sync_ctx, prev_active);
                }
            } else if (event.type == SDL_KEYDOWN) {
                int scancode = event.key.keysym.scancode;
                bool is_repeat = (event.key.repeat != 0);
                bool prev_active = tg2_menu_is_active(&menu);

                if (scancode == SDL_SCANCODE_ESCAPE && is_repeat) {
                    continue;
                }

                if (tg2_menu_handle_key(&menu, scancode, true)) {
                    sync_menu_changes(&sync_ctx, prev_active);
                    continue;
                }

                if (is_repeat) continue;

                switch (scancode) {
                    case SDL_SCANCODE_F11:
                        fullscreen = !fullscreen;
                        menu.fullscreen = fullscreen;
                        sdl.SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                        if (!fullscreen && sdl.SetWindowSize) {
                            sdl.SetWindowSize(window, cur_aspect_w * cur_window_scale, cur_aspect_h * cur_window_scale);
                        }
                        break;
                    case SDL_SCANCODE_P:
                        if (!is_scancode_bound(&menu, scancode)) {
                            paused = !paused;
                            if (audio_dev && sdl.PauseAudioDevice) {
                                sdl.PauseAudioDevice(audio_dev, paused ? 1 : 0);
                            }
                            if (!paused) {
                                next_frame_tick = sdl.GetPerformanceCounter();
                            }
                        }
                        break;
                    case SDL_SCANCODE_R:
                        if (!is_scancode_bound(&menu, scancode)) {
                            tg2_reset(tg2);
                            career_synced = false;
                            last_circuit = 0xFFFFu;
                            last_speed = 0;
                            if (audio_dev && sdl.ClearQueuedAudio) sdl.ClearQueuedAudio(audio_dev);
                            printf("Game reset.\n");
                        }
                        break;
                    case SDL_SCANCODE_F1:
                        tg2_set_visual_mode(tg2, TG2_VISUAL_PIXEL_PURO);
                        cur_visual_mode = 0;
                        menu.visual_mode = 0;
                        if (sdl.SetHint) sdl.SetHint("SDL_RENDER_SCALE_QUALITY", "0");
                        recreate_texture = true;
                        break;
                    case SDL_SCANCODE_F2:
                        tg2_set_visual_mode(tg2, TG2_VISUAL_CRT_ARCADE);
                        cur_visual_mode = 1;
                        menu.visual_mode = 1;
                        if (sdl.SetHint) sdl.SetHint("SDL_RENDER_SCALE_QUALITY", "1");
                        recreate_texture = true;
                        break;
                    case SDL_SCANCODE_F3:
                        cur_visual_mode = (cur_visual_mode >= 2 && cur_visual_mode < TG2_VISUAL_COUNT - 1)
                                          ? (cur_visual_mode + 1)
                                          : TG2_VISUAL_ENHANCED_SMOOTH;
                        tg2_set_visual_mode(tg2, (Tg2VisualMode)cur_visual_mode);
                        menu.visual_mode = cur_visual_mode;
                        if (sdl.SetHint) sdl.SetHint("SDL_RENDER_SCALE_QUALITY", "1");
                        recreate_texture = true;
                        break;
                    case SDL_SCANCODE_F4: {
                        Tg2AspectMode asp = tg2_get_aspect_mode(tg2);
                        asp = (asp == TG2_ASPECT_16_9) ? TG2_ASPECT_4_3 : TG2_ASPECT_16_9;
                        tg2_set_aspect_mode(tg2, asp);
                        cur_aspect_mode = (int)asp;
                        menu.aspect_mode = (int)asp;
                        cur_aspect_w = (asp == TG2_ASPECT_16_9) ? TOPGEAR2_FRAME_WIDTH_16_9 : TOPGEAR2_FRAME_WIDTH_4_3;
                        if (!fullscreen && sdl.SetWindowSize) {
                            sdl.SetWindowSize(window, cur_aspect_w * cur_window_scale, cur_aspect_h * cur_window_scale);
                        }
                        recreate_texture = true;
                        break;
                    }
                    case SDL_SCANCODE_F5:
                        tg2_menu_apply_game_settings(&menu, tg2);
                        if (tg2_save_state_file(tg2, quicksave_path))
                            printf("Quick Save State created.\n");
                        break;
                    case SDL_SCANCODE_F8:
                        if (tg2_load_state_file(tg2, quicksave_path)) {
                            tg2_menu_apply_game_settings(&menu, tg2);
                            if (audio_dev && sdl.ClearQueuedAudio) sdl.ClearQueuedAudio(audio_dev);
                            printf("Quick Save State restored.\n");
                        }
                        break;
                    case SDL_SCANCODE_F6:
                        cur_p1_paint = (cur_p1_paint + 1) % 8;
                        menu.p1_paint = cur_p1_paint;
                        tg2_set_car_paint(tg2, 1, cur_p1_paint, menu.p1_rgb[0], menu.p1_rgb[1], menu.p1_rgb[2]);
                        break;
                    case SDL_SCANCODE_F7:
                        cur_p2_paint = (cur_p2_paint + 1) % 8;
                        menu.p2_paint = cur_p2_paint;
                        tg2_set_car_paint(tg2, 2, cur_p2_paint, menu.p2_rgb[0], menu.p2_rgb[1], menu.p2_rgb[2]);
                        break;
                    default:
                        break;
                }
            } else if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_JOYDEVICEADDED) {
                open_controller_slot(event.cdevice.which, &pad1, &pad2);
            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                handle_controller_removed(event.cdevice.which, &pad1, &pad2);
            }
        }

        /* ── Menu active: re-render current frame live + menu overlay ── */
        if (tg2_menu_is_active(&menu)) {
            int out_w = 0, out_h = 0;
            tg2_render_hd_frame(tg2, argb_hd, &out_w, &out_h);
            if (out_w > 0 && out_h > 0 && (out_w != tex_w || out_h != tex_h || recreate_texture)) {
                recreate_texture = false;
                recreate_sdl_texture(renderer, &texture, out_w, out_h, &tex_w, &tex_h);
            }

            memcpy(overlay_buf, argb_hd, (size_t)tex_w * (size_t)tex_h * sizeof(uint32_t));
            tg2_menu_render(&menu, overlay_buf, tex_w, tex_h);

            sdl.UpdateTexture(texture, NULL, overlay_buf, tex_w * (int)sizeof(uint32_t));
            sdl.RenderClear(renderer);
            sdl.RenderCopy(renderer, texture, NULL, NULL);
            sdl.RenderPresent(renderer);

            sdl.Delay(16);
            continue;
        }

        if (paused) {
            int out_w = 0, out_h = 0;
            tg2_render_hd_frame(tg2, argb_hd, &out_w, &out_h);
            if (out_w > 0 && out_h > 0 && (out_w != tex_w || out_h != tex_h || recreate_texture)) {
                recreate_texture = false;
                recreate_sdl_texture(renderer, &texture, out_w, out_h, &tex_w, &tex_h);
            }
            memcpy(overlay_buf, argb_hd, (size_t)tex_w * (size_t)tex_h * sizeof(uint32_t));
            tg2_menu_render_pause_banner(overlay_buf, tex_w, tex_h);

            sdl.UpdateTexture(texture, NULL, overlay_buf, tex_w * (int)sizeof(uint32_t));
            sdl.RenderClear(renderer);
            sdl.RenderCopy(renderer, texture, NULL, NULL);
            sdl.RenderPresent(renderer);

            sdl.Delay(16);
            continue;
        }

        {
            uint16_t p1 = poll_controller_input(pad1, &menu, true);
            uint16_t p2 = poll_controller_input(pad2, &menu, false);
            if (wait_input_release) {
                if (p1 == 0 && p2 == 0) {
                    wait_input_release = false;
                } else {
                    p1 = 0;
                    p2 = 0;
                }
            }

            if (!tg2_step_frame(tg2, p1, p2)) {
                fprintf(stderr, "Error during frame %llu execution!\n", (unsigned long long)frame_index);
                break;
            }

            update_gamepad_rumble(pad1, tg2, p1, &last_speed, &menu);

            const uint8_t *wram = tg2_get_wram(tg2);
            const uint8_t *ppu = tg2_get_ppu_registers(tg2);

            if (wram) {
                bool has_career_sig = (wram[0x1CD7] == 0x65u && wram[0x1CD8] == 0x7Eu &&
                                       wram[0x1CD9] == 0x43u && wram[0x1CDA] == 0x55u);
                if (has_career_sig) {
                    if (!career_synced) {
                        tg2_menu_apply_game_settings(&menu, tg2);
                        career_synced = true;
                    }
                } else {
                    career_synced = false;
                }

                if (ppu && ppu[0x07] == 0x78u && ppu[0x08] == 0x71u && (ppu[0x00] & 0x80u) == 0u) {
                    uint16_t cur_circ = (uint16_t)(wram[0x0108] | ((uint16_t)wram[0x0109] << 8));
                    if (cur_circ != last_circuit && cur_circ < 64u) {
                        last_circuit = cur_circ;
                        if (tg2_save_state_file(tg2, autosave_path)) {
                            tg2_save_state_json_summary(tg2, savejson_path);
                        }
                    }
                }
            }

            int out_w = 0, out_h = 0;
            tg2_render_hd_frame(tg2, argb_hd, &out_w, &out_h);

            if (out_w > 0 && out_h > 0 && (out_w != tex_w || out_h != tex_h || recreate_texture)) {
                recreate_texture = false;
                recreate_sdl_texture(renderer, &texture, out_w, out_h, &tex_w, &tex_h);
            }

            sdl.UpdateTexture(texture, NULL, argb_hd, tex_w * (int)sizeof(uint32_t));
            sdl.RenderClear(renderer);
            sdl.RenderCopy(renderer, texture, NULL, NULL);
            sdl.RenderPresent(renderer);

            if (audio_dev) {
                size_t frames = tg2_get_audio_samples(tg2, audio_buffer, sizeof(audio_buffer) / sizeof(audio_buffer[0]));
                if (frames > 0) {
                    if (menu.master_volume < 100) {
                        int vol = menu.master_volume;
                        size_t sample_count = frames * 2u;
                        for (size_t s = 0; s < sample_count; s++) {
                            audio_buffer[s] = (int16_t)(((int32_t)audio_buffer[s] * vol) / 100);
                        }
                    }
                    uint32_t bytes_to_queue = (uint32_t)(frames * 2 * sizeof(int16_t));
                    uint32_t queued = sdl.GetQueuedAudioSize(audio_dev);
                    if (queued < (uint32_t)(TOPGEAR2_AUDIO_SAMPLE_RATE * 2 * sizeof(int16_t) * 0.20)) {
                        sdl.QueueAudio(audio_dev, audio_buffer, bytes_to_queue);
                    }
                }
            }

            frame_index++;
            if (target_frames > 0 && frame_index >= target_frames) {
                running = false;
            }
        }

        /* 60.0988 Hz frame pacing */
        next_frame_tick += (uint64_t)frame_ticks;
        uint64_t now = sdl.GetPerformanceCounter();
        if (now < next_frame_tick) {
            uint32_t ms_to_sleep = (uint32_t)(((next_frame_tick - now) * 1000) / perf_freq);
            if (ms_to_sleep > 1) {
                sdl.Delay(ms_to_sleep - 1);
            }
            while (sdl.GetPerformanceCounter() < next_frame_tick) {
            }
        } else if (now > next_frame_tick + (uint64_t)(frame_ticks * 4)) {
            next_frame_tick = now;
        }
    }

    printf("\nShutting down Top Gear 2 Definitive Edition...\n");
    tg2_menu_save_config(&menu, config_path);
    if (pad1) sdl.GameControllerClose(pad1);
    if (pad2) sdl.GameControllerClose(pad2);
    if (audio_dev) sdl.CloseAudioDevice(audio_dev);
    sdl.DestroyTexture(texture);
    sdl.DestroyRenderer(renderer);
    sdl.DestroyWindow(window);
    sdl.Quit();
    DYNLIB_CLOSE(sdl.handle);

    printf("Total frames executed: %llu\n", (unsigned long long)tg2_get_frame_count(tg2));
    printf("Total instructions executed: %llu\n", (unsigned long long)tg2_get_instruction_count(tg2));
    tg2_destroy(tg2);

    return 0;
}
