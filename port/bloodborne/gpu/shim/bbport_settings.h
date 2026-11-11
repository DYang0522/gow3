// SPDX-License-Identifier: GPL-2.0-or-later
// bbport: user settings changed at run time from the in-game menu (bbport_overlay.h) and kept
// in bbport.ini (BB_CONFIG overrides the path). Environment variables override the file at
// start. Readers load the atomics every frame; writers are the menu and Load().

#pragma once

#include <atomic>

namespace BbSettings {

enum Upscaler : int { UpscalerOff = 0, UpscalerFsr3 = 1, UpscalerTaa = 2, UpscalerCount };
enum Preset : int { NativeAA = 0, Quality, Balanced, Performance, UltraPerformance, PresetCount };
enum DebugView : int { DebugNone = 0, DebugReactive = 1, DebugMotion = 2, DebugViewCount };
/// Frame generation (vk_frame_generation.h): frames generated between two of the game's.
enum FrameGen : int { FrameGenOff = 0, FrameGenFsr3 = 1, FrameGenCount };
/// Game effects switched by the community patches at start (patches.py EFFECTS): ini key,
/// menu label, default (the game's own behaviour).
struct Effect {
    const char* key;
    const char* label;
    bool default_on;
};
inline constexpr Effect Effects[] = {
    {"effect_chromatic_aberration", "Chromatic aberration", true},
    {"effect_dof", "Depth of field (DoF)", true},
    {"effect_motion_blur", "Motion blur", true},
    {"effect_ssao", "Ambient occlusion (SSAO)", true},
    {"effect_game_aa", "Game anti-aliasing", true},
    {"effect_dynamic_shadows", "Dynamic shadows", true},
    {"effect_ssr", "Screen-space reflections (SSR)", false},
    {"skip_intro", "Skip intro logos and movie", false},
    {"debug_camera", "Free camera (Cross + L3)", false},
    {"debug_menu", "Debug menu (requires font files)", false},
    {"cheat_no_death", "Cheat: never die (minimum 1 HP)", false},
    {"cheat_stealth", "Cheat: enemies cannot see you", false},
    {"cheat_silent", "Cheat: enemies cannot hear you", false},
    {"cheat_rally_no_decay", "Cheat: Rally never decays", false},
    {"cheat_enemy_control", "Cheat: control targeted enemy (R3 / L3)", false},
    {"tweak_no_rally", "Disable Rally (health recovery)", false},
    {"tweak_camera_distance", "Increase camera distance", false},
    {"tweak_no_camera_rotation", "Disable automatic camera rotation", false},
    {"tweak_easy_run", "Run with less stick tilt", false},
    {"tweak_ragdoll", "Dark Souls-style ragdoll physics", false},
    {"tweak_circle_confirm", "Japanese/Asian buttons (Circle confirms)", false},
    {"quit_to_desktop", "Quit to Desktop in the title and System menus", true},
    {"pc_menu", "PC Settings in the System menu", true},
    {"rebirth", "Rebirth at the Doll (5 Insight)", true},
    {"steady_cloth", "Cloth as on PS4 (30 FPS)", true},
};
inline constexpr int EffectCount = int(sizeof(Effects) / sizeof(Effects[0]));
/// Output resolutions: the upscaler's output and the UI host targets. The 16:9 ones change
/// live; the ultrawide ones need the startup patch (patches.py ultrawide_writes, run.py).
inline constexpr int OutputWidths[] = {1280, 1920, 2560, 3840, 2560, 3440, 3840, 5120, 3840, 5120};
inline constexpr int OutputHeights[] = {720, 1080, 1440, 2160, 1080, 1440, 1600, 2160, 1080, 1440};
inline constexpr int OutputCount = 10;
inline constexpr int OutputDefault = 1; ///< 1920x1080, the game's own size
/// Wider than 16:9: camera and HUD patched at startup.
inline bool Ultrawide(int output) {
    return OutputWidths[output] * 9 > OutputHeights[output] * 16;
}

/// Button prompts (bbport_prompts.cpp): the game's icons switch with the device in use, or stay.
enum Prompts : int {
    PromptsAuto = 0, PromptsPlayStation = 1, PromptsKeyboard = 2, PromptsXbox = 3, PromptsSwitch = 4, PromptsCount
};

/// Frame rate mode (patches.py FPS_PRESETS, bbport.ini fps_mode): the community patches applied
/// at start. The launcher and the PC Settings page share it.
enum FpsMode : int { Fps30 = 0, Fps60 = 1, Fps90 = 2, FpsUncap = 3, FpsModeCount };
inline constexpr const char* FpsModeNames[FpsModeCount] = {"30", "60", "90", "uncap"};
/// Frame cap of the unlocked mode (bbport.ini frame_cap, BB_FPS_LIMIT): -1 the port's own (the
/// display refresh, at most 120), 0 none. Applied at start.
inline constexpr int FrameCapAuto = -1;
inline constexpr int FrameCaps[] = {FrameCapAuto, 60, 90, 120, 144, 165, 240, 0};
inline constexpr int FrameCapCount = int(sizeof(FrameCaps) / sizeof(FrameCaps[0]));

/// Field of view (fov.py): the game's camera has 43 degrees.
inline constexpr int FovDefault = 43, FovMin = 30, FovMax = 90;

struct Values {
    std::atomic<int> upscaler{UpscalerFsr3};
    std::atomic<int> preset{NativeAA};
    std::atomic<bool> sharpen{true};
    std::atomic<float> sharpness{0.3f};
    std::atomic<bool> jitter{true};
    std::atomic<bool> reactive{false};
    std::atomic<bool> object_motion{true};
    std::atomic<bool> object_motion_cache{false};
    /// UI visibility only; saved compatibility selections remain visible.
    std::atomic<bool> fsr_compatibility_options{false};
    std::atomic<float> reactive_scale{1.0f};
    std::atomic<float> reactive_threshold{0.2f};
    std::atomic<float> reactive_max{0.9f};
    std::atomic<int> debug_view{DebugNone};
    /// The FPS overlay's readouts, one toggle each (bbport.ini: show_fps, show_frame_times,
    /// show_vram, show_gpu, show_cpu, show_ram, show_fps_graph).
    std::atomic<bool> show_fps{false};
    std::atomic<bool> show_frame_times{false};
    /// The counter's values green / amber / red by how they stand (bbport.ini counter_colors=1),
    /// or all in plain grey (0).
    std::atomic<bool> counter_colors{true};
    std::atomic<bool> show_vram{false};
    std::atomic<bool> show_gpu{false};
    std::atomic<bool> show_cpu{false};
    std::atomic<bool> show_ram{false};
    std::atomic<bool> show_fps_graph{false};
    /// Silence the game while its window does not have focus (bbgpu_audio_muted).
    std::atomic<bool> mute_unfocused{false};
    /// Keyboard and mouse button prompts: with the device in use (auto), or always one kind.
    std::atomic<int> button_prompts{PromptsAuto};
    std::atomic<int> active_render_width{1920}, active_render_height{1080};
    /// Applied at start (patches.py); the menu shows when a restart is needed.
    std::atomic<bool> effects[EffectCount]{};
    std::atomic<int> model_lod{0}; ///< -2 highest .. 2 lowest, 0 the game's
    std::atomic<int> output_res{OutputDefault}; ///< index into OutputWidths
    std::atomic<bool> vsync{true}; ///< FIFO presentation; off selects Mailbox/Immediate when available
    /// Ultrawide outputs: the HUD's corner groups, 0 centred in 16:9 .. 100 at the screen edges
    /// (hud_layout.py at start).
    std::atomic<int> hud_position{100};
    /// Sharp UI: menus and HUD drawn at the output's size (patches.py ui_viewport at start). Set in
    /// the launcher; kept here so the menu's Save does not drop it.
    std::atomic<bool> sharp_ui{false};
    /// Vertical field of view in degrees, 43 the game's (run.py, fov.py at start).
    std::atomic<int> fov{FovDefault};
    /// FpsMode and frame cap (run.py at start; the launcher passes the same).
    std::atomic<int> fps_mode{FpsUncap};
    std::atomic<int> frame_cap{FrameCapAuto};
    /// Live resolution and preset changes (run.sh): 0 off by default (startup patch, fastest
    /// on the Steam Deck and older GPUs), -1 auto (strong discrete GPUs), 1 on. On restart.
    std::atomic<int> live_resolution{0};
    /// Frame generation and the frames per game frame (2x with FSR 3.1).
    std::atomic<int> frame_gen{FrameGenOff};
    std::atomic<const char*> frame_gen_problem{nullptr};
    /// Generated frames actually presented for the latest game frame.
    std::atomic<int> frame_gen_now{0};
    /// The game's own presentation rate, before generated frames.
    std::atomic<float> game_fps{0.0f};
    /// Why this GPU runs without object motion vectors whatever the setting says (vk_object_motion.cpp:
    /// RX 6000 and older, under 6 GB of VRAM), nullptr when the setting decides.
    std::atomic<const char*> object_motion_unavailable{nullptr};

    /// Startup settings for the explicit BB_RENDER_RES compatibility patch only.
    int startup_preset = NativeAA;
    int startup_upscaler = UpscalerFsr3;
    bool startup_object_motion = true;
    bool startup_object_motion_cache = false;
    bool startup_effects[EffectCount]{};
    int startup_model_lod = 0;
    int startup_output_res = OutputDefault;
    int startup_hud_position = 100;
    int startup_fov = FovDefault;
    int startup_fps_mode = FpsUncap;
    int startup_frame_cap = FrameCapAuto;
    int startup_live_resolution = 0;
};

Values& Get();

/// Reads the file, then the environment overrides. Called once at start.
void Load();
void SelectFrameGen(int provider);
void FrameGenFailure(const char* problem);
/// Startup-patched scene dimensions cannot change until run.sh prepares a new image.
bool FixedRenderSession();
int RenderPreset();
bool ResolutionNeedsRestart();
/// The cache keeps its startup behavior; provider changes apply live.
bool FsrSettingsNeedRestart();
/// Writes the file (menu changes).
void Save();

/// Render resolution divisor of a preset (1.0 native, 1.5 quality, ...).
float PresetScale(int preset);
const char* PresetName(int preset);
const char* UpscalerName(int upscaler);
const char* FrameGenName(int frame_gen);

} // namespace BbSettings
