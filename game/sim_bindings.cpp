// Headless python bindings (module `pacman_sim`). No SFML / window required.
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <random>
#include <vector>

#include "simulator.hpp"

namespace py = pybind11;

using u8arr = py::array_t<uint8_t, py::array::c_style>;
using f32arr = py::array_t<float, py::array::c_style>;
using i32arr = py::array_t<int32_t, py::array::c_style>;
using i64arr = py::array_t<int64_t, py::array::c_style | py::array::forcecast>;

// Event columns returned by BatchSim.step
enum EventCol { EV_PELLETS = 0, EV_POWER, EV_GHOSTS, EV_DIED, EV_WON, EV_FRAMES, EV_COLS };

// N independent games stepped in lock-step at the decision level, with automatic reset.
class BatchSim {
    std::vector<Simulator> sims;
    std::mt19937 seeder;
    int maxDecisions;

    void observe(int i, uint8_t* planes, float* scalars, uint8_t* masks) {
        const int P = OBS_PLANES * OBS_H * OBS_W;
        sims[i].writeObservation(planes + (size_t)i * P, scalars + (size_t)i * OBS_SCALARS);
        sims[i].validActions(masks + (size_t)i * 4);
    }

public:
    BatchSim(int n, uint32_t seed, int maxDecisions) : seeder(seed), maxDecisions(maxDecisions) {
        sims.reserve(n);
        for (int i = 0; i < n; ++i) sims.emplace_back(seeder());
    }

    int size() const { return (int)sims.size(); }

    py::tuple reset() {
        const int n = size();
        u8arr planes({n, OBS_PLANES, OBS_H, OBS_W});
        f32arr scalars({n, OBS_SCALARS});
        u8arr masks({n, 4});
        for (int i = 0; i < n; ++i) {
            sims[i].reset(seeder());
            observe(i, planes.mutable_data(), scalars.mutable_data(), masks.mutable_data());
        }
        return py::make_tuple(planes, scalars, masks);
    }

    // Returns (planes, scalars, masks, events[N,EV_COLS], done[N], truncated[N], ep_score[N], ep_len[N], ep_won[N]).
    // For finished games, the observation is that of the freshly reset game; ep_* hold final stats.
    py::tuple step(i64arr actions) {
        const int n = size();
        if (actions.ndim() != 1 || actions.shape(0) != n) throw std::invalid_argument("actions must have shape [N]");
        auto a = actions.unchecked<1>();

        u8arr planes({n, OBS_PLANES, OBS_H, OBS_W});
        f32arr scalars({n, OBS_SCALARS});
        u8arr masks({n, 4});
        i32arr events({n, (int)EV_COLS});
        py::array_t<bool> done(n), truncated(n), epWon(n);
        i32arr epScore(n), epLen(n);
        auto ev = events.mutable_unchecked<2>();
        auto d = done.mutable_unchecked<1>();
        auto tr = truncated.mutable_unchecked<1>();
        auto ew = epWon.mutable_unchecked<1>();
        auto es = epScore.mutable_unchecked<1>();
        auto el = epLen.mutable_unchecked<1>();

        // pre-draw seeds so results don't depend on thread scheduling
        std::vector<uint32_t> seeds(n);
        for (int i = 0; i < n; ++i) seeds[i] = seeder();

        uint8_t* pl = planes.mutable_data();
        float* sc = scalars.mutable_data();
        uint8_t* mk = masks.mutable_data();

        {
            py::gil_scoped_release release;
            // a decision costs ~1us, so threads only pay off for large batches
            #pragma omp parallel for schedule(static) if(n >= 1024)
            for (int i = 0; i < n; ++i) {
                Simulator& s = sims[i];
                StepEvents e = s.act((int)a(i));
                ev(i, EV_PELLETS) = e.pellets;
                ev(i, EV_POWER) = e.powerPellets;
                ev(i, EV_GHOSTS) = e.ghosts;
                ev(i, EV_DIED) = e.died;
                ev(i, EV_WON) = e.won;
                ev(i, EV_FRAMES) = e.frames;
                const bool over = s.gameOver();
                const bool timeUp = !over && (int)s.decisionCount() >= maxDecisions;
                d(i) = over || timeUp;
                tr(i) = timeUp;
                ew(i) = s.gameWon();
                es(i) = (int32_t)s.score();
                el(i) = (int32_t)s.decisionCount();
                if (d(i)) s.reset(seeds[i]);
                observe(i, pl, sc, mk);
            }
        }
        return py::make_tuple(planes, scalars, masks, events, done, truncated, epScore, epLen, epWon);
    }
};

PYBIND11_MODULE(pacman_sim, m) {
    m.doc() = "Headless deterministic Pacman simulator for reinforcement learning";
    m.attr("OBS_PLANES") = OBS_PLANES;
    m.attr("OBS_SCALARS") = OBS_SCALARS;
    m.attr("OBS_H") = OBS_H;
    m.attr("OBS_W") = OBS_W;
    m.attr("EVENT_COLUMNS") = py::make_tuple("pellets", "power_pellets", "ghosts", "died", "won", "frames");

    py::class_<Simulator>(m, "Simulator")
        .def(py::init<uint32_t>(), py::arg("seed") = 0)
        .def("reset", &Simulator::reset, py::arg("seed"))
        .def("act", [](Simulator& s, int action) {
            StepEvents e = s.act(action);
            py::dict out;
            out["frames"] = e.frames; out["pellets"] = e.pellets; out["power_pellets"] = e.powerPellets;
            out["ghosts"] = e.ghosts; out["died"] = e.died; out["won"] = e.won;
            return out;
        })
        .def("frame", [](Simulator& s) { s.frame(); })
        .def("valid_actions", [](const Simulator& s) {
            u8arr m(4); s.validActions(m.mutable_data()); return m;
        })
        .def("observation", [](const Simulator& s) {
            u8arr planes({OBS_PLANES, OBS_H, OBS_W});
            f32arr scalars(OBS_SCALARS);
            s.writeObservation(planes.mutable_data(), scalars.mutable_data());
            return py::make_tuple(planes, scalars);
        })
        .def("decision_available", &Simulator::decisionAvailable)
        .def("game_over", &Simulator::gameOver)
        .def("game_won", &Simulator::gameWon)
        .def("score", &Simulator::score)
        .def("pellets_remaining", &Simulator::pelletsRemaining)
        .def("decisions", &Simulator::decisionCount)
        .def("frames", &Simulator::frameCount);

    py::class_<BatchSim>(m, "BatchSim")
        .def(py::init<int, uint32_t, int>(), py::arg("num_envs"), py::arg("seed") = 0, py::arg("max_decisions") = 3000)
        .def("__len__", &BatchSim::size)
        .def("reset", &BatchSim::reset)
        .def("step", &BatchSim::step, py::arg("actions"));
}
