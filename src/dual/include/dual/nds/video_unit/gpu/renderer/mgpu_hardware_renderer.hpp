
#pragma once

#include <atom/float.hpp>
#include <atom/vector_n.hpp>
#include <dual/nds/video_unit/gpu/renderer/mgpu_texture_cache.hpp>
#include <dual/nds/video_unit/gpu/renderer/renderer_base.hpp>
#include <dual/nds/vram/region.hpp>
#include <mgpu/mgpu.h>
#include <memory>
#include <SDL.h>
#include <vector>

namespace dual::nds::gpu {

class MGPUHardwareRenderer final : public RendererBase {
  public:
    MGPUHardwareRenderer(
      IO& io,
      const Region<4, 131072>& vram_texture,
      const Region<8>& vram_palette
    );

   ~MGPUHardwareRenderer() override;

    void SetWBufferEnable(bool enabel_w_buffer) override {
    }

    void UpdateEdgeColor(size_t table_offset, std::span<const u32> table_data) override {
    }

    void UpdateToonTable(size_t table_offset, std::span<const u32> table_data) override {
    }

    void Render(const Viewport& viewport, std::span<const Polygon* const> polygons) override;

    void CaptureColor(int scanline, std::span<u16, 256> dst_buffer, int dst_width, bool display_capture) override {
    }

    void CaptureAlpha(int scanline, std::span<int, 256> dst_buffer) override {
    }

  private:
    /**
     * Up to 2048 n-gons can be rendered in a single frame.
     * A single n-gon may have up to ten vertices (10-gon).
     *
     * We render each n-gon with `n - 2` triangles, meaning there are
     * up to 8 triangles per n-gon.
     *
     * Each triangle consists of three vertices.
     */
    static constexpr size_t k_total_vertices = 2048 * 8 * 3;

    struct BufferVertex {
      // clip-space position
      f32 x;
      f32 y;
      f32 z;
      f32 w;

      // vertex color
      f32 r;
      f32 g;
      f32 b;
      f32 a;

      // texture coordinate
      f32 s;
      f32 t;
    } __attribute__((packed));

    IO& m_io;

    SDL_Window* m_sdl_window{};
    MGPUInstance m_mgpu_instance{};
    MGPUSurface m_mgpu_surface{};
    MGPUDevice m_mgpu_device{};
    MGPUSwapChain m_mgpu_swap_chain{};
    std::vector<MGPUTexture> m_mgpu_swap_chain_color_textures{};
    std::vector<MGPUTexture> m_mgpu_swap_chain_depth_textures{};
    std::vector<MGPUTextureView> m_mgpu_swap_chain_color_texture_views{};
    std::vector<MGPUTextureView> m_mgpu_swap_chain_depth_texture_views{};

    MGPUBuffer m_mgpu_vbo{};
    MGPUBuffer m_mgpu_test_ubo{};
    MGPUSampler m_mgpu_nearest_sampler{};
    MGPUResourceSetLayout m_mgpu_resource_set_layout{};
    MGPUShaderModule m_mgpu_vert_shader{};
    MGPUShaderModule m_mgpu_frag_shader{};
    MGPUShaderProgram m_mgpu_shader_program{};
    MGPURasterizerState m_mgpu_rasterizer_state{};
    MGPUVertexInputState m_mgpu_vertex_input_state{};
    MGPUDepthStencilState m_mgpu_depth_stencil_state{};
    MGPUCommandList m_mgpu_cmd_list{};
    MGPUQueue m_mgpu_queue{};

    std::unique_ptr<MGPUTextureCache> m_texture_cache{};

    atom::Vector_N<BufferVertex, k_total_vertices> m_vbo_data{};
};

} // namespace dual::nds::gpu
