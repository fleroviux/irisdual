
#include <dual/nds/video_unit/ppu/ppu.hpp>

namespace dual::nds {

  void PPU::RenderLayerOAM(u16 vcount) {
    static constexpr int k_obj_size[4][4][2] = {
      { { 8 , 8  }, { 16, 16 }, { 32, 32 }, { 64, 64 } }, // Square
      { { 16, 8  }, { 32, 8  }, { 32, 16 }, { 64, 32 } }, // Horizontal
      { { 8 , 16 }, { 8 , 32 }, { 16, 32 }, { 32, 64 } }, // Vertical
      { { 8 , 8  }, { 8 , 8  }, { 8 , 8  }, { 8 , 8  } }  // Prohibited
    };

    const auto& mmio = m_mmio_copy[vcount];

    s16 transform[4];

    int tile_num;
    u16 pixel;

    m_line_contains_alpha_obj = false;

    for(auto& point : m_buffer_obj) {
      point.priority = 4;
      point.color = k_color_transparent;
      point.alpha = 0;
      point.window = 0;
    }

    for(s32 offset = 0; offset <= 127 * 8; offset += 8) {
      // Check if OBJ is disabled (affine=0, attr0bit9=1)
      if((m_render_oam[offset + 1] & 3) == 2) {
        continue;
      }

      const u16 attr0 = atom::read<u16>(m_render_oam, offset + 0);
      const u16 attr1 = atom::read<u16>(m_render_oam, offset + 2);
      const u16 attr2 = atom::read<u16>(m_render_oam, offset + 4);

      int width;
      int height;

      s32 x = attr1 & 0x1FF;
      s32 y = attr0 & 0x0FF;
      int shape  =  attr0 >> 14;
      int size   =  attr1 >> 14;
      int prio   = (attr2 >> 10) & 3;
      int mode   = (attr0 >> 10) & 3;
      int mosaic = (attr0 >> 12) & 1;

      if(x >= 256) x -= 512;
      if(y >= 192) y -= 256;

      int affine  = (attr0 >> 8) & 1;
      int attr0b9 = (attr0 >> 9) & 1;

      // Decode OBJ width and height.
      width  = k_obj_size[shape][size][0];
      height = k_obj_size[shape][size][1];

      int half_width  = width / 2;
      int half_height = height / 2;

      // Set x and y to OBJ origin.
      x += half_width;
      y += half_height;

      // Load transform matrix.
      if(affine) {
        int group = ((attr1 >> 9) & 0x1F) << 5;

        // Read transform matrix.
        transform[0] = atom::read<s16>(m_render_oam, group + 0x6);
        transform[1] = atom::read<s16>(m_render_oam, group + 0xE);
        transform[2] = atom::read<s16>(m_render_oam, group + 0x16);
        transform[3] = atom::read<s16>(m_render_oam, group + 0x1E);

        // Check double-size flag. Doubles size of the view rectangle.
        if(attr0b9) {
          x += half_width;
          y += half_height;
          half_width  *= 2;
          half_height *= 2;
        }
      } else {
        /* Set transform to identity:
         * [ 1 0 ]
         * [ 0 1 ]
         */
        transform[0] = 0x100;
        transform[1] = 0;
        transform[2] = 0;
        transform[3] = 0x100;
      }

      // Bail out if scanline is outside OBJ's view rectangle.
      if(vcount < (y - half_height) || vcount >= (y + half_height)) {
        continue;
      }

      s16 local_y = (s16)(vcount - y);
      int number  =  attr2 & 0x3FF;
      int palette = (attr2 >> 12) + 16;
      int flip_h  = !affine && (attr1 & (1 << 12));
      int flip_v  = !affine && (attr1 & (1 << 13));
      int is_256  = (attr0 >> 13) & 1;

      int mosaic_x = 0;

      if(mosaic) {
        mosaic_x = (x - half_width) % mmio.mosaic.obj.size_x;
        local_y = (s16)(local_y - mmio.mosaic.obj.counter_y);
      }

      // Render OBJ scanline.
      for(int local_x = -half_width; local_x <= half_width; local_x++) {
        int _local_x = local_x - mosaic_x;
        int global_x = local_x + x;

        if(mosaic && (++mosaic_x == mmio.mosaic.obj.size_x)) {
          mosaic_x = 0;
        }

        if(global_x < 0 || global_x >= 256) {
          continue;
        }

        int tex_x = ((transform[0] * _local_x + transform[1] * local_y) >> 8) + (width / 2);
        int tex_y = ((transform[2] * _local_x + transform[3] * local_y) >> 8) + (height / 2);

        // Check if transformed coordinates are inside bounds.
        if(tex_x >= width || tex_y >= height ||
          tex_x < 0 || tex_y < 0) {
          continue;
        }

        if(flip_h) tex_x = width  - tex_x - 1;
        if(flip_v) tex_y = height - tex_y - 1;

        int tile_x  = tex_x % 8;
        int tile_y  = tex_y % 8;
        int block_x = tex_x / 8;
        int block_y = tex_y / 8;

        if(mode == OBJ_BITMAP) {
          // @todo: Attr 2, Bit 12-15 is used as Alpha-OAM value (instead of as palette setting).
          if(mmio.dispcnt.bitmap_obj_mapping == DisplayControl::Mapping::OneDimensional) {
            pixel = atom::read<u16>(m_render_vram_obj, (number * (64 << mmio.dispcnt.bitmap_obj_boundary) + tex_y * width + tex_x) * 2);
          } else {
            auto dimension = mmio.dispcnt.bitmap_obj_dimension;
            auto mask = (16 << dimension) - 1;

            pixel = atom::read<u16>(m_render_vram_obj, ((number & ~mask) * 64 + (number & mask) * 8 + tex_y * (128 << dimension) + tex_x) * 2);
          }

          if((pixel & 0x8000) == 0) {
            pixel = k_color_transparent;
          }
        } else if(is_256) {
          if(mmio.dispcnt.tile_obj_mapping == DisplayControl::Mapping::OneDimensional) {
            tile_num = (number << mmio.dispcnt.tile_obj_boundary) + block_y * (width / 4);
          } else {
            tile_num = (number & ~1) + block_y * 32;
          }

          tile_num += block_x * 2;

          pixel = DecodeTilePixel8BPP_OBJ(tile_num * 32, mmio.dispcnt.enable_extpal_obj, palette, tile_x, tile_y);
        } else {
          if(mmio.dispcnt.tile_obj_mapping == DisplayControl::Mapping::OneDimensional) {
            tile_num = (number << mmio.dispcnt.tile_obj_boundary) + block_y * (width / 8);
          } else {
            tile_num = number + block_y * 32;
          }

          tile_num += block_x;

          pixel = DecodeTilePixel4BPP_OBJ(tile_num * 32, palette, tile_x, tile_y);
        }

        auto& point = m_buffer_obj[global_x];

        if(pixel != k_color_transparent) {
          if(mode == OBJ_WINDOW) {
            point.window = 1;
          } else if(prio < point.priority) {
            point.priority = prio;
            point.color = pixel;
            point.alpha = (mode == OBJ_SEMI) ? 1 : 0;
            if(point.alpha) {
              m_line_contains_alpha_obj = true;
            }
          }
        }
      }
    }
  }

} // namespace dual::nds
