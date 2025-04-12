
#include <dual/nds/video_unit/gpu/renderer/mgpu_texture_cache.hpp>

// TODO(fleroviux): do not duplicate this across multiple files
#define MGPU_CHECK(result_expression) \
  do { \
    MGPUResult result = result_expression; \
    if(result != MGPU_SUCCESS) \
      ATOM_PANIC("MGPU error: {} ({})", "" # result_expression, mgpuResultCodeToString(result)); \
  } while(0)

namespace dual::nds::gpu {

MGPUTextureCache::MGPUTextureCache(
  MGPUDevice mgpu_device,
  const Region<4, 131072>& vram_texture,
  const Region<8>& vram_palette
)   : m_mgpu_device{mgpu_device}
    , m_vram_texture{vram_texture}
    , m_vram_palette{vram_palette} {
  m_mgpu_queue = mgpuDeviceGetQueue(mgpu_device, MGPU_QUEUE_TYPE_GRAPHICS_COMPUTE);
}

MGPUTextureCache::~MGPUTextureCache() {
  for(auto& [_, entry] : m_texture_cache) {
    mgpuTextureViewDestroy(entry.texture_view);
    mgpuTextureDestroy(entry.texture);
  }
}

MGPUTextureView MGPUTextureCache::GetOrCreate(const TextureParams& params, u32 palette_base) {
  const u64 key = params.word | (u64)palette_base << 32;

  if(auto match = m_texture_cache.find(key); match != m_texture_cache.end()) {
    return match->second.texture_view;
  }

  const int width  = 8 << params.log2_s_size;
  const int height = 8 << params.log2_t_size;

  const auto data = new u32[width * height];

  using Format = TextureParams::Format;

  switch((Format)params.format) {
    case Format::Disabled: break;
    case Format::A3I5:          Decode_A3I5(width, height, params, palette_base, data); break;
    case Format::A5I3:          Decode_A5I3(width, height, params, palette_base, data); break;
    case Format::Palette2BPP:   Decode_Palette2BPP(width, height, params, palette_base, data); break;
    case Format::Palette4BPP:   Decode_Palette4BPP(width, height, params, palette_base, data); break;
    case Format::Palette8BPP:   Decode_Palette8BPP(width, height, params, palette_base, data); break;
    case Format::Compressed4x4: Decode_Compressed4x4(width, height, params, palette_base, data); break;
    case Format::Raw16BPP:      Decode_Direct(width, height, params, data); break;
  }

  const MGPUTextureCreateInfo texture_create_info{
    .format = MGPU_TEXTURE_FORMAT_B8G8R8A8_SRGB,
    .type = MGPU_TEXTURE_TYPE_2D,
    .extent = {
      .width = (u32)width,
      .height = (u32)height,
      .depth = 1u
    },
    .mip_count = 1u,
    .array_layer_count = 1u,
    .usage = MGPU_TEXTURE_USAGE_SAMPLED | MGPU_TEXTURE_USAGE_COPY_DST
  };

  MGPUTexture mgpu_texture{};
  MGPU_CHECK(mgpuDeviceCreateTexture(m_mgpu_device, &texture_create_info, &mgpu_texture));

  const MGPUTextureViewCreateInfo texture_view_create_info{
    .type = MGPU_TEXTURE_VIEW_TYPE_2D,
    .format = MGPU_TEXTURE_FORMAT_B8G8R8A8_SRGB,
    .aspect = MGPU_TEXTURE_ASPECT_COLOR,
    .base_mip = 0u,
    .mip_count = 1u,
    .base_array_layer = 0u,
    .array_layer_count = 1u
  };

  MGPUTextureView mgpu_texture_view{};
  MGPU_CHECK(mgpuTextureCreateView(mgpu_texture, &texture_view_create_info, &mgpu_texture_view));

  const MGPUTextureUploadRegion texture_upload_region{
    .offset = {.x = 0u, .y = 0u, .z = 0u},
    .extent = texture_create_info.extent,
    .mip_level = 0u,
    .base_array_layer = 0u,
    .array_layer_count = 1u
  };
  MGPU_CHECK(mgpuQueueTextureUpload(m_mgpu_queue, mgpu_texture, &texture_upload_region, data));

  delete[] data;
  m_texture_cache[key] = {mgpu_texture, mgpu_texture_view};
  return mgpu_texture_view;
}

void MGPUTextureCache::Decode_A3I5(int width, int height, const TextureParams& params, u32 palette_base, u32 *data) {
  const int texels  = width * height;
  const u32 palette_address = palette_base << 4;
  u32 texture_address = params.vram_offset_div_8 << 3;

  for(int i = 0; i < texels; i++) {
    const u8 texel = m_vram_texture.Read<u8>(texture_address);
    const int index = texel & 31;
    const u16 rgb555 = m_vram_palette.Read<u16>(palette_address + index * sizeof(u16)) & 0x7FFF;
    int alpha = texel >> 5;

    // 3-bit alpha to 8-bit alpha conversion
    alpha = (alpha << 5) | (alpha << 2) | (alpha >> 1);

    *data++ = ConvertColor(rgb555, alpha);

    texture_address++;
  }
}

void MGPUTextureCache::Decode_A5I3(int width, int height, const TextureParams& params, u32 palette_base, u32 *data) {
  const int texels  = width * height;
  const u32 palette_address = palette_base << 4;
  u32 texture_address = params.vram_offset_div_8 << 3;

  for(int i = 0; i < texels; i++) {
    const u8 texel = m_vram_texture.Read<u8>(texture_address);
    const int index = texel & 7;
    const u16 rgb555 = m_vram_palette.Read<u16>(palette_address + index * sizeof(u16)) & 0x7FFF;
    int alpha = texel >> 3;

    // 5-bit alpha to 8-bit alpha conversion
    alpha = (alpha << 3) | (alpha >> 2);

    *data++ = ConvertColor(rgb555, alpha);

    texture_address++;
  }
}

void MGPUTextureCache::Decode_Palette2BPP(int width, int height, const TextureParams& params, u32 palette_base, u32* data) {
  const int texels  = width * height;
  const u32 palette_address = palette_base << 3;
  const bool color0_transparent = params.color0_transparent;
  u32 texture_address = params.vram_offset_div_8 << 3;

  // @todo: think about fetching 32 pixels (64-bits) at once
  for(int i = 0; i < texels; i += 4) {
    u8 indices = m_vram_texture.Read<u8>(texture_address);

    for(int j = 0; j < 4; j++) {
      const int index = indices & 2;

      // @todo: do 2BPP textures actually honor the color0_transparent flag?
      if(color0_transparent && index == 0) {
        *data++ = 0;
      } else {
        *data++ = ConvertColor(m_vram_palette.Read<u16>(palette_address + index * sizeof(u16)) & 0x7FFF);
      }

      indices >>= 2;
    }

    texture_address++;
  }
}

void MGPUTextureCache::Decode_Palette4BPP(int width, int height, const TextureParams& params, u32 palette_base, u32* data) {
  const int texels  = width * height;
  const u32 palette_address = palette_base << 4;
  const bool color0_transparent = params.color0_transparent;
  u32 texture_address = params.vram_offset_div_8 << 3;

  // @todo: think about fetching 16 pixels (64-bits) at once
  for(int i = 0; i < texels; i += 2) {
    u8 indices = m_vram_texture.Read<u8>(texture_address);

    for(int j = 0; j < 2; j++) {
      const int index = indices & 15;

      if(color0_transparent && index == 0) {
        *data++ = 0;
      } else {
        *data++ = ConvertColor(m_vram_palette.Read<u16>(palette_address + index * sizeof(u16)) & 0x7FFF);
      }

      indices >>= 4;
    }

    texture_address++;
  }
}

void MGPUTextureCache::Decode_Palette8BPP(int width, int height, const TextureParams& params, u32 palette_base, u32* data) {
  const int texels  = width * height;
  const u32 palette_address = palette_base << 4;
  const bool color0_transparent = params.color0_transparent;
  u32 texture_address = params.vram_offset_div_8 << 3;

  // @todo: think about fetching 8 pixels (64-bits) at once
  for(int i = 0; i < texels; i++) {
    const u8 index = m_vram_texture.Read<u8>(texture_address);

    if(color0_transparent && index == 0) {
      *data++ = 0;
    } else {
      *data++ = ConvertColor(m_vram_palette.Read<u16>(palette_address + index * sizeof(u16)) & 0x7FFF);
    }

    texture_address++;
  }
}

void MGPUTextureCache::Decode_Compressed4x4(int width, int height, const TextureParams& params, u32 palette_base, u32* data) {
  const u32 palette_address = palette_base << 4;
  u32 texture_address = params.vram_offset_div_8 << 3;

  const int rows = width >> 2;

  for(int block_y = 0; block_y < height; block_y += 4) {
    for(int block_x = 0; block_x < width; block_x += 4) {
      const u32 block_data_address = texture_address + block_y * rows + block_x;

      const u32 block_data_slot_index  = block_data_address >> 18;
      const u32 block_data_slot_offset = block_data_address & 0x1FFFF;
      const u32 block_info_address = 0x20000 + (block_data_slot_offset >> 1) + (block_data_slot_index * 0x10000);

      u32 block_data = m_vram_texture.Read<u32>(block_data_address);
      const u16 block_info = m_vram_texture.Read<u16>(block_info_address);

      // decode block information
      const int palette_offset = block_info & 0x3FFF;
      const int blend_mode = block_info >> 14;

      const u32 final_palette_address = palette_address + palette_offset * sizeof(u32);

      const auto FetchPRAM = [&](uint index) {
        return ConvertColor(m_vram_palette.Read<u16>(final_palette_address + index * sizeof(u16)) & 0x7FFF);
      };

      const auto Mix = [&](u32 color0, u32 color1, int alpha0, int alpha1, int shift) -> u32 {
        int r0 = (int)(color0 >> 16) & 0xFF;
        int g0 = (int)(color0 >>  8) & 0xFF;
        int b0 = (int)(color0 >>  0) & 0xFF;

        int r1 = (int)(color1 >> 16) & 0xFF;
        int g1 = (int)(color1 >>  8) & 0xFF;
        int b1 = (int)(color1 >>  0) & 0xFF;

        int ro = (r0 * alpha0 + r1 * alpha1) >> shift;
        int go = (g0 * alpha0 + g1 * alpha1) >> shift;
        int bo = (b0 * alpha0 + b1 * alpha1) >> shift;

        return 0xFF000000 | ro << 16 | go << 8 | bo;
      };

      u32 palette[4];
      palette[0] = FetchPRAM(0);
      palette[1] = FetchPRAM(1);

      switch(blend_mode) {
        case 0: {
          palette[2] = FetchPRAM(2);
          palette[3] = 0;
          break;
        }
        case 1: {
          palette[2] = Mix(palette[0], palette[1], 1, 1, 1);
          palette[3] = 0;
          break;
        }
        case 2: {
          palette[2] = FetchPRAM(2);
          palette[3] = FetchPRAM(3);
          break;
        }
        case 3: {
          palette[2] = Mix(palette[0], palette[1], 5, 3, 3);
          palette[3] = Mix(palette[0], palette[1], 3, 5, 3);
          break;
        }
      }

      for(int inner_y = 0; inner_y < 4; inner_y++) {
        u32* row_data = &data[(block_y + inner_y) * width + block_x];

        for(int inner_x = 0; inner_x < 4; inner_x++) {
          *row_data++ = palette[block_data & 3];
          block_data >>= 2;
        }
      }
    }
  }
}

void MGPUTextureCache::Decode_Direct(int width, int height, const TextureParams& params, u32* data) {
  const int texels = width * height;
  u32 texture_address = params.vram_offset_div_8 << 3;

  for(int i = 0; i < texels; i++) {
    const u16 abgr1555 = m_vram_texture.Read<u16>(texture_address);
    *data++ = ConvertColor(abgr1555, (abgr1555 >> 15) * 255);
    texture_address += sizeof(u16);
  }
}

void MGPUTextureCache::RegisterVRAMMapUnmapHandlers() {
  const auto OnMapUnmap = [&](u32 offset, size_t size) {
    // this is the poor girl's cache invalidation.
    // @todo: do not invalidate textures which aren't affected.
    for(auto& [_, entry] : m_texture_cache) {
      mgpuTextureViewDestroy(entry.texture_view);
      mgpuTextureDestroy(entry.texture);
    }

    m_texture_cache.clear();
  };

  m_vram_texture.AddCallback(OnMapUnmap);
  m_vram_palette.AddCallback(OnMapUnmap);
}

} // namespace dual::nds::gpu
