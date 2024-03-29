
#pragma once

#include <atom/bit.hpp>
#include <atom/vector_n.hpp>
#include <dual/nds/video_unit/gpu/math.hpp>
#include <dual/nds/video_unit/gpu/registers.hpp>
#include <span>

namespace dual::nds::gpu {

  union TextureParams {
    enum class Format {
      Disabled = 0,
      A3I5 = 1,
      Palette2BPP = 2,
      Palette4BPP = 3,
      Palette8BPP = 4,
      Compressed4x4 = 5,
      A5I3 = 6,
      Raw16BPP = 7
    };

    enum class Transform {
      None = 0,
      TexCoord = 1,
      Normal = 2,
      Position = 3
    };

    atom::Bits< 0, 16, u32> vram_offset_div_8;
    atom::Bits<16,  2, u32> repeat;
    atom::Bits<18,  2, u32> flip;
    atom::Bits<20,  3, u32> log2_s_size;
    atom::Bits<23,  3, u32> log2_t_size;
    atom::Bits<26,  3, u32> format;
    atom::Bits<29,  1, u32> color0_transparent;
    atom::Bits<30,  2, u32> st_transform;

    u32 word = 0u;
  };

  struct Vertex {
    Vector4<Fixed20x12> position;
    Vector2<Fixed12x4> uv;
    Color4 color;
  };

  struct Polygon {
    enum class Mode {
      Modulation = 0,
      Decal = 1,
      Shaded = 2,
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

    TextureParams texture_params;
    u32 palette_base = 0u;

    int windedness;
    bool translucent;
    u32 sorting_key;

    atom::Vector_N<Vertex*, 10> vertices;
    atom::Vector_N<u16, 10> w_16;
    int w_l_shift;
    int w_r_shift;
  };

  class GeometryEngine {
    public:
      explicit GeometryEngine(gpu::IO& io);

      void Reset();
      void SwapBuffers();

      void Begin(u32 parameter);
      void End();

      [[nodiscard]] u32 Read_RAM_COUNT() const {
        return m_polygon_ram[m_current_buffer].Size() | m_vertex_ram[m_current_buffer].Size() << 16;
      }

      void SetPolygonAttributes(u32 attributes) {
        m_pending_polygon_attributes = attributes;
      }

      void SetTextureParameters(u32 parameters) {
        m_texture_parameters.word = parameters;
      }

      void SetPaletteBase(u32 palette_base) {
        m_texture_palette_base = palette_base;
      }

      void SetVertexColor(const Color4& color) {
        m_vertex_color = color;
      }

      void SetVertexUV(Vector2<Fixed12x4> uv, const Matrix4<Fixed20x12>& texture_matrix);
      void SetNormal(Vector3<Fixed20x12> normal, const Matrix4<Fixed20x12>& texture_matrix);

      void SetMaterialDiffuseColor(const Color4& color) {
        m_material.diffuse_color = color;
      }

      void SetMaterialAmbientColor(const Color4& color) {
        m_material.ambient_color = color;
      }

      void SetMaterialSpecularColor(const Color4& color) {
        m_material.specular_color = color;
      }

      void SetMaterialEmissiveColor(const Color4& color) {
        m_material.emissive_color = color;
      }

      void SetLightDirection(int index, Vector3<Fixed20x12> direction) {
        Light& light = m_lights[index];

        light.direction = direction;
        light.halfway = Vector3<Fixed20x12>{
          direction.X().Raw() >> 1,
          direction.Y().Raw() >> 1,
         (direction.Z().Raw() - (1 << 12)) >> 1
        };
      }

      void SetLightColor(int index, const Color4& color) {
        m_lights[index].color = color;
      }

      void SetShininessTableEnable(bool enabled) {
        m_material.enable_shininess_table = enabled;
      }

      void SetManualTranslucentYSorting(bool manual_translucent_y_sorting) {
        m_manual_translucent_y_sorting = manual_translucent_y_sorting;
      }

      std::array<u8, 128>& GetShininessTable() {
        return m_material.shininess_table;
      }

      [[nodiscard]] std::span<const Polygon* const> GetPolygonsToRender() const {
        return m_polygons_sorted;
      }

      void SubmitVertex(Vector3<Fixed20x12> position, const Matrix4<Fixed20x12>& clip_matrix, const Matrix4<Fixed20x12>& texture_matrix);

      [[nodiscard]] atom::Vector_N<Vertex, 10> ClipPolygon(
        const atom::Vector_N<Vertex, 10>& vertex_list, bool quad_strip) const;

    private:
      void CalculateSortingKey(Polygon& poly) const;

      static void NormalizeW(Polygon& poly);

      template<int axis, typename Comparator>
      static bool ClipPolygonAgainstPlane(
        const atom::Vector_N<Vertex, 10>& vertex_list_in,
        atom::Vector_N<Vertex, 10>& vertex_list_out
      );

      static int CalculateWindedness(
        const Vector4<Fixed20x12>& v0,
        const Vector4<Fixed20x12>& v1,
        const Vector4<Fixed20x12>& v2
      );

      gpu::IO& m_io;

      atom::Vector_N<Vertex, 6144> m_vertex_ram[2];
      atom::Vector_N<Polygon, 2048> m_polygon_ram[2];
      atom::Vector_N<const Polygon*, 2048> m_polygons_sorted{};
      atom::Vector_N<Vertex, 10> m_current_vertex_list;
      bool m_inside_vertex_list{};
      bool m_primitive_is_quad{};
      bool m_primitive_is_strip{};
      bool m_first_vertex{};
      int m_polygon_strip_length{};
      int m_current_buffer{};
      u32 m_pending_polygon_attributes{};
      Polygon::Attributes m_polygon_attributes{};
      TextureParams m_texture_parameters{};
      u32 m_texture_palette_base{};
      Color4 m_vertex_color{};
      Vector2<Fixed12x4> m_vertex_uv{};
      Vector2<Fixed12x4> m_vertex_uv_src{};

      struct Light {
        Vector3<Fixed20x12> direction{};
        Vector3<Fixed20x12> halfway{};
        Color4 color{};
      };

      std::array<Light, 4> m_lights{};

      struct Material {
        Color4 diffuse_color{};
        Color4 ambient_color{};
        Color4 specular_color{};
        Color4 emissive_color{};
        std::array<u8, 128> shininess_table{};
        bool enable_shininess_table{false};
      } m_material{};

      bool m_manual_translucent_y_sorting{};
  };

} // namespace dual::nds::gpu
