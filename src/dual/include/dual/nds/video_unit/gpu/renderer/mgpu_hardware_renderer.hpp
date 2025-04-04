
#pragma once

#include <atom/float.hpp>
#include <atom/vector_n.hpp>
#include <dual/nds/video_unit/gpu/renderer/renderer_base.hpp>
#include <dual/nds/vram/region.hpp>
#include <mgpu/mgpu.h>
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
      float x;
      float y;
      float z;
      float w;

      // vertex color
      float r;
      float g;
      float b;
      float a;

      // texture coordinate
      float s;
      float t;
    } __attribute__((packed));

    IO& m_io;
    const Region<4, 131072>& m_vram_texture;
    const Region<8>& m_vram_palette;

    SDL_Window* m_sdl_window{};
    MGPUInstance m_mgpu_instance{};
    MGPUSurface m_mgpu_surface{};
    MGPUDevice m_mgpu_device{};
    MGPUSwapChain m_mgpu_swap_chain{};
    std::vector<MGPUTexture> m_mgpu_swap_chain_textures{};
    std::vector<MGPUTextureView> m_mgpu_swap_chain_texture_views{};
    MGPUBuffer m_mgpu_vbo{};
    MGPUShaderModule m_mgpu_vert_shader{};
    MGPUShaderModule m_mgpu_frag_shader{};
    MGPUShaderProgram m_mgpu_shader_program{};
    MGPURasterizerState m_mgpu_rasterizer_state{};
    MGPUInputAssemblyState m_mgpu_input_assembly_state{};
    MGPUColorBlendState m_mgpu_color_blend_state{};
    MGPUVertexInputState m_mgpu_vertex_input_state{};
    MGPUCommandList m_mgpu_cmd_list{};
    MGPUQueue m_mgpu_queue{};

    atom::Vector_N<BufferVertex, k_total_vertices> m_vbo_data{};
};

} // namespace dual::nds::gpu
