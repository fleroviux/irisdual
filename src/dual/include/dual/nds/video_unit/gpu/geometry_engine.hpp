
#pragma once

#include <atom/bit.hpp>
#include <atom/vector_n.hpp>
#include <dual/nds/video_unit/gpu/math.hpp>
#include <dual/nds/video_unit/gpu/registers.hpp>
#include <span>

namespace dual::nds::gpu {

  struct Vertex {
    Vector4<Fixed20x12> position;
    Vector2<Fixed12x4> uv;
    Color4 color;
  };

  struct Polygon {
    enum class Mode {
      Modulation = 0,
      Decal = 1,
      Toon = 2,
      Shadow = 3
    };

    union Attributes {
      atom::Bits< 0, 4, u32> light_enable;
      atom::Bits< 4, 2, u32> polygon_mode;
      atom::Bits< 6, 1, u32> render_back_face;
      atom::Bits< 7, 1, u32> render_front_face;
      atom::Bits<11, 1, u32> enable_translucent_depth_write;
      atom::Bits<12, 1, u32> render_far_plane_intersecting;
      atom::Bits<14, 1, u32> use_equal_depth_test;
      atom::Bits<15, 1, u32> enable_fog;
      atom::Bits<16, 5, u32> alpha;
      atom::Bits<24, 6, u32> polygon_id;

      u32 word = 0u;
    } attributes;

    atom::Vector_N<Vertex*, 10> vertices;
  };

  class GeometryEngine {
    public:
      explicit GeometryEngine(gpu::IO& io);

      void Reset();
      void SwapBuffers();

      void Begin(u32 parameter);
      void End();

      void SetPolygonAttributes(u32 attributes) {
        m_pending_polygon_attributes = attributes;
      }

      void SetVertexColor(const Color4& color) {
        m_vertex_color = color;
      }

      void SubmitVertex(Vector3<Fixed20x12> position, const Matrix4<Fixed20x12>& clip_matrix);

      [[nodiscard]] std::span<const Polygon> GetPolygonsToRender() const {
        return m_polygon_ram[m_current_buffer ^ 1];
      }

    private:
      atom::Vector_N<Vertex, 10> ClipPolygon(const atom::Vector_N<Vertex, 10>& vertex_list, bool quad_strip);

      template<int axis, typename Comparator>
      bool ClipPolygonAgainstPlane(
        const atom::Vector_N<Vertex, 10>& vertex_list_in,
        atom::Vector_N<Vertex, 10>& vertex_list_out
      );

      gpu::IO& m_io;

      atom::Vector_N<Vertex, 6144> m_vertex_ram[2];
      atom::Vector_N<Polygon, 2048> m_polygon_ram[2];
      atom::Vector_N<Vertex, 10> m_current_vertex_list;
      bool m_inside_vertex_list{};
      bool m_primitive_is_quad{};
      bool m_primitive_is_strip{};
      bool m_first_vertex{};
      int m_polygon_strip_length{};
      int m_current_buffer{};
      u32 m_pending_polygon_attributes{};
      Polygon::Attributes m_polygon_attributes{};
      Color4 m_vertex_color{};
  };

} // namespace dual::nds::gpu
