
#pragma once

#include <atom/integer.hpp>
#include <atom/punning.hpp>
#include <atomic>
#include <condition_variable>
#include <dual/nds/video_unit/gpu/gpu.hpp>
#include <dual/nds/video_unit/ppu/registers.hpp>
#include <dual/nds/vram/vram.hpp>
#include <dual/nds/system_memory.hpp>
#include <functional>
#include <mutex>
#include <thread>

namespace dual::nds {

  /* 2D picture processing unit (PPU).
   * The Nintendo DS has two PPUs (PPU A and PPU B), one for each screen.
   */
  class PPU {
    public:
      PPU(
        int id,
        SystemMemory& memory,
        GPU* gpu = nullptr
      );

     ~PPU();

      struct MMIO {
        DisplayControl dispcnt;

        BackgroundControl bgcnt[4]{};
        BackgroundOffset bghofs[4];
        BackgroundOffset bgvofs[4];
        RotateScaleParameter bgpa[2];
        RotateScaleParameter bgpb[2];
        RotateScaleParameter bgpc[2];
        RotateScaleParameter bgpd[2];
        ReferencePoint bgx[2];
        ReferencePoint bgy[2];

        WindowRange winh[2];
        WindowRange winv[2];
        WindowLayerSelect winin;
        WindowLayerSelect winout;

        BlendControl bldcnt;
        BlendAlpha bldalpha;
        BlendBrightness bldy;

        Mosaic mosaic;

        MasterBrightness master_bright;

        bool capture_bg_and_3d;
      } m_mmio;

      void Reset();

      [[nodiscard]] const u32* GetFrameBuffer() const {
        return &m_frame_buffer[m_frame][0];
      }

      [[nodiscard]] const u16* GetLayerMergeOutput() const {
        return &m_buffer_compose[0];
      }

      void SwapBuffers() {
        m_frame ^= 1;
      }

      void WaitForRenderWorker() {
        while(m_render_worker.vcount <= m_render_worker.vcount_max) {}
      }

      void OnWriteVRAM_BG(size_t address_lo, size_t address_hi) {
        OnRegionWrite(m_vram_bg, m_render_vram_bg, m_vram_bg_dirty, {address_lo, address_hi});
      }

      void OnWriteVRAM_OBJ(size_t address_lo, size_t address_hi) {
        OnRegionWrite(m_vram_obj, m_render_vram_obj, m_vram_obj_dirty, {address_lo, address_hi});
      }

      void OnWriteExtPal_BG(size_t address_lo, size_t address_hi) {
        OnRegionWrite(m_extpal_bg, m_render_extpal_bg, m_extpal_bg_dirty, {address_lo, address_hi});
      }

      void OnWriteExtPal_OBJ(size_t address_lo, size_t address_hi) {
        OnRegionWrite(m_extpal_obj, m_render_extpal_obj, m_extpal_obj_dirty, {address_lo, address_hi});
      }

      void OnWriteVRAM_LCDC(size_t address_lo, size_t address_hi) {
        OnRegionWrite(m_vram_lcdc, m_render_vram_lcdc, m_vram_lcdc_dirty, {address_lo, address_hi});
      }

      void OnWritePRAM(size_t address_lo, size_t address_hi) {
        OnRegionWrite(m_pram, m_render_pram, m_pram_dirty, {address_lo, address_hi});
      }

      void OnWriteOAM(size_t address_lo, size_t address_hi) {
        OnRegionWrite(m_oam, m_render_oam, m_oam_dirty, {address_lo, address_hi});
      }

      void OnDrawScanlineBegin(u16 vcount, bool capture_bg_and_3d);
      void OnDrawScanlineEnd();
      void OnBlankScanlineBegin(u16 vcount);

      [[nodiscard]] bool GetPowerOn() const {
        return m_power_on;
      }

      void SetPowerOn(bool power_on) {
        m_power_on = power_on;
      }

    private:
      enum ObjectMode {
        OBJ_NORMAL = 0,
        OBJ_SEMI   = 1,
        OBJ_WINDOW = 2,
        OBJ_BITMAP = 3
      };

      enum Layer {
        LAYER_BG0 = 0,
        LAYER_BG1 = 1,
        LAYER_BG2 = 2,
        LAYER_BG3 = 3,
        LAYER_OBJ = 4,
        LAYER_SFX = 5,
        LAYER_BD  = 5
      };

      enum Enable {
        ENABLE_BG0 = 0,
        ENABLE_BG1 = 1,
        ENABLE_BG2 = 2,
        ENABLE_BG3 = 3,
        ENABLE_OBJ = 4,
        ENABLE_WIN0 = 5,
        ENABLE_WIN1 = 6,
        ENABLE_OBJWIN = 7
      };

      struct AddressRange {
        size_t lo = std::numeric_limits<size_t>::max();
        size_t hi = 0;

        void Expand(const AddressRange& other) {
          lo = std::min(lo, other.lo);
          hi = std::max(hi, other.hi);
        }
      };

      void AffineRenderLoop(
        u16 vcount,
        uint id,
        int  width,
        int  height,
        const std::function<void(int, int, int)>& render_func
      );

      void RenderScanline(u16 vcount, bool capture_bg_and_3d);
      void RenderDisplayOff(u16 vcount);
      void RenderNormal(u16 vcount);
      void RenderVideoMemoryDisplay(u16 vcount);
      void RenderMainMemoryDisplay(u16 vcount);
      void RenderBackgroundsAndComposite(u16 vcount);
      void RenderMasterBrightness(int vcount);

      void RenderLayerText(uint id, u16 vcount);
      void RenderLayerAffine(uint id, u16 vcount);
      void RenderLayerExtended(uint id, u16 vcount);
      void RenderLayerLarge(u16 vcount);
      void RenderLayerOAM(u16 vcount);
      void RenderWindow(uint id, u8 vcount);

      template<bool window, bool blending, bool opengl>
      void ComposeScanlineTmpl(u16 vcount, int bg_min, int bg_max);
      void ComposeScanline(u16 vcount, int bg_min, int bg_max);
      u16  AlphaBlend(u16 color_a, u16 color_b, int eva, int evb);
      u16  Brighten(u16 color, int evy);
      u16  Darken(u16 color, int evy);

      void SetupRenderWorker();
      void StopRenderWorker();
      void SubmitScanline(u16 vcount, bool capture_bg_and_3d);
      void RegisterMapUnmapCallbacks();

      u16 ReadPalette(uint palette, uint index) {
        return atom::read<u16>(m_render_pram, palette << 5 | index << 1) & 0x7FFFu;
      }

      void DecodeTileLine4BPP(u16* buffer, u32 base, uint palette, uint number, uint y, bool flip) {
        int xor_x = flip ? 7 : 0;
        u32 data  = atom::read<u32>(m_render_vram_bg, base + (number << 5 | y << 2));

        for(int x = 0; x < 8; x++) {
          uint index = data & 15;

          buffer[x ^ xor_x] = index == 0u ? k_color_transparent : ReadPalette(palette, index);
          data >>= 4;
        }
      }

      void DecodeTileLine8BPP(u16* buffer, u32 base, bool enable_extpal, uint palette, uint extpal_slot, uint number, uint y, bool flip) {
        int xor_x = flip ? 7 : 0;
        u64 data  = atom::read<u64>(m_render_vram_bg, base + (number << 6 | y << 3));

        for(uint x = 0; x < 8; x++) {
          uint index = data & 0xFF;

          if(index == 0) {
            buffer[x ^ xor_x] = k_color_transparent;
          } else if(enable_extpal) {
            buffer[x ^ xor_x] = atom::read<u16>(m_render_extpal_bg, extpal_slot << 13 | palette << 9 | index << 1);
          } else {
            buffer[x ^ xor_x] = ReadPalette(0, index);
          }

          data >>= 8;
        }
      }

      u16 DecodeTilePixel4BPP_OBJ(u32 address, uint palette, int x, int y) {
        u8 tuple = atom::read<u8>(m_render_vram_obj, address + (y << 2 | x >> 1));
        u8 index = (x & 1) ? (tuple >> 4) : (tuple & 0xF);

        if(index == 0) {
          return k_color_transparent;
        } else {
          return ReadPalette(palette, index);
        }
      }

      u16 DecodeTilePixel8BPP_BG(u32 address, bool enable_extpal, uint palette, uint extpal_slot, int x, int y) {
        const u8 index = atom::read<u8>(m_render_vram_bg, address + (y << 3) + x);
        if(index == 0) {
          return k_color_transparent;
        }
        if(enable_extpal) {
          return atom::read<u16>(m_render_extpal_bg, extpal_slot << 13 | palette << 9 | index << 1);
        }
        return ReadPalette(0, index);
      }

      u16 DecodeTilePixel8BPP_OBJ(u32 address, bool enable_extpal, uint palette, int x, int y) {
        const u8 index = atom::read<u8>(m_render_vram_obj, address + (y << 3) + x);
        if(index == 0) {
          return k_color_transparent;
        }
        if(enable_extpal) {
          return atom::read<u16>(m_render_extpal_obj, (palette << 9 | index << 1) & 0x1FFF);
        }
        return ReadPalette(16, index);
      }

      static u32 ConvertColor(u16 color) {
        u32 r = (color >>  0) & 0x1F;
        u32 g = (color >>  5) & 0x1F;
        u32 b = (color >> 10) & 0x1F;

        return r << 19 | g << 11 | b << 3 | 0xFF000000;
      }

      template<typename T>
      static void CopyVRAM(const T& src, u8* dst, const AddressRange& range) {
        for(size_t address = range.lo; address < range.hi; address++) {
          dst[address] = src.template Read<u8>(address);
        }
      }

      static void CopyVRAM(const u8* src, u8* dst, const AddressRange& range) {
        for(size_t address = range.lo; address < range.hi; address++) {
          dst[address] = src[address];
        }
      }

      template<typename T>
      void OnRegionWrite(const T& region, u8* copy_dst, AddressRange& dirty_range, const AddressRange& write_range) {
        if(m_vcount < 192) {
          WaitForRenderWorker();
          CopyVRAM(region, copy_dst, write_range);
        } else {
          dirty_range.Expand(write_range);
        }
      }

      u32 m_frame_buffer[2][256 * 192];
      u16 m_buffer_compose[256];
      u16 m_buffer_bg[4][256];
      bool m_buffer_win[2][256];
      bool m_window_scanline_enable[2];
      int m_buffer_3d_alpha[256];

      struct ObjectPixel {
        u16 color;
        u8  priority;
        unsigned alpha  : 1;
        unsigned window : 1;
      } m_buffer_obj[256];

      bool m_line_contains_alpha_obj = false;

      struct RenderWorker {
        std::atomic_int vcount;
        std::atomic_int vcount_max;
        std::atomic_bool running = false;
        std::condition_variable cv;
        std::mutex mutex;
        bool ready;
        std::thread thread;
      } m_render_worker;

      MMIO m_mmio_copy[263];

      const Region<32>& m_vram_bg;  //< Background tile, map and bitmap data
      const Region<16>& m_vram_obj; //< OBJ tile and bitmap data
      const Region<4, 8192>& m_extpal_bg;  //< Background extended palette data
      const Region<1, 8192>& m_extpal_obj; //< OBJ extended palette data
      const Region<64>& m_vram_lcdc; //< LCDC mapped VRAM
      const u8* m_pram; //< Palette RAM
      const u8* m_oam;  //< Object Attribute Map

      // Copies of VRAM, PRAM and OAM read by the rendering thread:
      u8 m_render_vram_bg[524288];
      u8 m_render_vram_obj[262144];
      u8 m_render_extpal_bg[32768];
      u8 m_render_extpal_obj[8192];
      u8 m_render_vram_lcdc[1048576];
      u8 m_render_pram[0x400];
      u8 m_render_oam[0x400];

      // Lowest and highest dirty VRAM addresses
      AddressRange m_vram_bg_dirty;
      AddressRange m_vram_obj_dirty;
      AddressRange m_extpal_bg_dirty;
      AddressRange m_extpal_obj_dirty;
      AddressRange m_vram_lcdc_dirty;
      AddressRange m_pram_dirty;
      AddressRange m_oam_dirty;

      int m_vcount;
      int m_frame = 0;

      bool m_power_on{};

      GPU* m_gpu{};

      static constexpr u16 k_color_transparent = 0x8000u;
  };

} // namespace dual::nds
