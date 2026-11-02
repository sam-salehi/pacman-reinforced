// Python bindings for the SFML front-end (module `pacman`). Used for playing / watching agents.
// Training uses the headless module `pacman_sim` instead (see game/sim_bindings.cpp).
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "display.hpp"
#include "../game/game.hpp"

namespace py = pybind11;

PYBIND11_MODULE(pacman, m) {
    py::class_<Display>(m, "Display")
        .def(py::init<uint32_t, bool>(), py::arg("seed") = 0, py::arg("limit_framerate") = true)
        .def("running", &Display::running)
        .def("close", &Display::close)
        .def("update", &Display::update)
        .def("render", &Display::render)
        .def("step", &Display::step)
        .def("pacman_contained", &Display::pacmanContainedInCell)
        .def("game_over", [](Display& d) { return d.simulator().gameOver(); })
        .def("game_won", [](Display& d) { return d.simulator().gameWon(); })
        .def("get_score", [](Display& d) { return d.simulator().score(); })
        .def("pellets_remaining", [](Display& d) { return d.simulator().pelletsRemaining(); })
        .def("valid_actions", [](Display& d) {
            py::array_t<uint8_t> m(4); d.simulator().validActions(m.mutable_data()); return m;
        })
        .def("observation", [](Display& d) {
            py::array_t<uint8_t> planes({OBS_PLANES, OBS_H, OBS_W});
            py::array_t<float> scalars(OBS_SCALARS);
            d.simulator().writeObservation(planes.mutable_data(), scalars.mutable_data());
            return py::make_tuple(planes, scalars);
        })
        .def("get_screenshot", [](Display &self) {
            sf::Image screenshot = self.getScreenshot();
            const sf::Uint8* pixels = screenshot.getPixelsPtr();
            int width = screenshot.getSize().x;
            int height = screenshot.getSize().y;
            return py::array_t<uint8_t>({height, width, 4}, pixels);
        });
}
