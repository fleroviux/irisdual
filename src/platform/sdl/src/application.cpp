
#include <algorithm>
#include <atom/logger/logger.hpp>
#include <atom/arguments.hpp>
#include <dual/nds/backup/automatic.hpp>
#include <fstream>

#include "application.hpp"
#include "sdl2_audio_driver.hpp"

Application::Application() {
  SDL_Init(SDL_INIT_VIDEO);

  m_nds = std::make_unique<dual::nds::NDS>();
  m_nds->GetAPU().SetAudioDriver(std::make_shared<SDL2AudioDriver>());
}

Application::~Application() {
  for(auto& texture : m_textures) SDL_DestroyTexture(texture);

  SDL_DestroyRenderer(m_renderer);
  SDL_DestroyWindow(m_window);
  SDL_Quit();
}

int Application::Run(int argc, char** argv) {
  std::vector<const char*> files{};
  std::string boot7_path = "boot7.bin";
  std::string boot9_path = "boot9.bin";
  int scale = 0;
  bool fullscreen = false;
  bool enable_jit = false;

  atom::Arguments args{"irisdual", "A Nintendo DS emulator developed for fun, with performance and multicore CPUs in mind.", {0, 1, 0}};
  args.RegisterArgument(boot7_path, true, "boot7", "Path to the ARM7 Boot ROM", "path");
  args.RegisterArgument(boot9_path, true, "boot9", "Path to the ARM9 Boot ROM", "path");
  args.RegisterArgument(scale, true, "scale", "Screen scale factor");
  args.RegisterArgument(fullscreen, true, "fullscreen", "Whether to run in fullscreen or windowed mode");
#ifdef DUAL_ENABLE_JIT
  args.RegisterArgument(enable_jit, true, "jit", "Use dynamic recompilation");
#endif
  args.RegisterFile("nds_file", false);

  if(!args.Parse(argc, argv, &files)) {
    std::exit(-1);
  }

  CreateWindow(scale, fullscreen);
#ifdef DUAL_ENABLE_JIT
  // CPU engine must be configured before resetting the emulator
  if(enable_jit) {
    m_nds->SetCPUExecutionEngine(dual::nds::CPUExecutionEngine::JIT);
  }
#endif
  // ARM7 boot ROM must be loaded before the ROM when firmware booting.
  LoadBootROM(boot7_path.c_str(), false);
  LoadBootROM(boot9_path.c_str(), true);
  LoadROM(files[0]);
  MainLoop();
  return 0;
}

void Application::CreateWindow(int scale, bool fullscreen) {
  if(fullscreen) {
    m_window = SDL_CreateWindow(
      "irisdual",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      1,
      1,
      SDL_WINDOW_FULLSCREEN_DESKTOP //SDL_WINDOW_ALLOW_HIGHDPI
    );

    int win_w, win_h;
    SDL_GetWindowSize(m_window, &win_w, &win_h);

    if(scale == 0) {
      const int screen_w = win_h * 256 / 384;
      const int screen_h = win_h / 2;
      const int x_offset = (win_w - screen_w) / 2;

      m_screen_geometry[0] = {x_offset, 0, screen_w, screen_h};
      m_screen_geometry[1] = {x_offset, screen_h, screen_w, screen_h};
    } else {
      const int actual_scale = std::min(scale, std::min(win_w / 256, win_h / 384));
      const int screen_w = 256 * actual_scale;
      const int screen_h = 192 * actual_scale;
      const int x_offset = (win_w - screen_w) / 2;
      const int y_offset = (win_h - screen_h * 2) / 2;

      m_screen_geometry[0] = {x_offset, y_offset, screen_w, screen_h};
      m_screen_geometry[1] = {x_offset, y_offset + screen_h, screen_w, screen_h};
    }
  } else {
    const int actual_scale = std::max(scale, 1);
    const int screen_w = 256 * actual_scale;
    const int screen_h = 192 * actual_scale;

    m_screen_geometry[0] = {0, 0, screen_w, screen_h};
    m_screen_geometry[1] = {0, screen_h, screen_w, screen_h};

    m_window = SDL_CreateWindow(
      "irisdual",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      screen_w,
      screen_h * 2,
      0//SDL_WINDOW_ALLOW_HIGHDPI
    );
  }

  m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);

  for(auto& texture : m_textures) {
    texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 192);
  }
}

void Application::LoadROM(const char* path) {
  u8* data;
  size_t size;
  std::ifstream file{path, std::ios::binary};

  if(!file.good()) {
    ATOM_PANIC("Failed to open NDS file: '{}'", path);
  }

  file.seekg(0, std::ios::end);
  size = file.tellg();
  file.seekg(0);

  data = new u8[size];
  file.read((char*)data, static_cast<std::streamsize>(size));

  if(!file.good()) {
    ATOM_PANIC("Failed to read NDS file: '{}'", path);
  }

  const auto save_path = std::filesystem::path{path}.replace_extension("sav").string();

  const auto backup = std::make_shared<dual::nds::AutomaticBackupDevice>(save_path);
  m_nds->LoadROM(std::make_shared<dual::nds::MemoryROM>(data, size), backup);
  m_nds->DirectBoot();
}

void Application::LoadBootROM(const char* path, bool arm9) {
  const size_t maximum_size = arm9 ? 0x8000 : 0x4000;

  std::ifstream file{path, std::ios::binary};

  if(!file.good()) {
    ATOM_PANIC("Failed to open boot ROM: '{}'", path);
  }

  size_t size;

  file.seekg(0, std::ios::end);
  size = file.tellg();
  file.seekg(0);

  if(size > maximum_size) {
    ATOM_PANIC("Boot ROM is too big, expected {} bytes but got {} bytes", maximum_size, size);
  }

  std::array<u8, 0x8000> boot_rom{};

  file.read((char*)boot_rom.data(), static_cast<std::streamsize>(size));

  if(!file.good()) {
    ATOM_PANIC("Failed to read Boot ROM: '{}'", path);
  }

  if(arm9) {
    m_nds->LoadBootROM9(boot_rom);
  } else {
    m_nds->LoadBootROM7(std::span<u8, 0x4000>{boot_rom.data(), 0x4000});
  }
}

void Application::MainLoop() {
  SDL_Event event;

  m_emu_thread.Start(std::move(m_nds));

  while(true) {
    while(SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        return;
      }
      HandleEvent(event);
    }

    const auto frame = m_emu_thread.AcquireFrame();

    if(frame.has_value()) {
      SDL_UpdateTexture(m_textures[0], nullptr, frame.value().first, 256 * sizeof(u32));
      SDL_UpdateTexture(m_textures[1], nullptr, frame.value().second, 256 * sizeof(u32));

      SDL_RenderClear(m_renderer);
      SDL_RenderCopy(m_renderer, m_textures[0], nullptr, &m_screen_geometry[0]);
      SDL_RenderCopy(m_renderer, m_textures[1], nullptr, &m_screen_geometry[1]);
      SDL_RenderPresent(m_renderer);

      m_emu_thread.ReleaseFrame();

      UpdateFPS();
    }
  }
}

void Application::HandleEvent(const SDL_Event& event) {
  const u32 type = event.type;

  const SDL_Rect& bottom_screen = m_screen_geometry[1];
  const i32 window_x0 = bottom_screen.x;
  const i32 window_x1 = bottom_screen.x + bottom_screen.w;
  const i32 window_y0 = bottom_screen.y;
  const i32 window_y1 = bottom_screen.y + bottom_screen.h;

  const auto window_xy_in_range = [&](i32 x, i32 y) {
    return x >= window_x0 && x < window_x1 && y >= window_y0 && y < window_y1;
  };

  const auto window_x_to_screen_x = [&](i32 window_x) -> u8 {
    return std::clamp((window_x - window_x0) * 256 / (window_x1 - window_x0), 0, 255);
  };

  const auto window_y_to_screen_y = [&](i32 window_y) -> u8 {
    return std::clamp((window_y - window_y0) * 192 / (window_y1 - window_y0), 0, 191);
  };

  const auto set_touch_state = [&](i32 window_x, i32 window_y) {
    m_emu_thread.SetTouchState(m_touch_pen_down, window_x_to_screen_x(window_x), window_y_to_screen_y(window_y));
  };

  if(type == SDL_MOUSEBUTTONUP || type == SDL_MOUSEBUTTONDOWN) {
    const SDL_MouseButtonEvent& mouse_event = (const SDL_MouseButtonEvent&)event;

    if(mouse_event.button == SDL_BUTTON_LEFT) {
      const i32 window_x = mouse_event.x;
      const i32 window_y = mouse_event.y;

      m_touch_pen_down = window_xy_in_range(window_x, window_y) && mouse_event.state == SDL_PRESSED;
      set_touch_state(window_x, window_y);
    }
  }

  if(type == SDL_MOUSEMOTION) {
    const SDL_MouseMotionEvent& mouse_event = (const SDL_MouseMotionEvent&)event;

    const i32 window_x = mouse_event.x;
    const i32 window_y = mouse_event.y;

    if(window_xy_in_range(window_x, window_y)) {
      m_touch_pen_down = mouse_event.state & SDL_BUTTON_LMASK;
    }

    set_touch_state(window_x, window_y);
  }

  if(type == SDL_KEYUP || type == SDL_KEYDOWN) {
    const SDL_KeyboardEvent& keyboard_event = (const SDL_KeyboardEvent&)event;
    const bool pressed = type == SDL_KEYDOWN;

    const auto update_key = [&](dual::nds::Key key) {
      m_emu_thread.SetKeyState(key, pressed);
    };

    switch(keyboard_event.keysym.sym) {
      case SDLK_a: update_key(dual::nds::Key::A); break;
      case SDLK_s: update_key(dual::nds::Key::B); break;
      case SDLK_d: update_key(dual::nds::Key::L); break;
      case SDLK_f: update_key(dual::nds::Key::R); break;
      case SDLK_q: update_key(dual::nds::Key::X); break;
      case SDLK_w: update_key(dual::nds::Key::Y); break;
      case SDLK_BACKSPACE: update_key(dual::nds::Key::Select); break;
      case SDLK_RETURN:    update_key(dual::nds::Key::Start);  break;
      case SDLK_UP:    update_key(dual::nds::Key::Up);    break;
      case SDLK_DOWN:  update_key(dual::nds::Key::Down);  break;
      case SDLK_LEFT:  update_key(dual::nds::Key::Left);  break;
      case SDLK_RIGHT: update_key(dual::nds::Key::Right); break;
      case SDLK_F11: if(!pressed) m_emu_thread.Reset(); break;
      case SDLK_F12: if(!pressed) m_emu_thread.DirectBoot(); break;
      case SDLK_SPACE: m_emu_thread.SetFastForward(pressed); break;
    }
  }
}

void Application::UpdateFPS() {
  const auto now = std::chrono::system_clock::now();
  const auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
    now - m_last_fps_update).count();

  m_fps_counter++;

  if(elapsed_time >= 1000) {
    const f32 fps = (f32)m_fps_counter / (f32)elapsed_time * 1000.0f;
    SDL_SetWindowTitle(m_window, fmt::format("irisdual [{:.2f} fps]", fps).c_str());
    m_fps_counter = 0;
    m_last_fps_update = std::chrono::system_clock::now();
  }
}
