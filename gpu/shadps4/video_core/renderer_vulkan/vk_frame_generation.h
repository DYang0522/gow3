// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <chrono>
#include <deque>
#include <memory>
#include <span>
#include <vector>

#include "common/types.h"
#include "video_core/renderer_vulkan/vk_common.h"
#include "video_core/texture_cache/image.h"

struct FfxVkFsr3_3_1_6FrameGenerationContext;

namespace Vulkan {
class Instance;
class Scheduler;

struct FrameGenCamera {
    float near_plane = 0.1f;
    float far_plane = 3000.0f;
    float vertical_fov = 1.0f;
};

class FrameGenerator {
public:
    static constexpr u32 MaxGenerated = 1;
    static constexpr vk::Format ColorFormat = vk::Format::eR8G8B8A8Unorm;
    struct Output { vk::Image image; vk::ImageView view; };

    FrameGenerator(const Instance& instance, Scheduler& scheduler);
    ~FrameGenerator();
    [[nodiscard]] bool Wanted() const;
    [[nodiscard]] u32 WantedCount() const;
    void Skip();
    void Release();
    vk::ImageView BeginCapture(vk::CommandBuffer cmdbuf, vk::ImageView depth, u32 w, u32 h);
    void EndCapture(vk::CommandBuffer cmdbuf, std::array<float, 2> jitter, bool motion_valid,
                    FrameGenCamera camera = {});
    void CaptureHudless(vk::CommandBuffer cmdbuf, vk::Image source, vk::Format format, u32 w, u32 h);
    vk::ImageView Hudless(vk::Format format, vk::Extent2D& size);
    u32 Generate(vk::CommandBuffer& cmdbuf, vk::Image color, vk::ImageView color_view,
                 vk::ImageUsageFlags color_usage, vk::Image hudless, vk::ImageView hudless_view,
                 u32 width, u32 height, std::span<const Output> outputs);
    void DrainOverlap();
    void DumpIfRequested(vk::CommandBuffer cmdbuf, vk::Image image, u32 w, u32 h,
                         const char* name);

private:
    void EnsureInputs(u32 w, u32 h);
    void CreatePipeline();
    u32 GenerateFsr3(vk::CommandBuffer cmdbuf, vk::Image color, vk::ImageUsageFlags color_usage,
                     u32 width, u32 height, const Output& output, float frame_ms);
    void ComposeUi(vk::CommandBuffer cmdbuf, vk::ImageView color, vk::ImageView hudless,
                   const Output& output, u32 width, u32 height);
    void DestroyFsr3();
    void Fail(const char* reason);

    const Instance& instance;
    Scheduler& scheduler;
    VideoCore::UniqueImage depth_image, motion_image;
    vk::UniqueImageView depth_view, motion_view;
    u32 input_width = 0, input_height = 0;
    vk::UniqueDescriptorSetLayout desc_layout;
    vk::UniquePipelineLayout pipeline_layout;
    vk::UniquePipeline pipeline;
    vk::UniqueDescriptorSetLayout ui_desc_layout;
    vk::UniquePipelineLayout ui_pipeline_layout;
    vk::UniquePipeline ui_pipeline;

    VideoCore::UniqueImage hudless_image;
    vk::Format hudless_format{};
    u32 hudless_width = 0, hudless_height = 0;
    struct HudlessView { vk::Format format; vk::UniqueImageView view; };
    std::vector<HudlessView> hudless_views;
    bool hudless_captured = false;
    bool captured = false;
    std::array<float, 2> capture_jitter{};
    FrameGenCamera capture_camera{};
    bool reset = true;
    bool last_hudless = false;
    u32 hudless_changes = 0;
    std::chrono::steady_clock::time_point last_generate{}, last_capture{};
    float capture_frame_ms = 16.6f;

    FfxVkFsr3_3_1_6FrameGenerationContext* fsr3 = nullptr;
    u32 fsr3_width = 0, fsr3_height = 0, fsr3_render_width = 0, fsr3_render_height = 0;
    u64 fsr3_frame_id = 0;
    struct Pending { u64 frame_id; u64 tick; };
    std::deque<Pending> fsr3_pending;
};
} // namespace Vulkan
