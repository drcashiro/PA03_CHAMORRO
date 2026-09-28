#include "pa3/Window.h"
#include "pa3/Renderer.h"
#include <algorithm>
#include <iostream>
#include <string>

namespace {
int integer(const char* text) {
    std::size_t used = 0;
    const std::string value(text);
    const int result = std::stoi(value, &used);
    if (used != value.size()) throw std::runtime_error("Invalid integer: " + value);
    return result;
}
}
int main(int argc, char** argv) {
    try {
        int frameLimit = 0, experiment = 0;
        std::string capture;
        for (int i = 1; i < argc; ++i) {
            const std::string option(argv[i]);
            if (option == "--help") {
                std::cout << "PA3Scene [--frames N] [--case 0..5] [--capture image.ppm]\n";
                return 0;
            }
            if (i+1 >= argc) throw std::runtime_error("Missing value for " + option);
            if (option == "--frames") {
                frameLimit = integer(argv[++i]);
                if (frameLimit <= 0) throw std::runtime_error("Frame count must be positive");
            } else if (option == "--case") experiment = integer(argv[++i]);
            else if (option == "--capture") capture = argv[++i];
            else throw std::runtime_error("Unknown option: " + option);
        }
        if (experiment < 0 || experiment > 5) throw std::runtime_error("Case must be in [0,5]");
        if (!capture.empty() && frameLimit == 0) frameLimit = 1;
        pa3::Window window;
        {
            // All GPU-owning objects are destroyed while window/context is still alive.
            pa3::Renderer renderer;
            pa3::Scene scene;
            pa3::Camera camera;
            scene.applyCase(experiment, camera);
            window.checkErrors();
            std::cout << "Arrows: orbit | W/S: zoom | A/D Q/E I/K: move point light\n"
                      << "T: texture | B: specular | P/L: lights | Space: pause | R: reset\n"
                      << "0..5: experiments | [/]: sphere shininess | Esc: quit\n";
            double last = glfwGetTime(), nextTitle = 0;
            int rendered = 0;
            while (!glfwWindowShouldClose(window.native())) {
                glfwPollEvents();
                if (window.down(GLFW_KEY_ESCAPE)) break;
                const double now = glfwGetTime();
                const float dt = frameLimit > 0 ? 1.f/60.f : static_cast<float>(std::clamp(now-last, 0.0, .05));
                last = now;
                const auto [width, height] = window.framebuffer();
                if (width <= 0 || height <= 0) { glfwWaitEventsTimeout(.05); continue; }
                scene.input(window, camera, dt);
                scene.update(dt);
                renderer.draw(scene, camera, width, height);
                if (!capture.empty() && rendered+1 == frameLimit) renderer.capture(capture,width,height);
                window.checkErrors();
                glfwSwapBuffers(window.native());
                if (now >= nextTitle) {
                    const auto title = scene.status();
                    glfwSetWindowTitle(window.native(), title.c_str());
                    nextTitle = now+.2;
                }
                ++rendered;
                if (frameLimit > 0 && rendered >= frameLimit) break;
            }
        }
        window.checkErrors();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fatal: " << error.what() << '\n';
        return 1;
    }
}
