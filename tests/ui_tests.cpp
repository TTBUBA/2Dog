#include <cmath>
#include <iostream>
#include <stdexcept>
#include <twodog/gameplay/Input.hpp>
#include <twodog/platform/GameUI.hpp>
using namespace twodog;
namespace {
void require(bool ok, const char *why) {
    if (!ok)
        throw std::runtime_error(why);
}
void near(float a, float b) { require(std::abs(a - b) < .01F, "UI rectangle differs from expected layout"); }
Json node(const char *id) {
    return Json::Object{
        {"id", id},
        {"ui",
         Json::parse(
             R"({"kind":"Panel","parent":"","anchorMin":[0,0],"anchorMax":[0,0],"pivot":[0,0],"offset":[20,30],"size":[200,50],"order":0})")}};
}
template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    require(rejected, "Invalid UI was accepted");
}
void run(const std::string &test) {
    auto a = node("a");
    auto b = node("b");
    Json scene = Json::Object{{"entities", Json::Array{a}}};
    if (test == "anchors") {
        auto &u = scene["entities"].array()[0]["ui"];
        u["anchorMin"] = Json::Array{1, 1};
        u["anchorMax"] = Json::Array{1, 1};
        u["pivot"] = Json::Array{1, 1};
        u["offset"] = Json::Array{-20, -30};
        const auto items = GameUI::layout(scene, 1280, 720);
        near(items.at(0).rect.x, 1060);
        near(items.at(0).rect.y, 640);
        return;
    }
    if (test == "scaling") {
        const auto full = GameUI::layout(scene, 1280, 720), half = GameUI::layout(scene, 640, 360);
        near(full.at(0).rect.x, half.at(0).rect.x * 2);
        near(full.at(0).rect.w, half.at(0).rect.w * 2);
        return;
    }
    if (test == "layout") {
        a["ui"]["kind"] = "Layout";
        a["ui"]["size"] = Json::Array{200, 200};
        a["ui"]["padding"] = 10;
        b["ui"]["parent"] = "a";
        b["ui"]["order"] = 1;
        scene["entities"] = Json::Array{b, a};
        const auto items = GameUI::layout(scene, 1280, 720);
        require(items.at(0).id == "a", "Parent must render before child");
        near(items.at(1).rect.x, 30);
        near(items.at(1).rect.y, 40);
        near(items.at(1).rect.w, 180);
        return;
    }
    if (test == "cycle") {
        a["ui"]["parent"] = "b";
        b["ui"]["parent"] = "a";
        scene["entities"] = Json::Array{a, b};
        rejects([&] { GameUI::layout(scene, 1280, 720); });
        return;
    }
    if (test == "hidden") {
        a["ui"]["visible"] = false;
        b["ui"]["parent"] = "a";
        scene["entities"] = Json::Array{a, b};
        require(GameUI::layout(scene, 1280, 720).empty(), "Hidden parent must hide descendants");
        return;
    }
    if (test == "validation") {
        a["ui"]["value"] = 2;
        rejects([&] { GameUI::validate(a); });
        a = node("a");
        a["ui"]["parent"] = "missing";
        scene["entities"] = Json::Array{a};
        rejects([&] { GameUI::layout(scene, 1280, 720); });
        return;
    }
    if (test == "canvas" || test == "canvas_validation") {
        a["ui"]["kind"] = "Canvas";
        a["ui"]["referenceResolution"] = Json::Array{1280, 720};
        a["ui"]["match"] = .5;
        b["ui"]["parent"] = "a";
        b["ui"]["anchorMin"] = b["ui"]["anchorMax"] = b["ui"]["pivot"] = Json::Array{1, 1};
        b["ui"]["offset"] = Json::Array{-20, -30};
        scene["entities"] = Json::Array{b, a};
        if (test == "canvas_validation") {
            a["ui"]["referenceResolution"] = Json::Array{0, 720};
            rejects([&] { GameUI::validate(a); });
            a["ui"]["referenceResolution"] = Json::Array{1280, 720};
            a["ui"]["match"] = 2;
            rejects([&] { GameUI::validate(a); });
            a["ui"]["match"] = .5;
            a["ui"]["parent"] = "b";
            rejects([&] { GameUI::validate(a); });
            return;
        }
        for (const auto size : {glm::ivec2(1280,720), glm::ivec2(1920,1080), glm::ivec2(720,1280)}) {
            const auto items = GameUI::layout(scene, size.x, size.y);
            require(items.size() == 2 && items[0].id == "a", "Canvas must precede children");
            near(items[0].rect.x, 0); near(items[0].rect.y, 0);
            near(items[0].rect.w, static_cast<float>(size.x)); near(items[0].rect.h, static_cast<float>(size.y));
            const float scale = std::sqrt((static_cast<float>(size.x) / 1280) * (static_cast<float>(size.y) / 720));
            near(items[1].scale, scale);
            near(items[1].rect.x + items[1].rect.w, static_cast<float>(size.x) - 20 * scale);
            near(items[1].rect.y + items[1].rect.h, static_cast<float>(size.y) - 30 * scale);
        }
        b["ui"]["anchorMin"] = Json::Array{0,0}; b["ui"]["anchorMax"] = Json::Array{1,1};
        b["ui"]["pivot"] = Json::Array{.5,.5}; b["ui"]["size"] = Json::Array{-40,-40};
        b["ui"]["offset"] = Json::Array{0,0};
        scene["entities"] = Json::Array{a,b};
        auto items = GameUI::layout(scene, 1280,720);
        near(items[1].rect.x,20); near(items[1].rect.w,1240); near(items[1].rect.h,680);
        scene["entities"].array()[0]["ui"]["visible"] = false;
        require(GameUI::layout(scene,1280,720).empty(), "Hidden Canvas must hide descendants");
        return;
    }
    if (test == "capture") {
        Input input;
        input.bind("jump", {{InputDevice::Keyboard, 32}});
        input.bind("spawn", {{InputDevice::Mouse, 0}});
        input.button(InputDevice::Keyboard, 32, true);
        input.button(InputDevice::Mouse, 0, true);
        input.setBlocked(false, true, false);
        require(input.action("jump").pressed && !input.action("spawn").pressed,
                "UI mouse capture must leave keyboard available");
        auto fixed = input.consumeFixed();
        require(!fixed.at("spawn").pressed, "Blocked click leaked into fixed update");
        input.setBlocked(false, false, false);
        fixed = input.consumeFixed();
        require(!fixed.at("spawn").pressed, "Blocked pending click replayed after capture");
        return;
    }
    throw std::invalid_argument("Unknown UI test");
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 2)
            throw std::invalid_argument("Choose one UI test");
        run(argv[1]);
        std::cout << "PASS ui." << argv[1] << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL ui: " << e.what() << '\n';
        return 1;
    }
}
