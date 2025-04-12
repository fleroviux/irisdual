
#pragma once

#include <atom/integer.hpp>
#include <dual/nds/video_unit/gpu/geometry_engine.hpp>
#include <dual/nds/vram/region.hpp>
#include <mgpu/mgpu.h>
#include <unordered_map>

namespace dual::nds::gpu {

class MGPUTextureCache {
  public:
    MGPUTextureCache(
      MGPUDevice mgpu_device,
      const Region<4, 131072>& vram_texture,
      const Region<8>& vram_palette
    );

   ~MGPUTextureCache();

    MGPUTextureView GetOrCreate(const TextureParams& params, u32 palette_base);

  private:
    struct Entry {
      MGPUTexture texture;
      MGPUTextureView texture_view;
    };

    // @fixme: this more or less is a copy from ppu.hpp:
    static auto ConvertColor(u16 color, u8 alpha = 0xFF) -> u32 {
      const u32 r = (color >>  0) & 0x1F;
      const u32 g = (color >>  5) & 0x1F;
      const u32 b = (color >> 10) & 0x1F;

      return r << 19 | g << 11 | b << 3 | (alpha << 24);
    }

    void Decode_A3I5(int width, int height, const TextureParams& params, u32 palette_base, u32* data);
    void Decode_A5I3(int width, int height, const TextureParams& params, u32 palette_base, u32* data);
    void Decode_Palette2BPP(int width, int height, const TextureParams& params, u32 palette_base, u32* data);
    void Decode_Palette4BPP(int width, int height, const TextureParams& params, u32 palette_base, u32* data);
    void Decode_Palette8BPP(int width, int height, const TextureParams& params, u32 palette_base, u32* data);
    void Decode_Compressed4x4(int width, int height, const TextureParams& params, u32 palette_base, u32* data);
    void Decode_Direct(int width, int height, const TextureParams& params, u32* data);

    void RegisterVRAMMapUnmapHandlers();

    MGPUDevice m_mgpu_device;
    MGPUQueue m_mgpu_queue{};
    const Region<4, 131072>& m_vram_texture;
    const Region<8>& m_vram_palette;
    std::unordered_map<u64, Entry> m_texture_cache{};
};

} // namespace dual::nds::gpu
