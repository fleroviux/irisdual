
#pragma once

#include <atom/bit.hpp>
#include <atom/integer.hpp>
#include <array>

namespace dual::nds::gpu {

  union DISP3DCNT {
    atom::Bits< 0, 1, u16> enable_texture_mapping;
    atom::Bits< 1, 1, u16> enable_highlight_shading;
    atom::Bits< 2, 1, u16> enable_alpha_test;
    atom::Bits< 3, 1, u16> enable_alpha_blend;
    atom::Bits< 4, 1, u16> enable_anti_aliasing;
    atom::Bits< 5, 1, u16> enable_edge_marking;
    atom::Bits< 6, 1, u16> ignore_fog_rgb_color;
    atom::Bits< 7, 1, u16> enable_fog;
    atom::Bits< 8, 4, u16> fog_depth_shift;
    atom::Bits<13, 1, u16> poly_or_vert_ram_overflow;
    atom::Bits<14, 1, u16> enable_rear_plane_bitmap;

    u16 half = 0u;
  };

  union GXSTAT {
    enum class IRQ {
      Never = 0,
      LessThanHalfFull = 1,
      Empty = 2,
      Reserved = 3
    };

    atom::Bits< 0, 1, u32> test_cmd_busy;
    atom::Bits< 1, 1, u32> test_cmd_result;
    atom::Bits< 8, 5, u32> coordinate_stack_level;
    atom::Bits<13, 1, u32> projection_stack_level;
    atom::Bits<14, 1, u32> matrix_stack_busy;
    atom::Bits<15, 1, u32> matrix_stack_error_flag;
    atom::Bits<16, 9, u32> cmd_fifo_size;
    atom::Bits<25, 1, u32> cmd_fifo_less_than_half_full;
    atom::Bits<26, 1, u32> cmd_fifo_empty;
    atom::Bits<27, 1, u32> busy;
    atom::Bits<30, 2, u32> cmd_fifo_irq;

    u32 word = 0u;
  };

  union CLEAR_COLOR {
    atom::Bits< 0, 5, u32> color_r;
    atom::Bits< 5, 5, u32> color_g;
    atom::Bits<10, 5, u32> color_b;
    atom::Bits<15, 1, u32> enable_rear_plane_fog;
    atom::Bits<16, 5, u32> color_a;
    atom::Bits<24, 6, u32> polygon_id;

    u32 word = 0u;
  };

  union FOG_COLOR {
    atom::Bits< 0, 5, u32> color_r;
    atom::Bits< 5, 5, u32> color_g;
    atom::Bits<10, 5, u32> color_b;
    atom::Bits<16, 5, u32> color_a;

    u32 word = 0u;
  };

  struct IO {
    DISP3DCNT disp3dcnt;
    GXSTAT gxstat;
    std::array<u32, 4> edge_color{};
    u32 alpha_test_ref{};
    CLEAR_COLOR clear_color{};
    u32 clear_depth{};
    FOG_COLOR fog_color{};
    u32 fog_offset{};
    std::array<u32,  8> fog_table{};
    std::array<u32, 16> toon_table{};
  };

} // namespace dual::nds::gpu
