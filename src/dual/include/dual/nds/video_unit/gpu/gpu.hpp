
#pragma once

#include <dual/common/scheduler.hpp>
#include <dual/nds/arm9/dma.hpp>
#include <dual/nds/video_unit/gpu/command_processor.hpp>
#include <dual/nds/video_unit/gpu/geometry_engine.hpp>
#include <dual/nds/video_unit/gpu/registers.hpp>
#include <dual/nds/vram/vram.hpp>
#include <dual/nds/irq.hpp>

namespace dual::nds {

  // 3D graphics processing unit (GPU)
  class GPU {
    public:
      GPU(
        Scheduler& scheduler,
        IRQ& arm9_irq,
        arm9::DMA& arm9_dma,
        const VRAM& vram
      );

      void Reset();

      [[nodiscard]] u32 Read_DISP3DCNT() const {
        return m_io.disp3dcnt.half;
      }

      void Write_DISP3DCNT(u16 value, u16 mask) {
        const u16 write_mask = 0x4FFFu & mask;

        m_io.disp3dcnt.half = (value & write_mask) | (m_io.disp3dcnt.half & ~write_mask);

        if(value & mask & 0x2000u) {
          m_io.disp3dcnt.poly_or_vert_ram_overflow = false;
        }
      }

      void Write_GXFIFO(u32 word) {
        m_cmd_processor.Write_GXFIFO(word);
      }

      void Write_GXCMDPORT(u32 address, u32 param) {
        m_cmd_processor.Write_GXCMDPORT(address, param);
      }

      [[nodiscard]] u32 Read_GXSTAT() const {
        return m_cmd_processor.Read_GXSTAT();
      }

      void Write_GXSTAT(u32 value, u32 mask) {
        m_cmd_processor.Write_GXSTAT(value, mask);
      }

      void SwapBuffers() {
        m_cmd_processor.SwapBuffers();
      }

      [[nodiscard]] bool GetRenderEnginePowerOn() const {
        return m_render_engine_power_on;
      }

      void SetRenderEnginePowerOn(bool power_on) {
        m_render_engine_power_on = power_on;
      }

      [[nodiscard]] bool GetGeometryEnginePowerOn() const {
        return m_geometry_engine_power_on;
      }

      void SetGeometryEnginePowerOn(bool power_on) {
        m_geometry_engine_power_on = power_on;
      }

    private:
      gpu::IO m_io;

      arm9::DMA& m_arm9_dma;
      const Region<4, 131072>& m_vram_texture;
      const Region<8>& m_vram_palette;

      gpu::CommandProcessor m_cmd_processor;
      gpu::GeometryEngine m_geometry_engine;

      bool m_render_engine_power_on{};
      bool m_geometry_engine_power_on{};
  };

} // namespace dual::nds
