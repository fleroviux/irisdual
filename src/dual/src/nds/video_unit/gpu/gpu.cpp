
#include <dual/nds/video_unit/gpu/renderer/mgpu_hardware_renderer.hpp>
#include <dual/nds/video_unit/gpu/renderer/software_renderer.hpp>
#include <dual/nds/video_unit/gpu/gpu.hpp>

namespace dual::nds {

  GPU::GPU(
    Scheduler& scheduler,
    IRQ& arm9_irq,
    arm9::DMA& arm9_dma,
    const VRAM& vram
  )   : m_cmd_processor{scheduler, arm9_irq, arm9_dma, m_io, m_geometry_engine}
      , m_geometry_engine{m_io} {
    //m_renderer = std::make_unique<gpu::SoftwareRenderer>(m_io, vram.region_gpu_texture, vram.region_gpu_palette);
    m_renderer = std::make_unique<gpu::MGPUHardwareRenderer>(m_io, vram.region_gpu_texture, vram.region_gpu_palette);
  }

  void GPU::Reset() {
    m_io = {};
    m_cmd_processor.Reset();
    m_geometry_engine.Reset();

    m_render_engine_power_on = false;
    m_geometry_engine_power_on = false;

    m_renderer->UpdateEdgeColor(0u, m_io.edge_color);
    m_renderer->UpdateToonTable(0u, m_io.toon_table);
  }

} // namespace dual::nds
