#include <twodog/core/Application.hpp>
#include <twodog/core/LaunchOptions.hpp>
#include <twodog/core/WindowConfig.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace twodog;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void near(double actual, double expected, double tolerance = 1e-8) {
    if (std::abs(actual - expected) > tolerance || !std::isfinite(actual))
        throw std::runtime_error("Expected " + std::to_string(expected) + ", got " + std::to_string(actual));
}
template<typename Exception = std::invalid_argument, typename F> void rejects(F function) {
    try { function(); } catch (const Exception&) { return; }
    throw std::runtime_error("Expected exception was not thrown");
}
LaunchOptions options(std::initializer_list<std::string_view> args) {
    return parseOptions(std::span<const std::string_view>(args.begin(), args.size()));
}
class FakeHost final : public FrameHost {
public:
    double time = 0.0;
    double increment = 0.01;
    bool closed = false;
    bool paused = false;
    bool failRender = false;
    int polls = 0, renders = 0, presents = 0, waits = 0;
    std::vector<double> deltas, alphas;
    std::vector<std::string> events;
    std::function<void(FakeHost&)> onPoll;
    std::function<void(FakeHost&)> onWait;
    void pollEvents() override {
        ++polls; time += increment; events.emplace_back("poll");
        if (onPoll) onPoll(*this);
    }
    bool shouldClose() const override { return closed; }
    bool suspended() const override { return paused; }
    void waitEvents(double timeout) override {
        near(timeout, 0.1); ++waits; time += 100.0;
        if (onWait) onWait(*this);
    }
    double nowSeconds() const override { return time; }
    void fixedUpdate(double dt) override { deltas.push_back(dt); events.emplace_back("fixed"); }
    void render(double alpha) override {
        if (failRender) throw std::runtime_error("render failure");
        ++renders; alphas.push_back(alpha); events.emplace_back("render");
    }
    void present() override { ++presents; events.emplace_back("present"); }
};
const std::map<std::string, std::function<void()>> cases = {
    {"clock_initial", [] {
        FixedStepClock clock;
        const auto p = clock.advance(0.0);
        require(p.fixedSteps == 0, "No tick before elapsed time"); near(p.alpha, 0); near(p.droppedSeconds, 0);
    }},
    {"clock_step", [] {
        FixedStepClock clock;
        const auto p = clock.advance(1.0 / 60.0);
        require(p.fixedSteps == 1, "One tick"); near(p.alpha, 0);
    }},
    {"clock_fraction", [] {
        FixedStepClock clock;
        near(clock.advance(1.0 / 120.0).alpha, 0.5);
        const auto p = clock.advance(1.0 / 120.0);
        require(p.fixedSteps == 1, "Fractions accumulate"); near(p.alpha, 0);
    }},
    {"clock_multiple", [] {
        FixedStepClock clock({0.01, 0.25, 8});
        const auto p = clock.advance(0.0375);
        require(p.fixedSteps == 3, "Three updates"); near(p.alpha, 0.75);
    }},
    {"clock_partition", [] {
        FixedStepClock a({0.01, 0.25, 100}), b({0.01, 0.25, 100});
        std::size_t sum = 0;
        for (int i = 0; i < 20; ++i) sum += a.advance(0.005).fixedSteps;
        require(sum == b.advance(0.1).fixedSteps, "Tick count independent of render partition");
    }},
    {"clock_clamp", [] {
        FixedStepClock clock({0.01, 0.1, 100});
        const auto p = clock.advance(2.0);
        require(p.fixedSteps == 10, "Clamp prevents two-second catch-up"); near(p.droppedSeconds, 1.9);
        near(p.acceptedSeconds, 0.1);
    }},
    {"clock_overload", [] {
        FixedStepClock clock({0.01, 0.25, 3});
        const auto p = clock.advance(0.105);
        require(p.fixedSteps == 3, "Catch-up work bounded"); near(p.droppedSeconds, 0.07); near(p.alpha, 0.5);
        const auto next = clock.advance(0.005);
        require(next.fixedSteps == 1, "Remainder survives dropped whole ticks"); near(next.alpha, 0);
    }},
    {"clock_invalid_config", [] {
        for (const double value : {0.0, -1.0, 1e-12, 2.0, std::numeric_limits<double>::infinity(),
                                   std::numeric_limits<double>::quiet_NaN()})
            rejects([&] { FixedStepClock c({value, 0.25, 8}); });
        for (const double value : {0.0, 0.001, 11.0, std::numeric_limits<double>::infinity()})
            rejects([&] { FixedStepClock c({0.01, value, 8}); });
        rejects([] { FixedStepClock c({0.01, 0.25, 0}); });
        rejects([] { FixedStepClock c({0.01, 0.25, 10001}); });
    }},
    {"clock_invalid_delta", [] {
        FixedStepClock c({0.01, 0.25, 8});
        static_cast<void>(c.advance(0.005));
        for (const double value : {-0.1, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
            rejects([&] { static_cast<void>(c.advance(value)); });
        require(c.advance(0.005).fixedSteps == 1, "Invalid input leaves clock unchanged");
    }},
    {"clock_reset", [] {
        FixedStepClock c({0.01, 0.25, 8});
        static_cast<void>(c.advance(0.008)); c.reset();
        const auto p = c.advance(0.002);
        require(p.fixedSteps == 0, "Reset clears remainder"); near(p.alpha, 0.2);
    }},
    {"clock_long_run", [] {
        FixedStepClock c; std::size_t ticks = 0;
        for (int i = 0; i < 144000; ++i) {
            const auto p = c.advance(1.0 / 144.0); ticks += p.fixedSteps;
            require(p.alpha >= 0.0 && p.alpha < 1.0, "Interpolation remains in range");
        }
        require(ticks == 60000, "60 Hz simulation across 1000 seconds at 144 Hz rendering");
    }},
    {"clock_conservation", [] {
        FixedStepClock c({0.01, 0.25, 8});
        double supplied = 0, dropped = 0, simulated = 0, remainder = 0;
        unsigned int random = 731U;
        for (int i = 0; i < 10000; ++i) {
            random = random * 1664525U + 1013904223U;
            const double delta = static_cast<double>(random % 4000U) / 10000.0;
            const auto p = c.advance(delta);
            supplied += delta; dropped += p.droppedSeconds;
            simulated += static_cast<double>(p.fixedSteps) * 0.01; remainder = p.alpha * 0.01;
        }
        near(supplied, dropped + simulated + remainder, 1e-7);
    }},
    {"clock_extreme_delta", [] {
        FixedStepClock c;
        const auto p = c.advance(std::numeric_limits<double>::max());
        require(p.fixedSteps <= 8 && std::isfinite(p.droppedSeconds), "Finite maximum cannot overflow scheduler");
        require(p.alpha >= 0 && p.alpha < 1, "Huge stall keeps a valid fraction");
    }},
    {"options_default", [] {
        const auto o = options({});
        require(o.vsync && !o.hidden && !o.help && !o.smokeTest && o.maxFrames == 0, "Launch defaults");
    }},
    {"options_flags", [] {
        const auto o = options({"--hidden", "--no-vsync"});
        require(o.hidden && !o.vsync, "Explicit flags");
        const auto s = options({"--smoke-test"});
        require(s.hidden && !s.vsync && s.maxFrames == 3, "Smoke is bounded and invisible");
    }},
    {"options_frames", [] {
        require(options({"--frames", "20"}).maxFrames == 20, "Frame parsing");
        require(options({"--frames", "18446744073709551615"}).maxFrames == std::numeric_limits<std::uint64_t>::max(), "uint64 max");
    }},
    {"options_invalid", [] {
        for (const auto value : {"0", "-1", "3x", "1.2", "", "+3", " 3", "18446744073709551616"})
            rejects([&] { static_cast<void>(options({"--frames", value})); });
        rejects([] { static_cast<void>(options({"--frames"})); });
        rejects([] { static_cast<void>(options({"--unknown"})); });
        rejects([] { static_cast<void>(options({"--frames", "2", "--frames", "3"})); });
        rejects([] { static_cast<void>(options({"--smoke-test", "--frames", "3"})); });
        rejects([] { static_cast<void>(options({"--capture", "shot.ppm"})); });
        rejects([] { static_cast<void>(options({"--assets"})); });
        rejects([] { static_cast<void>(options({"--assets", "a", "--assets", "b"})); });
        rejects([] { static_cast<void>(options({"--capture", "shot.ppm", "--frames", "2", "--window-only"})); });
        require(options({"--capture", "shot.ppm", "--frames", "2"}).capturePath=="shot.ppm", "Capture path retained");
    }},
    {"options_help", [] { require(options({"--help"}).help, "Help requested"); }},
    {"config_default", [] { WindowConfig{}.validate(); }},
    {"config_invalid", [] {
        for (const int value : {0, -1, 32769}) {
            WindowConfig c; c.width = value; rejects([&] { c.validate(); });
            c.width = 1280; c.height = value; rejects([&] { c.validate(); });
        }
        WindowConfig c; c.title = ""; rejects([&] { c.validate(); });
        c.title = std::string("A\0B", 3); rejects([&] { c.validate(); });
    }},
    {"loop_frame_limit", [] {
        FakeHost h;
        const auto stats = runApplication(h, 5, {0.01, 0.25, 8});
        require(stats.frames == 5 && h.polls == 5 && h.presents == 5 && h.renders == 5, "Exactly five frames");
        require(stats.fixedUpdates == 5, "Exactly five simulation ticks");
    }},
    {"loop_initial_close", [] {
        FakeHost h; h.closed = true;
        require(runApplication(h).frames == 0 && h.polls == 0, "Closed host is never polled or rendered");
    }},
    {"loop_poll_close", [] {
        FakeHost h; h.onPoll = [](FakeHost& f) { if (f.polls == 2) f.closed = true; };
        const auto s = runApplication(h);
        require(s.frames == 1 && h.presents == 1, "Close event prevents another render");
    }},
    {"loop_fixed_before_render", [] {
        FakeHost h; h.increment = 0.025;
        const auto s = runApplication(h, 1, {0.01, 0.25, 8});
        require(s.fixedUpdates == 2, "Multiple fixed steps");
        require(h.events == std::vector<std::string>{"poll", "fixed", "fixed", "render", "present"}, "Frame operation order");
        for (const auto dt : h.deltas) near(dt, 0.01);
        near(h.alphas.at(0), 0.5);
    }},
    {"loop_suspend", [] {
        FakeHost h; h.paused = true;
        h.onWait = [](FakeHost& f) { f.closed = true; };
        const auto s = runApplication(h);
        require(h.waits == 1 && s.frames == 0 && s.fixedUpdates == 0, "Suspended window waits without simulation");
    }},
    {"loop_resume", [] {
        FakeHost h; h.paused = true;
        h.onWait = [](FakeHost& f) { f.paused = false; };
        const auto s = runApplication(h, 2, {0.01, 0.25, 8});
        require(s.fixedUpdates == 2 && s.frames == 2, "Resume excludes paused wall time"); near(s.droppedSeconds, 0);
    }},
    {"loop_escape_exception", [] {
        FakeHost h; h.failRender = true;
        rejects<std::runtime_error>([&] { static_cast<void>(runApplication(h, 1)); });
        require(h.presents == 0, "No presentation after failed render");
    }},
    {"loop_wait_close", [] {
        FakeHost h; h.paused = true;
        h.onWait = [](FakeHost& f) { f.closed = true; f.paused = false; };
        require(runApplication(h).frames == 0 && h.polls == 1, "Close during wait exits without next frame");
    }}
};
}
int main(int argc, char** argv) {
    if (argc != 2 || !cases.contains(argv[1])) { std::cerr << "Unknown test case\n"; return 2; }
    try { cases.at(argv[1])(); std::cout << "PASS " << argv[1] << '\n'; return 0; }
    catch (const std::exception& e) { std::cerr << "FAIL " << argv[1] << ": " << e.what() << '\n'; return 1; }
}
