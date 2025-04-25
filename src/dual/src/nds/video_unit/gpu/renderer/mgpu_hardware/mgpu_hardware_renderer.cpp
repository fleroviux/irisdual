
#include <dual/nds/video_unit/gpu/renderer/mgpu_hardware_renderer.hpp>
#include <SDL_syswm.h>
#include <optional>

#include "shader/triangle.frag.h"
#include "shader/triangle.vert.h"

#define MGPU_CHECK(result_expression) \
  do { \
    MGPUResult result = result_expression; \
    if(result != MGPU_SUCCESS) \
      ATOM_PANIC("MGPU error: {} ({})", "" # result_expression, mgpuResultCodeToString(result)); \
  } while(0)

#ifdef SDL_VIDEO_DRIVER_COCOA
  extern "C" CAMetalLayer* TMP_Cocoa_CreateMetalLayer(NSWindow* ns_window);
#endif

namespace dual::nds::gpu {

MGPUHardwareRenderer::MGPUHardwareRenderer(
  IO& io,
  const Region<4, 131072>& vram_texture,
  const Region<8>& vram_palette
)   : m_io{io} {

  m_sdl_window = SDL_CreateWindow(
    "MGPU hardware renderer",
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    1024,
    768,
    SDL_WINDOW_VULKAN
  );

  // Create MGPU instance
  MGPU_CHECK(mgpuCreateInstance(MGPU_BACKEND_TYPE_VULKAN, &m_mgpu_instance));

  // Create surface
  {
    SDL_SysWMinfo wm_info{};
    SDL_GetWindowWMInfo(m_sdl_window, &wm_info);

    MGPUSurfaceCreateInfo surface_create_info{};

#if defined(SDL_VIDEO_DRIVER_WINDOWS)
    surface_create_info.win32 = {
      .hinstance = wm_info.info.win.hinstance,
      .hwnd = wm_info.info.win.window
    };
#elif defined(SDL_VIDEO_DRIVER_COCOA)
    surface_create_info.metal = {
      .metal_layer = TMP_Cocoa_CreateMetalLayer(wm_info.info.cocoa.window)
    };
#else
  #error "Unsupported SDL video driver"
#endif

    MGPU_CHECK(mgpuInstanceCreateSurface(m_mgpu_instance, &surface_create_info, &m_mgpu_surface));
  }

  // Pick physical device

  uint32_t mgpu_physical_device_count{};
  std::vector<MGPUPhysicalDevice> mgpu_physical_devices{};
  MGPU_CHECK(mgpuInstanceEnumeratePhysicalDevices(m_mgpu_instance, &mgpu_physical_device_count, nullptr));
  mgpu_physical_devices.resize(mgpu_physical_device_count);
  MGPU_CHECK(mgpuInstanceEnumeratePhysicalDevices(m_mgpu_instance, &mgpu_physical_device_count, mgpu_physical_devices.data()));

  std::optional<MGPUPhysicalDevice> discrete_gpu{};
  std::optional<MGPUPhysicalDevice> integrated_gpu{};
  std::optional<MGPUPhysicalDevice> virtual_gpu{};

  for(MGPUPhysicalDevice mgpu_physical_device : mgpu_physical_devices) {
    MGPUPhysicalDeviceInfo info{};
    MGPU_CHECK(mgpuPhysicalDeviceGetInfo(mgpu_physical_device, &info));
    switch(info.device_type) {
      case MGPU_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:     discrete_gpu = mgpu_physical_device; break;
      case MGPU_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: integrated_gpu = mgpu_physical_device; break;
      case MGPU_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:       virtual_gpu = mgpu_physical_device; break;
    }
  }

  MGPUPhysicalDevice mgpu_physical_device = discrete_gpu.value_or(
    integrated_gpu.value_or(
      virtual_gpu.value_or((MGPUPhysicalDevice)MGPU_NULL_HANDLE)));

  if(mgpu_physical_device == MGPU_NULL_HANDLE) {
    ATOM_PANIC("failed to find a suitable physical device");
  }

  MGPU_CHECK(mgpuPhysicalDeviceCreateDevice(mgpu_physical_device, &m_mgpu_device));

  // Create swap chain and the associated depth buffers
  {
    uint32_t surface_format_count{};
    std::vector<MGPUSurfaceFormat> surface_formats{};

    MGPU_CHECK(mgpuPhysicalDeviceEnumerateSurfaceFormats(mgpu_physical_device, m_mgpu_surface, &surface_format_count, nullptr));
    surface_formats.resize(surface_format_count);
    MGPU_CHECK(mgpuPhysicalDeviceEnumerateSurfaceFormats(mgpu_physical_device, m_mgpu_surface, &surface_format_count, surface_formats.data()));

    bool got_required_surface_format = false;

    for(const MGPUSurfaceFormat& surface_format : surface_formats) {
      if(surface_format.format == MGPU_TEXTURE_FORMAT_B8G8R8A8_SRGB && surface_format.color_space == MGPU_COLOR_SPACE_SRGB_NONLINEAR) {
        got_required_surface_format = true;
      }
    }

    if(!got_required_surface_format) {
      ATOM_PANIC("Failed to find a suitable surface format");
    }

    uint32_t present_modes_count{};
    std::vector<MGPUPresentMode> present_modes{};

    MGPU_CHECK(mgpuPhysicalDeviceEnumerateSurfacePresentModes(mgpu_physical_device, m_mgpu_surface, &present_modes_count, nullptr));
    present_modes.resize(present_modes_count);
    MGPU_CHECK(mgpuPhysicalDeviceEnumerateSurfacePresentModes(mgpu_physical_device, m_mgpu_surface, &present_modes_count, present_modes.data()));

    MGPUSurfaceCapabilities surface_capabilities{};
    MGPU_CHECK(mgpuPhysicalDeviceGetSurfaceCapabilities(mgpu_physical_device, m_mgpu_surface, &surface_capabilities));

    const MGPUSwapChainCreateInfo swap_chain_create_info{
      .surface = m_mgpu_surface,
      .format = MGPU_TEXTURE_FORMAT_B8G8R8A8_SRGB,
      .color_space = MGPU_COLOR_SPACE_SRGB_NONLINEAR,
      .present_mode = MGPU_PRESENT_MODE_FIFO,
      .usage = MGPU_TEXTURE_USAGE_RENDER_ATTACHMENT,
      .extent = surface_capabilities.current_extent,
      .min_texture_count = 2u,
      .old_swap_chain = nullptr
    };
    MGPU_CHECK(mgpuDeviceCreateSwapChain(m_mgpu_device, &swap_chain_create_info, &m_mgpu_swap_chain));

    u32 texture_count{};
    MGPU_CHECK(mgpuSwapChainEnumerateTextures(m_mgpu_swap_chain, &texture_count, nullptr));
    m_mgpu_swap_chain_color_textures.resize(texture_count);
    MGPU_CHECK(mgpuSwapChainEnumerateTextures(m_mgpu_swap_chain, &texture_count, m_mgpu_swap_chain_color_textures.data()));

    // TODO: do not hardcode the texture extents
    const MGPUTextureCreateInfo depth_texture_create_info{
      .format = MGPU_TEXTURE_FORMAT_DEPTH_F32,
      .type = MGPU_TEXTURE_TYPE_2D,
      .extent = {
        .width = 1024u,
        .height = 768u,
        .depth = 1u
      },
      .mip_count = 1u,
      .array_layer_count = 1u,
      .usage = MGPU_TEXTURE_USAGE_RENDER_ATTACHMENT
    };

    const MGPUTextureViewCreateInfo color_texture_view_create_info{
      .type = MGPU_TEXTURE_VIEW_TYPE_2D,
      .format = MGPU_TEXTURE_FORMAT_B8G8R8A8_SRGB,
      .aspect = MGPU_TEXTURE_ASPECT_COLOR,
      .base_mip = 0u,
      .mip_count = 1u,
      .base_array_layer = 0u,
      .array_layer_count = 1u
    };

    const MGPUTextureViewCreateInfo depth_texture_view_create_info{
      .type = MGPU_TEXTURE_VIEW_TYPE_2D,
      .format = MGPU_TEXTURE_FORMAT_DEPTH_F32,
      .aspect = MGPU_TEXTURE_ASPECT_DEPTH,
      .base_mip = 0u,
      .mip_count = 1u,
      .base_array_layer = 0u,
      .array_layer_count = 1u
    };

    for(MGPUTexture color_texture : m_mgpu_swap_chain_color_textures) {
      MGPUTexture depth_texture{};
      MGPU_CHECK(mgpuDeviceCreateTexture(m_mgpu_device, &depth_texture_create_info, &depth_texture));
      m_mgpu_swap_chain_depth_textures.push_back(depth_texture);

      MGPUTextureView depth_texture_view{};
      MGPU_CHECK(mgpuTextureCreateView(depth_texture, &depth_texture_view_create_info, &depth_texture_view));
      m_mgpu_swap_chain_depth_texture_views.push_back(depth_texture_view);

      MGPUTextureView color_texture_view{};
      MGPU_CHECK(mgpuTextureCreateView(color_texture, &color_texture_view_create_info, &color_texture_view));
      m_mgpu_swap_chain_color_texture_views.push_back(color_texture_view);
    }
  }

  // Create our vertex buffer
  const MGPUBufferCreateInfo vbo_create_info{
    .size = k_total_vertices * sizeof(BufferVertex),
    .usage = MGPU_BUFFER_USAGE_VERTEX_BUFFER | MGPU_BUFFER_USAGE_COPY_DST,
    .flags = 0//MGPU_BUFFER_FLAGS_HOST_VISIBLE
  };
  MGPU_CHECK(mgpuDeviceCreateBuffer(m_mgpu_device, &vbo_create_info, &m_mgpu_vbo));

  // Create our uniform buffer
  const MGPUBufferCreateInfo ubo_create_info{
    .size = sizeof(f32) * 4u,
    .usage = MGPU_BUFFER_USAGE_UNIFORM_BUFFER | MGPU_BUFFER_USAGE_COPY_DST,
    .flags = 0
  };
  MGPU_CHECK(mgpuDeviceCreateBuffer(m_mgpu_device, &ubo_create_info, &m_mgpu_test_ubo));

  // Create our test sampler
  const MGPUSamplerCreateInfo sampler_create_info{
    .mag_filter = MGPU_TEXTURE_FILTER_NEAREST,
    .min_filter = MGPU_TEXTURE_FILTER_NEAREST,
    .mip_filter = MGPU_TEXTURE_FILTER_NEAREST,
    .address_mode_u = MGPU_SAMPLER_ADDRESS_MODE_REPEAT,
    .address_mode_v = MGPU_SAMPLER_ADDRESS_MODE_REPEAT,
    .address_mode_w = MGPU_SAMPLER_ADDRESS_MODE_REPEAT,
    .mip_lod_bias = 0.0f,
    .anisotropy_enable = false,
    .max_anisotropy = 0.0f,
    .compare_enable = false,
    .compare_op = MGPU_COMPARE_OP_NEVER,
    .min_lod = 0.0f,
    .max_lod = 0.0f
  };
  MGPU_CHECK(mgpuDeviceCreateSampler(m_mgpu_device, &sampler_create_info, &m_mgpu_nearest_sampler));

  // Create the resource set layout
  const MGPUResourceSetLayoutBinding rset_layout_bindings[] {
    {
      .binding = 0u,
      .type = MGPU_RESOURCE_BINDING_TYPE_UNIFORM_BUFFER,
      .visibility = MGPU_SHADER_STAGE_ALL
    },
    {
      .binding = 1u,
      .type = MGPU_RESOURCE_BINDING_TYPE_TEXTURE_AND_SAMPLER,
      .visibility = MGPU_SHADER_STAGE_FRAGMENT
    }
  };
  const MGPUResourceSetLayoutCreateInfo rset_layout_create_info{
    .binding_count = sizeof(rset_layout_bindings) / sizeof(MGPUResourceSetLayoutBinding),
    .bindings = rset_layout_bindings
  };
  MGPU_CHECK(mgpuDeviceCreateResourceSetLayout(m_mgpu_device, &rset_layout_create_info, &m_mgpu_resource_set_layout));

  // Create pipeline state objects

  MGPU_CHECK(mgpuDeviceCreateShaderModule(m_mgpu_device, triangle_vert, sizeof(triangle_vert), &m_mgpu_vert_shader));
  MGPU_CHECK(mgpuDeviceCreateShaderModule(m_mgpu_device, triangle_frag, sizeof(triangle_frag), &m_mgpu_frag_shader));

  const MGPUShaderStageCreateInfo shader_stages[2] {
    {
      .stage = MGPU_SHADER_STAGE_VERTEX,
      .module = m_mgpu_vert_shader,
      .entrypoint = "main"
    },
    {
      .stage = MGPU_SHADER_STAGE_FRAGMENT,
      .module = m_mgpu_frag_shader,
      .entrypoint = "main"
    }
  };
  const MGPUShaderProgramCreateInfo shader_program_create_info{
    .shader_stage_count = 2u,
    .shader_stages = shader_stages,
    .resource_set_count = 1u,
    .resource_set_layouts = &m_mgpu_resource_set_layout
  };
  MGPU_CHECK(mgpuDeviceCreateShaderProgram(m_mgpu_device, &shader_program_create_info, &m_mgpu_shader_program));

  const MGPURasterizerStateCreateInfo rasterizer_state_create_info{
    .depth_clamp_enable = false,
    .rasterizer_discard_enable = false,
    .polygon_mode = MGPU_POLYGON_MODE_FILL,
    .cull_mode = 0,
    .front_face = MGPU_FRONT_FACE_COUNTER_CLOCKWISE,
    .depth_bias_enable = false,
    .depth_bias_constant_factor = 0.f,
    .depth_bias_clamp = 0.f,
    .depth_bias_slope_factor = 0.f,
    .line_width = 1.f
  };
  MGPU_CHECK(mgpuDeviceCreateRasterizerState(m_mgpu_device, &rasterizer_state_create_info, &m_mgpu_rasterizer_state));

  const MGPUVertexBinding vertex_input_binding{
    .binding = 0u,
    .stride = sizeof(BufferVertex),
    .input_rate = MGPU_VERTEX_INPUT_RATE_VERTEX
  };
  const MGPUVertexAttribute vertex_input_attributes[]{
    {
      .location = 0u,
      .binding = 0u,
      .format = MGPU_VERTEX_FORMAT_STUB_XYZW32323232,
      .offset = 0u
    },
    {
      .location = 1u,
      .binding = 0u,
      .format = MGPU_VERTEX_FORMAT_STUB_XYZW32323232,
      .offset = sizeof(f32) * 4
    },
    {
      .location = 2u,
      .binding = 0u,
      .format = MGPU_VERTEX_FORMAT_STUB_XY3232,
      .offset = sizeof(f32) * 8
    }
  };
  const MGPUVertexInputStateCreateInfo vertex_input_state_create_info{
    .binding_count = 1u,
    .bindings = &vertex_input_binding,
    .attribute_count = sizeof(vertex_input_attributes) / sizeof(MGPUVertexAttribute),
    .attributes = vertex_input_attributes
  };
  MGPU_CHECK(mgpuDeviceCreateVertexInputState(m_mgpu_device, &vertex_input_state_create_info, &m_mgpu_vertex_input_state));

  const MGPUDepthStencilStateCreateInfo depth_stencil_state_create_info{
    .depth_test_enable = true,
    .depth_write_enable = true,
    .depth_compare_op = MGPU_COMPARE_OP_LESS,
    .stencil_test_enable = false
  };
  MGPU_CHECK(mgpuDeviceCreateDepthStencilState(m_mgpu_device, &depth_stencil_state_create_info, &m_mgpu_depth_stencil_state));

  // Create a command list
  MGPU_CHECK(mgpuDeviceCreateCommandList(m_mgpu_device, &m_mgpu_cmd_list));

  // Finally get our graphics queue
  m_mgpu_queue = mgpuDeviceGetQueue(m_mgpu_device, MGPU_QUEUE_TYPE_GRAPHICS_COMPUTE);

  m_texture_cache = std::make_unique<MGPUTextureCache>(m_mgpu_device, vram_texture, vram_palette);
}

MGPUHardwareRenderer::~MGPUHardwareRenderer() {
  m_texture_cache.reset();

  mgpuCommandListDestroy(m_mgpu_cmd_list);
  mgpuDepthStencilStateDestroy(m_mgpu_depth_stencil_state);
  mgpuVertexInputStateDestroy(m_mgpu_vertex_input_state);
  mgpuRasterizerStateDestroy(m_mgpu_rasterizer_state);
  mgpuShaderProgramDestroy(m_mgpu_shader_program);
  mgpuShaderModuleDestroy(m_mgpu_frag_shader);
  mgpuShaderModuleDestroy(m_mgpu_vert_shader);
  mgpuResourceSetLayoutDestroy(m_mgpu_resource_set_layout);
  mgpuSamplerDestroy(m_mgpu_nearest_sampler);
  mgpuBufferDestroy(m_mgpu_test_ubo);
  mgpuBufferDestroy(m_mgpu_vbo);
  for(MGPUTexture texture : m_mgpu_swap_chain_depth_textures) {
    mgpuTextureDestroy(texture);
  }
  for(MGPUTextureView texture_view : m_mgpu_swap_chain_color_texture_views) {
    mgpuTextureViewDestroy(texture_view);
  }
  for(MGPUTextureView texture_view : m_mgpu_swap_chain_depth_texture_views) {
    mgpuTextureViewDestroy(texture_view);
  }
  mgpuSwapChainDestroy(m_mgpu_swap_chain);
  mgpuDeviceDestroy(m_mgpu_device);
  mgpuSurfaceDestroy(m_mgpu_surface);
  mgpuInstanceDestroy(m_mgpu_instance);
  SDL_DestroyWindow(m_sdl_window);
}

void MGPUHardwareRenderer::Render(const Viewport& viewport, std::span<const Polygon* const> polygons) {
  u32 texture_index;
  MGPU_CHECK(mgpuSwapChainAcquireNextTexture(m_mgpu_swap_chain, &texture_index));

  MGPU_CHECK(mgpuCommandListClear(m_mgpu_cmd_list));

  std::vector<MGPUResourceSet> temp_resource_sets{};

  // Generate and upload vertex data
  {
    m_vbo_data.Clear();

    for(const auto& polygon : polygons) {
      for(size_t i = 1; i < polygon->vertices.Size() - 1u; i++) {
        for(const size_t j : {(size_t)0, i, i + 1}) {
          auto vert = polygon->vertices[j];

          const f32 position_x =  (f32)vert->position.X().Raw() / (f32)(1 << 12);
          const f32 position_y = -(f32)vert->position.Y().Raw() / (f32)(1 << 12);
          const f32 position_z =  (f32)vert->position.Z().Raw() / (f32)(1 << 12);
          const f32 position_w =  (f32)vert->position.W().Raw() / (f32)(1 << 12);

          const f32 color_a = (f32)vert->color.A().Raw() / (f32)(1 << 6);
          const f32 color_r = (f32)vert->color.R().Raw() / (f32)(1 << 6);
          const f32 color_g = (f32)vert->color.G().Raw() / (f32)(1 << 6);
          const f32 color_b = (f32)vert->color.B().Raw() / (f32)(1 << 6);

          const f32 texcoord_s = (f32)vert->uv.X().Raw() / (f32)(1 << 4);
          const f32 texcoord_t = (f32)vert->uv.Y().Raw() / (f32)(1 << 4);

          m_vbo_data.PushBack({position_x, position_y, position_z, position_w, color_r, color_g, color_b, color_a, texcoord_s, texcoord_t});
        }
      }
    }

    MGPU_CHECK(mgpuQueueBufferUpload(m_mgpu_queue, m_mgpu_vbo, 0u, m_vbo_data.Size() * sizeof(BufferVertex), m_vbo_data.Data()));
  }

  static bool first_frame = true;
  if(first_frame) {
    const f32 uniform_color[4]{1.0, 1.0, 0.0, 1.0};
    MGPU_CHECK(mgpuQueueBufferUpload(m_mgpu_queue, m_mgpu_test_ubo, 0u, sizeof(uniform_color), uniform_color));
    first_frame = false;
  }

  const MGPURenderPassColorAttachment render_pass_color_attachments[1] {
    {
      .texture_view = m_mgpu_swap_chain_color_texture_views[texture_index],
      .load_op = MGPU_LOAD_OP_CLEAR,
      .store_op = MGPU_STORE_OP_STORE,
      .clear_color = {.r = 0.3f, .g = 0.f, .b = 0.9f, .a = 1.f}
    }
  };
  const MGPURenderPassDepthStencilAttachment render_pass_depth_attachment{
    .texture_view = m_mgpu_swap_chain_depth_texture_views[texture_index],
    .depth_load_op = MGPU_LOAD_OP_CLEAR,
    .depth_store_op = MGPU_STORE_OP_STORE,
    .stencil_load_op = MGPU_LOAD_OP_DONT_CARE,
    .stencil_store_op = MGPU_STORE_OP_DONT_CARE,
    .clear_depth = 1.0f,
    .clear_stencil = 0u
  };
  const MGPURenderPassBeginInfo render_pass_info{
    .color_attachment_count = 1u,
    .color_attachments = render_pass_color_attachments,
    .depth_stencil_attachment = &render_pass_depth_attachment
  };
  const auto mgpu_render_cmd_encoder = mgpuCommandListCmdBeginRenderPass(m_mgpu_cmd_list, &render_pass_info);
  mgpuRenderCommandEncoderCmdUseShaderProgram(mgpu_render_cmd_encoder, m_mgpu_shader_program);
  mgpuRenderCommandEncoderCmdUseRasterizerState(mgpu_render_cmd_encoder, m_mgpu_rasterizer_state);
  mgpuRenderCommandEncoderCmdUseVertexInputState(mgpu_render_cmd_encoder, m_mgpu_vertex_input_state);
  mgpuRenderCommandEncoderCmdUseDepthStencilState(mgpu_render_cmd_encoder, m_mgpu_depth_stencil_state);
  mgpuRenderCommandEncoderCmdBindVertexBuffer(mgpu_render_cmd_encoder, 0u, m_mgpu_vbo, 0u);
  {
    // TODO(fleroviux): reimplement proper batching code
    int next_batch_start = 0;

    TextureParams current_batch_params;
    u32 current_batch_palette_base;
    int current_batch_vertex_start = 0;

    const auto RenderBatch = [&]() {
      const int vertex_count = next_batch_start - current_batch_vertex_start;
      if(vertex_count != 0) {
        MGPUTextureView texture_view = m_texture_cache->GetOrCreate(current_batch_params, current_batch_palette_base);

        // Absolute cinema :^)
        // TODO(fleroviux): remove dummy UBO
        const MGPUResourceSetBinding rset_bindings[] {
          {
            .binding = 0u,
            .type = MGPU_RESOURCE_BINDING_TYPE_UNIFORM_BUFFER,
            .buffer = {
              .buffer = m_mgpu_test_ubo,
              .offset = 0u,
              .size = MGPU_WHOLE_SIZE
            }
          },
          {
            .binding = 1u,
            .type = MGPU_RESOURCE_BINDING_TYPE_TEXTURE_AND_SAMPLER,
            .texture = {
              .texture_view = texture_view,
              .sampler = m_mgpu_nearest_sampler
            }
          }
        };
        const MGPUResourceSetCreateInfo rset_create_info{
          .layout = m_mgpu_resource_set_layout,
          .binding_count = sizeof(rset_bindings) / sizeof(MGPUResourceSetBinding),
          .bindings = rset_bindings
        };
        MGPUResourceSet mgpu_resource_set{};
        MGPU_CHECK(mgpuDeviceCreateResourceSet(m_mgpu_device, &rset_create_info, &mgpu_resource_set));

        mgpuRenderCommandEncoderCmdBindResourceSet(mgpu_render_cmd_encoder, 0u, mgpu_resource_set);
        mgpuRenderCommandEncoderCmdDraw(mgpu_render_cmd_encoder, vertex_count, 1u, current_batch_vertex_start, 0u);

        temp_resource_sets.push_back(mgpu_resource_set);
      }
    };

    for(size_t i = 0; i < polygons.size(); i++) {
      const Polygon* const polygon = polygons[i];
      const int vertices = ((int)polygon->vertices.Size() - 2) * 3;

      // make sure current batch state is initialized
      if (i == 0) {
        current_batch_params = polygon->texture_params;
        current_batch_palette_base = polygon->palette_base;
      }

      if(current_batch_params.word != polygon->texture_params.word || current_batch_palette_base != polygon->palette_base) {
        RenderBatch();

        current_batch_params = polygon->texture_params;
        current_batch_palette_base = polygon->palette_base;
        current_batch_vertex_start = next_batch_start;
      }

      next_batch_start += vertices;
    }

    RenderBatch();
  }
  //mgpuCommandListCmdEndRenderPass(m_mgpu_cmd_list);
  mgpuRenderCommandEncoderClose(mgpu_render_cmd_encoder);

  MGPU_CHECK(mgpuQueueSubmitCommandList(m_mgpu_queue, m_mgpu_cmd_list));
  MGPU_CHECK(mgpuSwapChainPresent(m_mgpu_swap_chain));

  // We can only delete the resource sets once the command list is no longer in use
  for(auto mgpu_resource_set : temp_resource_sets) {
    mgpuResourceSetDestroy(mgpu_resource_set);
  }
}

} // namespace dual::nds::gpu
