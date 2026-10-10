#include <twodog/data/ComponentSchema.hpp>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <twodog/gameplay/Input.hpp>
#include <twodog/platform/GameUI.hpp>

namespace twodog {
namespace {
void requireUI(bool ok, const char *text) {
    if (!ok)
        throw std::invalid_argument(text);
}
bool inside(const GameUI::Rect &r, float x, float y) { return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h; }
} // namespace
void GameUI::validate(const Json &entity) {
    if(entity.contains("ui")) ComponentSchema::validateUI(entity.at("ui"));
}
std::vector<GameUI::Item> GameUI::layout(const Json &scene, int width, int height) {
    requireUI(width > 0 && height > 0, "UI viewport must have positive dimensions");
    const float scale = std::max(.01F, std::min(static_cast<float>(width) / 1280, static_cast<float>(height) / 720));
    const Rect canvas{0, 0, static_cast<float>(width) / scale, static_cast<float>(height) / scale};
    std::map<std::string, const Json *, std::less<>> nodes;
    std::vector<std::string> order;
    for (const auto &e : scene.at("entities").array())
        if (e.contains("ui")) {
            validate(e);
            const auto id = e.at("id").string();
            nodes.emplace(id, &e.at("ui"));
            order.push_back(id);
        }
    std::stable_sort(order.begin(), order.end(), [&](const std::string &a, const std::string &b) {
        const auto oa = nodes.at(a)->num("order"), ob = nodes.at(b)->num("order");
        return oa == ob ? a < b : oa < ob;
    });
    std::map<std::string, Rect, std::less<>> resolved;
    std::map<std::string, float, std::less<>> scales;
    std::set<std::string> stack, hidden;
    std::function<Rect(const std::string &)> resolve = [&](const std::string &id) -> Rect {
        if (resolved.contains(id))
            return resolved.at(id);
        requireUI(stack.size() < 64 && stack.insert(id).second, "UI parent cycle or hierarchy deeper than 64");
        const auto &u = *nodes.at(id);
        const auto parent = u.text("parent");
        Rect area = canvas;
        float nodeScale = scale;
        if (!parent.empty()) {
            requireUI(nodes.contains(parent), "UI parent must refer to another UI object");
            area = resolve(parent);
            nodeScale = scales.at(parent);
        }
        if (!u.flag("visible", true) || hidden.contains(parent))
            hidden.insert(id);
        if (u.text("kind") == "Canvas") {
            const auto reference = jsonVec2(u.at("referenceResolution"));
            const float match = static_cast<float>(u.num("match", .5));
            nodeScale = std::max(.01F, std::exp2((1 - match) * std::log2(static_cast<float>(width) / reference.x) +
                                                match * std::log2(static_cast<float>(height) / reference.y)));
            const Rect full{0, 0, static_cast<float>(width) / nodeScale, static_cast<float>(height) / nodeScale};
            scales.emplace(id, nodeScale);
            resolved.emplace(id, full);
            stack.erase(id);
            return full;
        }
        scales.emplace(id, nodeScale);
        const auto amin = jsonVec2(u.at("anchorMin")), amax = jsonVec2(u.at("anchorMax"));
        const auto offset = jsonVec2(u.at("offset")), size = jsonVec2(u.at("size")), pivot = jsonVec2(u.at("pivot"));
        Rect r{0, 0, area.w * (amax.x - amin.x) + size.x, area.h * (amax.y - amin.y) + size.y};
        requireUI(r.w > 0 && r.h > 0, "UI rectangle must have positive size");
        r.x = area.x + area.w * amin.x + offset.x - pivot.x * size.x;
        r.y = area.y + area.h * amin.y + offset.y - pivot.y * size.y;
        if (!parent.empty() && nodes.at(parent)->text("kind") == "Layout") {
            const auto &container = *nodes.at(parent);
            const float pad = static_cast<float>(container.num("padding", 8));
            float cursor = pad;
            const bool vertical = container.text("direction", "vertical") == "vertical";
            for (const auto &other : order) {
                if (other == id)
                    break;
                const auto &sibling = *nodes.at(other);
                if (sibling.text("parent") == parent && sibling.flag("visible", true)) {
                    const auto s = jsonVec2(sibling.at("size"));
                    cursor += (vertical ? s.y : s.x) + static_cast<float>(container.num("spacing", 8));
                }
            }
            r.x = area.x + (vertical ? pad : cursor);
            r.y = area.y + (vertical ? cursor : pad);
            r.w = vertical ? std::max(1.0F, area.w - 2 * pad) : size.x;
            r.h = vertical ? size.y : std::max(1.0F, area.h - 2 * pad);
        }
        stack.erase(id);
        resolved.emplace(id, r);
        return r;
    };
    for (const auto &id : order)
        resolve(id);
    // Parents precede descendants. Stable sibling order is the scene order.
    std::vector<Item> result;
    std::set<std::string> emitted;
    std::function<void(const std::string &)> emit = [&](const std::string &id) {
        if (emitted.contains(id))
            return;
        const auto &u = *nodes.at(id);
        const auto p = u.text("parent");
        if (!p.empty())
            emit(p);
        emitted.insert(id);
        if (hidden.contains(id))
            return;
        auto r = resolved.at(id);
        const float itemScale = scales.at(id);
        r.x *= itemScale;
        r.y *= itemScale;
        r.w *= itemScale;
        r.h *= itemScale;
        result.push_back({id, r, u, itemScale});
    };
    for (const auto &id : order)
        emit(id);
    return result;
}
void GameUI::reset() {
    hovered_.clear();
    pressed_.clear();
    focus_.clear();
    mouseCapture_ = false;
}
void GameUI::poll(World2D &world, Window &window, int width, int height, const std::function<void(Json)> &send,
                  bool acceptInput) {
    if (!acceptInput) {
        reset();
        return;
    }
    auto &input = window.input();
    const auto items = layout(world.snapshot(), width, height);
    auto event = [&](const Item &item, const std::string &suffix) {
        const auto &u = item.data;
        send(Json::Object{{"entity", u.text("target", item.id)},
                          {"topic", u.text("topic", "ui") + "." + suffix},
                          {"payload", Json::Object{{"source", item.id},
                                                   {"value", world.get(item.id).document.at("ui").num("value", .5)}}}});
    };
    const Item *hit = nullptr;
    for (const auto &item : items)
        if (item.data.text("kind") != "Canvas" && item.data.flag("blockPointer", true) &&
            inside(item.rect, static_cast<float>(input.cursorX), static_cast<float>(input.cursorY)))
            hit = &item;
    const std::string hover = hit ? hit->id : "";
    if (hover != hovered_) {
        for (const auto &item : items)
            if (item.id == hovered_)
                event(item, "exit");
        if (hit)
            event(*hit, "hover");
        hovered_ = hover;
    }
    std::vector<const Item *> interactive;
    for (const auto &item : items)
        if (item.data.flag("interactable", true) &&
            (item.data.text("kind") == "Button" || item.data.text("kind") == "Slider"))
            interactive.push_back(&item);
    if (std::none_of(interactive.begin(), interactive.end(), [&](const Item *i) { return i->id == focus_; }))
        focus_.clear();
    if (input.action("ui.tab").pressed && !interactive.empty()) {
        const auto it =
            std::find_if(interactive.begin(), interactive.end(), [&](const Item *i) { return i->id == focus_; });
        const auto next =
            it == interactive.end() ? 0 : (static_cast<std::size_t>(it - interactive.begin()) + 1) % interactive.size();
        focus_ = interactive[next]->id;
    }
    const auto click = input.action("ui.pointer");
    if (click.pressed) {
        pressed_.clear();
        focus_.clear();
        if (hit && hit->data.flag("interactable", true) &&
            (hit->data.text("kind") == "Button" || hit->data.text("kind") == "Slider")) {
            pressed_ = hit->id;
            focus_ = hit->id;
        }
    }
    for (const auto *item : interactive) {
        if (item->data.text("kind") == "Button" && ((click.released && pressed_ == item->id && hover == item->id) ||
                                                    (focus_ == item->id && input.action("ui.submit").pressed)))
            event(*item, "click");
        if (item->data.text("kind") == "Slider") {
            auto &component = world.get(item->id).document.at("ui");
            if (!component.contains("value"))
                component["value"] = .5;
            auto &value = component["value"];
            const double old = value.null() ? .5 : value.number();
            double next = old;
            if (pressed_ == item->id && click.down)
                next = std::clamp((input.cursorX - item->rect.x) / item->rect.w, 0.0, 1.0);
            if (focus_ == item->id)
                next = std::clamp(next + (input.action("ui.right").pressed ? .05 : 0) -
                                      (input.action("ui.left").pressed ? .05 : 0),
                                  0.0, 1.0);
            if (next != old) {
                value = next;
                event(*item, "change");
            }
        }
    }
    mouseCapture_ = hit != nullptr || !pressed_.empty();
    if (click.released || !click.down)
        pressed_.clear();
}
void GameUI::draw(World2D &world, VisualAssets &assets, SpriteRenderer &renderer, int width, int height) {
    const auto items = layout(world.snapshot(), width, height);
    Camera2D camera;
    camera.width = width;
    camera.height = height;
    camera.pixelsPerUnit = 1;
    camera.center = {static_cast<float>(width) * .5F, static_cast<float>(height) * .5F};
    std::vector<SpriteDraw> draws;
    int layer = 1000;
    auto quad = [&](Rect r, glm::vec4 color, std::uint32_t texture = 0) {
        SpriteDraw d;
        d.transform.position = {r.x + r.w * .5F, static_cast<float>(height) - r.y - r.h * .5F};
        d.transform.size = {r.w, r.h};
        d.sprite.color = color;
        d.sprite.texture = texture;
        d.sprite.layer = layer++;
        draws.push_back(d);
    };
    for (const auto &item : items) {
        const auto &u = item.data;
        const auto kind = u.text("kind");
        if (kind == "Canvas")
            continue;
        const float scale = item.scale;
        auto color = u.contains("color") ? jsonVec4(u.at("color")) : glm::vec4(.16F, .22F, .3F, 1);
        if (!u.flag("interactable", true))
            color *= glm::vec4(.5F, .5F, .5F, 1);
        else if (item.id == hovered_)
            color = glm::min(color + glm::vec4(.12F, .12F, .12F, 0), glm::vec4(1));
        if (kind != "Label" && kind != "Layout")
            quad(item.rect, color, kind == "Image" ? assets.texture(u.text("texture")) : 0);
        if (kind == "Slider") {
            auto fill = item.rect;
            fill.w *= static_cast<float>(u.num("value", .5));
            if (fill.w > 0)
                quad(fill, {.2F, .8F, .6F, 1});
        }
        if (focus_ == item.id) {
            auto line = item.rect;
            line.h = 2;
            quad(line, {1, .8F, .3F, 1});
        }
        if (u.contains("text"))
            assets.font(u.text("font", "bitmap"))
                .append(draws, u.at("text").string(),
                        {item.rect.x + 6 * scale, static_cast<float>(height) - item.rect.y - 6 * scale},
                        static_cast<float>(u.num("fontScale", 2)) * scale,
                        u.contains("textColor") ? jsonVec4(u.at("textColor")) : glm::vec4(1), layer++);
    }
    renderer.draw(camera, draws);
}
} // namespace twodog
