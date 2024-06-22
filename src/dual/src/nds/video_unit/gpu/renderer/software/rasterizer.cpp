
#include <algorithm>
#include <dual/nds/video_unit/gpu/renderer/software_renderer.hpp>
#include <limits>

#include "edge.hpp"
#include "interpolator.hpp"

namespace dual::nds::gpu {

  void SoftwareRenderer::ClearColorBuffer() {
    const Color4 clear_color = Color4{
      (i8)((m_io.clear_color.color_r << 1) | (m_io.clear_color.color_r >> 4)),
      (i8)((m_io.clear_color.color_g << 1) | (m_io.clear_color.color_g >> 4)),
      (i8)((m_io.clear_color.color_b << 1) | (m_io.clear_color.color_b >> 4)),
      (i8)((m_io.clear_color.color_a << 1) | (m_io.clear_color.color_a >> 4)),
    };

    for(int y = 0; y < 192; y++) {
      for(int x = 0; x < 256; x++) {
        m_frame_buffer[0][y][x] = clear_color;
        m_frame_buffer[1][y][x] = Color4{0, 0, 0, 0};
      }
    }
  }

  void SoftwareRenderer::ClearDepthBuffer() {
    for(int y = 0; y < 192; y++) {
      for(int x = 0; x < 256; x++) {
        m_depth_buffer[0][y][x] = m_clear_depth;
        m_depth_buffer[1][y][x] = m_clear_depth;
      }
    }
  }

  void SoftwareRenderer::ClearAttributeBuffer() {
    const u8 poly_id = m_io.clear_color.polygon_id;

    const PixelAttributes clear_attributes = {
      .poly_id = { poly_id, poly_id },
      .flags = (u16)(m_io.clear_color.enable_rear_plane_fog ? PixelAttributes::Fog : 0)
    };

    // @todo: Validate that the translucent polygon ID is initialized correctly.
    for(int y = 0; y < 192; y++) {
      for(int x = 0; x < 256; x++) {
        m_attribute_buffer[y][x] = clear_attributes;
      }
    }
  }

  void SoftwareRenderer::ClearCoverageBuffer() {
    for(int y = 0; y < 192; y++) {
      for(int x = 0; x < 256; x++) {
        m_coverage_buffer[y][x] = 63;
      }
    }
  }

  void SoftwareRenderer::RenderPolygons(const Viewport& viewport, std::span<const Polygon* const> polygons) {
    for(const Polygon* polygon : polygons) {
      RenderPolygon(viewport, *polygon);
    }
  }

  void SoftwareRenderer::RenderPolygon(const Viewport& viewport, const Polygon& polygon) {
    const int vertex_count = (int)polygon.vertices.Size();

    i32 x0[2];
    i32 x1[2];
    Line line{};
    Edge::Point points[10];
    Interpolator<9> edge_interp{};

    int initial_vertex;
    int final_vertex;
    i32 y_min = std::numeric_limits<i32>::max();
    i32 y_max = std::numeric_limits<i32>::min();

    const bool force_edge_draw_1 = polygon.translucent || m_io.disp3dcnt.enable_anti_aliasing || m_io.disp3dcnt.enable_edge_marking;

    for(int i = 0; i < vertex_count; i++) {
      const Vertex* vertex = polygon.vertices[i];
      const Vector4<Fixed20x12>& position = vertex->position;

      const i64 w = position.W().Raw();
      const i64 two_w = (i64)w << 1;

      if(w == 0) {
        return;
      }

      const i32 x = (i32)(((( (i64)position.X().Raw() + w) * viewport.width  + 0x800) / two_w) + viewport.x0);
      const i32 y = (i32)((((-(i64)position.Y().Raw() + w) * viewport.height + 0x800) / two_w) + viewport.y0);
      const u32 depth = (u32)std::clamp<i64>(((((i64)position.Z().Raw() << 14) / w + 0x3FFF) << 9), 0ll, 0xFFFFFFll);

      points[i] = Edge::Point{x, y, depth, (i32)w, vertex};

      if(y < y_min) {
        y_min = y;
        initial_vertex = i;
      }

      if(y > y_max) {
        y_max = y;
        final_vertex = i;
      }
    }

    const bool wireframe = polygon.attributes.alpha == 0;

    const int a = polygon.windedness <= 0 ? 0 : 1;
    const int b = a ^ 1;

    int start[2] { initial_vertex, initial_vertex };
    int end[2];

    end[a] = initial_vertex + 1;
    end[b] = initial_vertex - 1;

    if(end[a] == vertex_count) {
      end[a] = 0;
    }

    if(end[b] == -1) {
      end[b] = vertex_count - 1;
    }

    Edge edge[2] {
      {points[initial_vertex], points[end[0]]},
      {points[initial_vertex], points[end[1]]}
    };

    // Allow horizontal line polygons to render.
    if(y_min == y_max) y_max++;

    int l = 0;
    int r = 1;

    for(i32 y = y_min; y < y_max; y++) {
      if(y >= points[end[a]].y && end[a] != final_vertex) {
        do {
          start[a] = end[a];
          if(++end[a] == vertex_count) {
            end[a] = 0;
          }
        } while(y >= points[end[a]].y && end[a] != final_vertex);

        edge[a] = Edge{points[start[a]], points[end[a]]};
      }

      if(y >= points[end[b]].y && end[b] != final_vertex) {
        do {
          start[b] = end[b];
          if(--end[b] == -1) {
            end[b] = vertex_count - 1;
          }
        } while(y >= points[end[b]].y && end[b] != final_vertex);

        edge[b] = Edge{points[start[b]], points[end[b]]};
      }

      for(int i = 0; i < 2; i++) {
        edge[i].Interpolate(y, x0[i], x1[i]);
      }

      // Detect when the left and right edges become swapped
      if(x0[l] >> 18 > x0[r] >> 18 || x1[l] >> 18 > x1[r] >> 18) {
        l ^= 1;
        r ^= 1;
      }

      for(int i = 0; i < 2; i++) {
        const int j = i ^ l;
        const u16 w0 = polygon.w_16[start[i]];
        const u16 w1 = polygon.w_16[end[i]];

        if(edge[i].IsXMajor()) {
          const i32 x_min = points[start[i]].x;
          const i32 x_max = points[end[i]].x;
          const i32 x = (i == l ? x0[l] : x1[r]) >> 18;

          if(x_min <= x_max) {
            edge_interp.Setup(w0, w1, x, x_min, x_max);
          } else {
            edge_interp.Setup(w0, w1, (x_min - (x - x_max)), x_max, x_min);
          }
        } else {
          edge_interp.Setup(w0, w1, y, points[start[i]].y, points[end[i]].y);
        }

        edge_interp.Perp(points[start[i]].vertex->color, points[end[i]].vertex->color, line.color[j]);
        edge_interp.Perp(points[start[i]].vertex->uv, points[end[i]].vertex->uv, line.uv[j]);
        line.w_16[j] = edge_interp.Perp(w0, w1);

        if(m_enable_w_buffer) {
          line.depth[j] = (u32)line.w_16[j] << polygon.w_l_shift >> polygon.w_r_shift;
        } else {
          line.depth[j] = edge_interp.Lerp(points[start[i]].depth, points[end[i]].depth);
        }
      }

      if(y < 0) {
        continue;
      }

      if(y >= 192) {
        break;
      }

      const bool inner_span_is_edge = y == y_min || y == y_max - 1;

      int xl0 = x0[l] >> 18;
      int xr1 = x1[r] >> 18;
      int xl1 = std::clamp(x1[l] >> 18, xl0, xr1);
      int xr0 = std::clamp(x0[r] >> 18, xl1, xr1);

      // Setup the leftmost and rightmost X-coordinates for the horizontal interpolator.
      line.x[0] = xl0;
      line.x[1] = xr1;

      const bool l_vertical = points[start[l]].x == points[end[l]].x;
      const bool r_vertical = points[start[r]].x == points[end[r]].x;

      /**
       * From StrikerX3:
       *   A perfectly vertical right edge has a few gotchas:
       *   - the horizontal attribute interpolator's rightmost X coordinate is incremented by one
       *   - the right edge is nudged to the left by one pixel
       *
       * According to Jakly this applies specifically to right vertical edges these additional conditions are met:
       *   - The left slope isn't vertical OR the start of the span is unequal to the end of the span
       *   - The end of the span is not at x == 0
       * She also noted that the logic is applied before edge-swapping.
       *
       * Currently we aren't 100% accurate here. We do the adjustment after edge-swapping.
       * We also check the end of the span against 255 instead of 0 (Q: is this equivalent to x == 256?).
       * For the latter this was necessary to avoid gaps at the right border of the viewport.
       * Maybe the viewport transform or polygon clipping aren't accurate enough?
       */
      if(r_vertical && (!l_vertical || line.x[0] != line.x[1]) && line.x[1] != 255) {
        line.x[1]++;
        xr0 = std::max(xl1, xr0 - 1);
        xr1 = std::max(xl1, xr1 - 1);
      }

      /**
       * Span fill rules, described by StrikerX3:
       *  The interior is always filled, except in wireframe mode
       *   - the left edge is filled if the slope is negative or not x-major
       *   - the right edge is filled if the slope is positive and x-major, or if it is vertical (x0==x1)
       *   - both edges are drawn when the polygon is translucent or wireframe,
       *     or when antialiasing or edge marking is enabled, or it's the last scanline on the screen
       */
      const bool force_edge_draw_2 = force_edge_draw_1 || y == 191;

      if(edge[l].GetXSlope() < 0 || !edge[l].IsXMajor() || force_edge_draw_2) {
        int cov[2];

        if(edge[l].IsXMajor()) {
          cov[0] = 63;
          cov[1] = 0;
        } else {
          cov[0] = edge[l].GetXSlope() != 0 ? (u8)(((x0[l] >> 12) & 63u) ^ 63u) : 63;
        }

        RenderPolygonSpan(polygon, line, y, xl0, xl1, true, cov[0], cov[1]);
      }

      if(!wireframe || inner_span_is_edge) {
        RenderPolygonSpan(polygon, line, y, xl1 + 1, xr0 - 1, inner_span_is_edge, 63, 63);
      }

      if((edge[r].GetXSlope() > 0 && edge[r].IsXMajor()) || edge[r].GetXSlope() == 0 || force_edge_draw_2) {
        int cov[2];

        if(edge[r].IsXMajor()) {
          cov[0] = 0;
          cov[1] = 63;
        } else {
          cov[0] = edge[r].GetXSlope() != 0 ? (u8)((x1[r] >> 12) & 63u) : 63;
        }

        RenderPolygonSpan(polygon, line, y, xr0, xr1, true, cov[0], cov[1]);
      }
    }
  }

  void SoftwareRenderer::RenderPolygonSpan(const Polygon& polygon, const Line& line, i32 y, int x0, int x1, bool edge, int cov0, int cov1) {
    const i32 depth_test_threshold = m_enable_w_buffer ? 0xFF : 0x200;
    const u32 alpha = polygon.attributes.alpha << 1 | polygon.attributes.alpha >> 4;
    const auto polygon_mode = (Polygon::Mode)polygon.attributes.polygon_mode;
    const u8 polygon_id = polygon.attributes.polygon_id;
    const bool enable_fog = polygon.attributes.enable_fog;

    int alpha_test_threshold = 0u;

    if(m_io.disp3dcnt.enable_alpha_test) {
      alpha_test_threshold = m_io.alpha_test_ref << 1 | m_io.alpha_test_ref >> 4;
    }

    x0 = std::max(x0, 0);
    x1 = std::min(x1, 255);

    Interpolator<8> line_interp{};
    Color4 color;
    Vector2<Fixed12x4> uv;

    const auto EvaluateDepthTest = [&](u32 depth_old, u32 depth_new) {
      if(polygon.attributes.use_equal_depth_test) [[unlikely]] {
        return std::abs((i32)depth_new - (i32)depth_old) <= depth_test_threshold;
      }
      return depth_new < depth_old;
    };

    for(int x = x0; x <= x1; x++) {
      line_interp.Setup(line.w_16[0], line.w_16[1], x, line.x[0], line.x[1]);

      const u32 depth_old = m_depth_buffer[0][y][x];
      const u32 depth_new = m_enable_w_buffer ?
        line_interp.Perp(line.depth[0], line.depth[1]) : line_interp.Lerp(line.depth[0], line.depth[1]);

      const bool top_depth_test_passed = EvaluateDepthTest(depth_old, depth_new);

      if(polygon_mode == Polygon::Mode::Shadow && polygon_id == 0u) {
        /**
         * I'm not sure if the depth buffer and Polygon ID should be updated,
         * in case the depth test passes.
         * But certainly the shadow flag should _not_ be cleared if the test passes (this causes shadows in Mario Kart DS to look incorrect).
         */
        if(!top_depth_test_passed) {
          m_attribute_buffer[y][x].flags |= PixelAttributes::Shadow;
        }
        continue;
      }

      const bool bottom_depth_test_passed = top_depth_test_passed || EvaluateDepthTest(m_depth_buffer[1][y][x], depth_new);

      if(!bottom_depth_test_passed) {
        continue;
      }

      line_interp.Perp(line.color[0], line.color[1], color);
      line_interp.Perp(line.uv[0], line.uv[1], uv);

      color.A() = (i8)alpha;

      if(m_io.disp3dcnt.enable_texture_mapping && polygon.texture_params.format != TextureParams::Format::Disabled) {
        const Color4 texel = SampleTexture(polygon.texture_params, polygon.palette_base, uv);

        if(texel.A().Raw() <= alpha_test_threshold) {
          continue;
        }

        color = ShadeTexturedPolygon(polygon_mode, texel, color);
      } else if(polygon_mode == Polygon::Mode::Shaded) {
        color = ShadeShadedUntexturedPolygon(color);
      }

      PixelAttributes& attributes = m_attribute_buffer[y][x];

      const bool opaque_pixel = color.A() == 63;

      if(!opaque_pixel && (attributes.flags & PixelAttributes::Translucent) && attributes.poly_id[1] == polygon_id) {
        continue;
      }

      bool color_write = true;

      if(polygon_mode == Polygon::Mode::Shadow) {
        // We assume that polygon_id != 0 here because we discard the other shadow polygon pixels.
        color_write = (attributes.flags & PixelAttributes::Shadow) && attributes.poly_id[0] != polygon_id;
      }

      if(!top_depth_test_passed) {
        if(color_write && bottom_depth_test_passed) {
          if(!opaque_pixel) {
            m_frame_buffer[1][y][x] = AlphaBlend(color, m_frame_buffer[1][y][x]);
          } else {
            m_frame_buffer[1][y][x] = color;
          }
          m_depth_buffer[1][y][x] = depth_new;
        }
        continue;
      }

      if(color_write) {
        if(!opaque_pixel) {
          m_frame_buffer[1][y][x] = AlphaBlend(color, m_frame_buffer[1][y][x]);
          m_frame_buffer[0][y][x] = AlphaBlend(color, m_frame_buffer[0][y][x]);
          attributes.flags |= PixelAttributes::Translucent;
          if(!enable_fog) {
            attributes.flags &= ~PixelAttributes::Fog;
          }
        } else {
          m_frame_buffer[1][y][x] = m_frame_buffer[0][y][x];
          m_frame_buffer[0][y][x] = color;
          attributes.flags &= ~(PixelAttributes::Translucent | PixelAttributes::Edge | PixelAttributes::Fog);

          if(edge && polygon_id != m_attribute_buffer[y][x].poly_id[0]) {
            attributes.flags |= PixelAttributes::Edge;
          }

          if(enable_fog) {
            attributes.flags |= PixelAttributes::Fog;
          }

          if(x1 != x0) {
            m_coverage_buffer[y][x] = (cov0 * (x - x0) + cov1 * (x1 - x)) / (x1 - x0);
          } else {
            m_coverage_buffer[y][x] = cov0;
          }
        }
      }

      // @todo: Ensure that this is correct if translucent depth write is off.
      m_depth_buffer[1][y][x] = m_depth_buffer[0][y][x];

      if(opaque_pixel) {
        m_depth_buffer[0][y][x] = depth_new;
        attributes.poly_id[0] = polygon_id;
      } else {
        if(polygon.attributes.enable_translucent_depth_write) {
          m_depth_buffer[0][y][x] = depth_new;
        }
        attributes.poly_id[1] = polygon_id;
      }

      // Probably the shadow flag is cleared not only by shadow polygons, but this needs proof.
      attributes.flags &= ~PixelAttributes::Shadow;
    }
  }

  Color4 SoftwareRenderer::ShadeTexturedPolygon(Polygon::Mode polygon_mode, Color4 texture_color, Color4 vertex_color) {
    const auto modulate = [](Fixed6 a, Fixed6 b) {
      return (i8)(((a.Raw() + 1) * (b.Raw() + 1) - 1) >> 6);
    };

    Color4 result_color;

    switch(polygon_mode) {
      case Polygon::Mode::Modulation: {
        for(int i : {0, 1, 2, 3}) {
          result_color[i] = modulate(texture_color[i], vertex_color[i]);
        }
        break;
      }
      case Polygon::Mode::Shadow:
      case Polygon::Mode::Decal: {
        const int s = texture_color.A().Raw();
        const int t = 63 - s;

        for(int i : {0, 1, 2}) {
          result_color[i] = (i8)((texture_color[i].Raw() * s + vertex_color[i].Raw() * t) >> 6);
        }
        break;
      }
      case Polygon::Mode::Shaded: {
        const Color4 toon_color = m_toon_table[vertex_color.R().Raw() >> 1];

        if(!m_io.disp3dcnt.enable_highlight_shading) {
          for(int i : {0, 1, 2}) {
            result_color[i] = modulate(texture_color[i], toon_color[i]);
          }
        } else {
          for(int i : {0, 1, 2}) {
            result_color[i] = (i8)std::min(64, modulate(texture_color[i], vertex_color[i]) + toon_color[i].Raw());
          }
        }

        result_color[3] = modulate(texture_color[3], vertex_color[3]);
        break;
      }
      default: {
        ATOM_PANIC("unhandled polygon mode");
      }
    }

    return result_color;
  }

  Color4 SoftwareRenderer::ShadeShadedUntexturedPolygon(Color4 vertex_color) {
    const Color4 toon_color = m_toon_table[vertex_color.R().Raw() >> 1];

    Color4 result_color;

    if(!m_io.disp3dcnt.enable_highlight_shading) {
      for(int i : {0, 1, 2}) {
        result_color[i] = toon_color[i];
      }
    } else {
      for(int i : {0, 1, 2}) {
        result_color[i] = (i8)std::min(64, vertex_color[i].Raw() + toon_color[i].Raw());
      }
    }

    return result_color;
  }

  Color4 SoftwareRenderer::AlphaBlend(Color4 src, Color4 dst) {
    if(dst.A() == 0) return src;

    const Fixed6 a0 = src.A();
    const Fixed6 a1 = Fixed6{63} - a0;

    for(const int i : {0, 1, 2}) {
      src[i] = src[i] * a0 + dst[i] * a1;
    }

    src.A() = std::max(src.A(), dst.A());

    return src;
  }

  Color4 SoftwareRenderer::SampleTexture(TextureParams params, u32 palette_base, Vector2<Fixed12x4> uv) {
    const int log2_size[2] {
      (int)params.log2_s_size,
      (int)params.log2_t_size
    };

    const int size[2] {
      8 << params.log2_s_size,
      8 << params.log2_t_size
    };

    int coord[2] { uv.X().Int(), uv.Y().Int() };

    for (int i = 0; i < 2; i++) {
      if (coord[i] < 0 || coord[i] >= size[i]) {
        int mask = size[i] - 1;
        if (params.repeat[i]) {
          bool odd = (coord[i] >> (3 + log2_size[i])) & 1;
          coord[i] &= mask;
          if (params.flip[i] && odd) {
            coord[i] ^= mask;
          }
        } else {
          coord[i] = std::clamp(coord[i], 0, mask);
        }
      }
    }

    auto offset = coord[1] * size[0] + coord[0];
    auto palette_addr = palette_base << 4;
    auto texture_addr = params.vram_offset_div_8 << 3;

    switch ((TextureParams::Format)params.format) {
      case TextureParams::Format::Disabled: {
        return Color4{};
      }
      case TextureParams::Format::A3I5: {
        u8  value = ReadTextureVRAM<u8>(texture_addr + offset);
        int index = value & 0x1F;
        int alpha = value >> 5;

        auto rgb555 = ReadPaletteVRAM<u16>(palette_addr + index * sizeof(u16)) & 0x7FFF;
        auto rgb6666 = Color4::FromRGB555(rgb555);

        rgb6666.A() = (alpha << 3) | alpha; // 3-bit alpha to 6-bit alpha
        return rgb6666;
      }
      case TextureParams::Format::Palette2BPP: {
        auto index = (ReadTextureVRAM<u8>(texture_addr + (offset >> 2)) >> (2 * (offset & 3))) & 3;

        if (params.color0_transparent && index == 0) {
          return Color4{0, 0, 0, 0};
        }

        return Color4::FromRGB555(ReadPaletteVRAM<u16>((palette_addr >> 1) + index * sizeof(u16)) & 0x7FFF);
      }
      case TextureParams::Format::Palette4BPP: {
        auto index = (ReadTextureVRAM<u8>(texture_addr + (offset >> 1)) >> (4 * (offset & 1))) & 15;

        if (params.color0_transparent && index == 0) {
          return Color4{0, 0, 0, 0};
        }

        return Color4::FromRGB555(ReadPaletteVRAM<u16>(palette_addr + index * sizeof(u16)) & 0x7FFF);
      }
      case TextureParams::Format::Palette8BPP: {
        auto index = ReadTextureVRAM<u8>(texture_addr + offset);

        if (params.color0_transparent && index == 0) {
          return Color4{0, 0, 0, 0};
        }

        return Color4::FromRGB555(ReadPaletteVRAM<u16>(palette_addr + index * sizeof(u16)) & 0x7FFF);
      }
      case TextureParams::Format::Compressed4x4: {
        auto row_x = coord[0] >> 2;
        auto row_y = coord[1] >> 2;
        auto tile_x = coord[0] & 3;
        auto tile_y = coord[1] & 3;
        auto row_size = size[0] >> 2;

        auto data_address = texture_addr + (row_y * row_size + row_x) * sizeof(u32) + tile_y;

        auto data_slot_index  = data_address >> 18;
        auto data_slot_offset = data_address & 0x1FFFF;
        auto info_address = 0x20000 + (data_slot_offset >> 1) + (data_slot_index * 0x10000);

        auto data = ReadTextureVRAM<u8>(data_address);
        auto info = ReadTextureVRAM<u16>(info_address);

        auto index = (data >> (tile_x * 2)) & 3;
        auto palette_offset = info & 0x3FFF;
        auto mode = info >> 14;

        palette_addr += palette_offset << 2;

        switch (mode) {
          case 0: {
            if (index == 3) {
              return Color4{0, 0, 0, 0};
            }
            return Color4::FromRGB555(ReadPaletteVRAM<u16>(palette_addr + index * sizeof(u16)) & 0x7FFF);
          }
          case 1: {
            if (index == 2) {
              auto color_0 = Color4::FromRGB555(ReadPaletteVRAM<u16>(palette_addr + 0) & 0x7FFF);
              auto color_1 = Color4::FromRGB555(ReadPaletteVRAM<u16>(palette_addr + 2) & 0x7FFF);

              for (uint i = 0; i < 3; i++) {
                color_0[i] = Fixed6{s8((color_0[i].Raw() >> 1) + (color_1[i].Raw() >> 1))};
              }

              return color_0;
            }
            if (index == 3) {
              return Color4{0, 0, 0, 0};
            }
            return Color4::FromRGB555(ReadPaletteVRAM<u16>(palette_addr + index * sizeof(u16)) & 0x7FFF);
          }
          case 2: {
            return Color4::FromRGB555(ReadPaletteVRAM<u16>(palette_addr + index * sizeof(u16)) & 0x7FFF);
          }
          default: {
            if (index == 2 || index == 3) {
              int coeff_0 = index == 2 ? 5 : 3;
              int coeff_1 = index == 2 ? 3 : 5;

              auto color_0 = Color4::FromRGB555(ReadPaletteVRAM<u16>(palette_addr + 0) & 0x7FFF);
              auto color_1 = Color4::FromRGB555(ReadPaletteVRAM<u16>(palette_addr + 2) & 0x7FFF);

              for (uint i = 0; i < 3; i++) {
                color_0[i] = Fixed6{s8(((color_0[i].Raw() * coeff_0) + (color_1[i].Raw() * coeff_1)) >> 3)};
              }

              return color_0;
            }
            return Color4::FromRGB555(ReadPaletteVRAM<u16>(palette_addr + index * sizeof(u16)) & 0x7FFF);
          }
        }
      }
      case TextureParams::Format::A5I3: {
        u8  value = ReadTextureVRAM<u8>(texture_addr + offset);
        int index = value & 7;
        int alpha = value >> 3;

        auto rgb555 = ReadPaletteVRAM<u16>((palette_base << 4) + index * sizeof(u16)) & 0x7FFF;
        auto rgb6666 = Color4::FromRGB555(rgb555);

        rgb6666.A() = (alpha << 1) | (alpha >> 4); // 5-bit alpha to 6-bit alpha
        return rgb6666;
      }
      case TextureParams::Format::Raw16BPP: {
        auto rgb1555 = ReadTextureVRAM<u16>(texture_addr + offset * sizeof(u16));
        auto rgb6666 = Color4::FromRGB555(rgb1555);

        rgb6666.A() = rgb6666.A().Raw() * (rgb1555 >> 15);
        return rgb6666;
      }
    };

    return {};
  }

  void SoftwareRenderer::RenderAntiAliasing() {
    for(int y = 0; y < 192; y++) {
      for(int x = 0; x < 256; x++) {
        const Color4 top    = m_frame_buffer[0][y][x];
        const Color4 bottom = m_frame_buffer[1][y][x];
        const int coverage  = m_coverage_buffer[y][x];

        if(bottom.A() == 0 || coverage == 63) {
          continue;
        }

        for(int i : {0, 1, 2, 3}) {
          m_frame_buffer[0][y][x][i] = (i8)((top[i].Raw() * coverage + bottom[i].Raw() * (64 - coverage)) >> 6);
        }
      }
    }
  }

} // namespace dual::nds::gpu
