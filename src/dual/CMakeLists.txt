cmake_minimum_required(VERSION 3.2)
project(dual-core CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
  src/arm/interpreter/tablegen/tablegen.cpp
  src/arm/interpreter/interpreter_cpu.cpp
  src/common/scheduler.cpp
  src/nds/arm7/apu.cpp
  src/nds/arm7/dma.cpp
  src/nds/arm7/io.cpp
  src/nds/arm7/memory.cpp
  src/nds/arm7/rtc.cpp
  src/nds/arm7/spi.cpp
  src/nds/arm7/touch_screen.cpp
  src/nds/arm7/wifi.cpp
  src/nds/arm9/cp15.cpp
  src/nds/arm9/dma.cpp
  src/nds/arm9/io.cpp
  src/nds/arm9/math.cpp
  src/nds/arm9/memory.cpp
  src/nds/backup/eeprom.cpp
  src/nds/backup/eeprom512b.cpp
  src/nds/backup/flash.cpp
  src/nds/video_unit/gpu/command_processor/command_processor.cpp
  src/nds/video_unit/gpu/command_processor/geometry.cpp
  src/nds/video_unit/gpu/command_processor/matrix.cpp
  src/nds/video_unit/gpu/renderer/software/edge_marking.cpp
  src/nds/video_unit/gpu/renderer/software/fog.cpp
  src/nds/video_unit/gpu/renderer/software/rasterizer.cpp
  src/nds/video_unit/gpu/renderer/software/software_renderer.cpp
  src/nds/video_unit/gpu/geometry_engine.cpp
  src/nds/video_unit/gpu/gpu.cpp
  src/nds/video_unit/ppu/render/affine.cpp
  src/nds/video_unit/ppu/render/oam.cpp
  src/nds/video_unit/ppu/render/text.cpp
  src/nds/video_unit/ppu/render/window.cpp
  src/nds/video_unit/ppu/composer.cpp
  src/nds/video_unit/ppu/ppu.cpp
  src/nds/video_unit/video_unit.cpp
  src/nds/cartridge.cpp
  src/nds/ipc.cpp
  src/nds/irq.cpp
  src/nds/nds.cpp
  src/nds/swram.cpp
  src/nds/timer.cpp
  src/nds/vram.cpp
)

set(HEADERS
  src/arm/interpreter/handlers/arithmetic.inl
  src/arm/interpreter/handlers/handler16.inl
  src/arm/interpreter/handlers/handler32.inl
  src/arm/interpreter/handlers/memory.inl
  src/arm/interpreter/tablegen/decoder.hpp
  src/arm/interpreter/tablegen/gen_arm.hpp
  src/arm/interpreter/tablegen/gen_thumb.hpp
  src/arm/interpreter/interpreter_cpu.hpp
  src/nds/video_unit/gpu/renderer/software/edge.hpp
  src/nds/video_unit/gpu/renderer/software/interpolator.hpp
)

set(HEADERS_PUBLIC
  include/dual/arm/coprocessor.hpp
  include/dual/arm/cpu.hpp
  include/dual/arm/memory.hpp
  include/dual/common/backup_file.hpp
  include/dual/common/fifo.hpp
  include/dual/common/scheduler.hpp
  include/dual/nds/arm7/apu.hpp
  include/dual/nds/arm7/dma.hpp
  include/dual/nds/arm7/memory.hpp
  include/dual/nds/arm7/rtc.hpp
  include/dual/nds/arm7/spi.hpp
  include/dual/nds/arm7/touch_screen.hpp
  include/dual/nds/arm7/wifi.hpp
  include/dual/nds/arm9/cp15.hpp
  include/dual/nds/arm9/dma.hpp
  include/dual/nds/arm9/math.hpp
  include/dual/nds/arm9/memory.hpp
  include/dual/nds/backup/eeprom.hpp
  include/dual/nds/backup/eeprom512b.hpp
  include/dual/nds/backup/flash.hpp
  include/dual/nds/video_unit/gpu/renderer/renderer_base.hpp
  include/dual/nds/video_unit/gpu/renderer/software_renderer.hpp
  include/dual/nds/video_unit/gpu/command_processor.hpp
  include/dual/nds/video_unit/gpu/geometry_engine.hpp
  include/dual/nds/video_unit/gpu/gpu.hpp
  include/dual/nds/video_unit/gpu/math.hpp
  include/dual/nds/video_unit/gpu/registers.hpp
  include/dual/nds/video_unit/ppu/ppu.hpp
  include/dual/nds/video_unit/ppu/registers.hpp
  include/dual/nds/video_unit/video_unit.hpp
  include/dual/nds/vram/region.hpp
  include/dual/nds/vram/vram.hpp
  include/dual/nds/cartridge.hpp
  include/dual/nds/header.hpp
  include/dual/nds/nds.hpp
  include/dual/nds/rom.hpp
  include/dual/nds/swram.hpp
  include/dual/nds/system_memory.hpp
  include/dual/nds/timer.hpp
)

option(DUAL_ENABLE_JIT "Enable Just-In-Time compiler support" ON)

if(DUAL_ENABLE_JIT)
  list(APPEND SOURCES src/arm/jit/lunatic_cpu.cpp)
  list(APPEND HEADERS src/arm/jit/lunatic_cpu.hpp)
endif()

add_library(dual ${SOURCES} ${HEADERS} ${HEADERS_PUBLIC})

target_link_libraries(dual PUBLIC atom-common atom-logger atom-math)
if(DUAL_ENABLE_JIT)
  target_link_libraries(dual PRIVATE lunatic)
  target_compile_definitions(dual PUBLIC DUAL_ENABLE_JIT)
endif()

target_include_directories(dual PUBLIC include)
target_include_directories(dual PRIVATE src)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  target_compile_options(dual PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fbracket-depth=8192>)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  target_compile_options(dual PRIVATE /bigobj)
endif()
