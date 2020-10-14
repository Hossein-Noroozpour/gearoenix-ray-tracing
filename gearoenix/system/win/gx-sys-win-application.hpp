#ifndef GEAROENIX_SYSTEM_WINDOWS_APPLICATION_HPP
#define GEAROENIX_SYSTEM_WINDOWS_APPLICATION_HPP
#include "../../core/gx-cr-build-configuration.hpp"
#ifdef GX_USE_WINAPI
#include "../../core/gx-cr-types.hpp"
#include <Windows.h>
namespace gearoenix {
namespace core {
    namespace asset {
        class Manager;
    }
    class Application;
}
namespace render {
    class Engine;
}
namespace system {
    class Application {
    private:
        int screen_width, screen_height, border_width, title_bar_height;
        core::Real screen_ratio, half_height_inversed;
        render::Engine* render_engine = nullptr;
        core::asset::Manager* astmgr = nullptr;
        core::Application* core_app = nullptr;
        HINSTANCE instance;
        HWND window;
        bool running = true;
        bool window_is_up = false, resizing = true;
        UINT mouse_x_pixel = 0, mouse_y_pixel = 0;
        core::Real mouse_x = 0.0f, mouse_y = 0.0f;
        core::Real supported_engine;
        static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam);
        LRESULT CALLBACK handler(HWND hwnd, UINT umessage, WPARAM wparam, LPARAM lparam);
        core::Real pixel_to_normal_pos_x(int x) const;
        core::Real pixel_to_normal_pos_y(int y) const;
        void update_mouse_position();
        void update_screen_sizes();

    public:
        Application();
        ~Application();
        void execute(core::Application* core_app);
        render::Engine* get_render_engine();
        const render::Engine* get_render_engine() const;
        core::Application* get_core_app();
        const core::Application* get_core_app() const;
        core::asset::Manager* get_asset_manager();
        const core::asset::Manager* get_asset_manager() const;
        core::Real get_window_ratio() const;
        unsigned int get_width() const;
        unsigned int get_height() const;
        HWND get_window();
    };
}
}
#endif // USE_WINAPI
#endif // GEAROENIX_SYSTEM_WINDOWS_APPLICATION_HPP
