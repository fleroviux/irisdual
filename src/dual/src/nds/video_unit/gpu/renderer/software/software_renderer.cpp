
#include <dual/nds/video_unit/gpu/renderer/software_renderer.hpp>

namespace dual::nds::gpu {

  SoftwareRenderer::SoftwareRenderer(
    IO& io,
    const Region<4, 131072>& vram_texture,
    const Region<8>& vram_palette
  )   : m_io{io}
      , m_vram_texture{vram_texture}
      , m_vram_palette{vram_palette} {
  }

  void SoftwareRenderer::Render(const Viewport& viewport, std::span<const Polygon* const> polygons) {
    if(m_io.disp3dcnt.enable_rear_plane_bitmap) {
      ATOM_PANIC("gpu: sw: Unimplemented rear plane bitmap");
    }

    const bool enabled_aa = m_io.disp3dcnt.enable_anti_aliasing;

    m_clear_depth = (((u32)m_io.clear_depth << 9) + (((u32)m_io.clear_depth + 1u) >> 15)) * 0x1FFu;

    CopyVRAM();
    ClearColorBuffer();
    ClearDepthBuffer();
    ClearAttributeBuffer();
    if(enabled_aa) {
      ClearCoverageBuffer();
    }
    RenderPolygons(viewport, polygons);

    if(m_io.disp3dcnt.enable_edge_marking) {
      RenderEdgeMarking();
    }

    if(m_io.disp3dcnt.enable_fog) {
      RenderFog();
    }

    if(enabled_aa) {
      RenderAntiAliasing();
    }
  }

  void SoftwareRenderer::CaptureColor(int scanline, std::span<u16, 256> dst_buffer, int dst_width, bool display_capture) {
    // @todo: write a separate method for display capture?

    for(int x = 0; x < dst_width; x++) {
      const Color4& color = m_frame_buffer[0][scanline][x];

      if(display_capture) {
        dst_buffer[x] = color.ToRGB555() | (color.A() != 0 ? 0x8000u : 0u);
      } else {
        dst_buffer[x] = color.A() == 0 ? 0x8000u : color.ToRGB555();
      }
    }
  }

  void SoftwareRenderer::CaptureAlpha(int scanline, std::span<int, 256> dst_buffer) {
    // Remapping from the [0, 63] range to the [0, 16] range is a hack but (currently) required for alpha-blending.
    // Most likely the alpha-blending math needs to happen with higher precision (but how many bits?).
    // @todo: make this more accurate.
    for(int x = 0; x < 256; x++) {
      dst_buffer[x] = (m_frame_buffer[0][scanline][x].A().Raw() + 1) >> 2;
    }
  }

  void SoftwareRenderer::CopyVRAM() {
    for(u32 address = 0; address < 0x80000u; address += 8u) {
      *(u64*)&m_vram_texture_copy[address] = m_vram_texture.Read<u64>(address);
    }

    for(u32 address = 0; address < 0x20000u; address += 8u) {
      *(u64*)&m_vram_palette_copy[address] = m_vram_palette.Read<u64>(address);
    }
  }

} // namespace dual::nds::gpu
