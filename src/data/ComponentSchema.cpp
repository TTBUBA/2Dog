#include <twodog/data/ComponentSchema.hpp>
#include <cmath>
#include <stdexcept>
namespace twodog {
namespace {
void require(bool value, const char *message) {
    if (!value)
        throw std::invalid_argument(message);
}
void color(const Json &value) {
    const auto c = jsonVec4(value);
    for (int i = 0; i < 4; ++i)
        require(c[i] >= 0 && c[i] <= 1, "Color/UV must be in 0..1");
}
void sprite(const Json &s) {
    static_cast<void>(s.object());
    static_cast<void>(s.text("texture"));
    for (const char *key : {"color", "uv"})
        if (s.contains(key))
            color(s.at(key));
    if (s.contains("parallax"))
        static_cast<void>(jsonVec2(s.at("parallax")));
    if (s.contains("layer"))
        static_cast<void>(s.at("layer").integer());
    require(std::isfinite(s.num("depth")) && std::abs(s.num("depth")) <= 100000, "Invalid sprite depth");
}
void text(const Json &t) {
    static_cast<void>(t.at("value").string());
    static_cast<void>(t.text("font", "bitmap"));
    require(t.num("scale", .03) > 0 && t.num("scale", .03) <= 10, "Text scale must be in (0,10]");
    if (t.contains("color"))
        color(t.at("color"));
}
void animation(const Json &animator) {
    const auto initial = animator.at("state").string();
    require(animator.at("states").contains(initial), "Unknown initial animation state");
    for (const auto &[name, state] : animator.at("states").object()) {
        static_cast<void>(name);
        require(state.num("fps", 8) > 0 && state.num("fps", 8) <= 240, "Animation fps must be 0..240");
        require(!state.at("frames").array().empty(), "Animation needs frames");
        for (const auto &frame : state.at("frames").array())
            color(frame);
        if (state.contains("next"))
            require(animator.at("states").contains(state.at("next").string()), "Unknown next animation state");
    }
}
void object(const Json &value) { static_cast<void>(value.object()); }
} // namespace
BodyConfig ComponentSchema::collider(const Json &j) {
    BodyConfig c;
    const auto shape = j.text("shape", "box");
    if (shape == "box")
        c.collider = ColliderType::Box;
    else if (shape == "circle")
        c.collider = ColliderType::Circle;
    else if (shape == "polygon") {
        c.collider = ColliderType::Polygon;
        for (const auto &v : j.at("vertices").array())
            c.vertices.push_back(jsonVec2(v));
    } else
        throw std::invalid_argument("Unknown collider shape");
    if (j.contains("halfSize"))
        c.halfSize = jsonVec2(j.at("halfSize"));
    if (j.contains("offset"))
        c.localPosition = jsonVec2(j.at("offset"));
    c.localAngle = static_cast<float>(j.num("angle"));
    c.radius = static_cast<float>(j.num("radius", .5));
    c.density = static_cast<float>(j.num("density", 1));
    c.friction = static_cast<float>(j.num("friction", .4));
    c.restitution = static_cast<float>(j.num("restitution", .2));
    c.sensor = j.flag("sensor");
    if (j.contains("category")) {
        const auto n = j.at("category").integer();
        if (n < 1)
            throw std::invalid_argument("Invalid collision category");
        c.category = static_cast<std::uint64_t>(n);
    }
    if (j.contains("mask")) {
        const auto n = j.at("mask").integer();
        if (n < 0)
            throw std::invalid_argument("Invalid collision mask");
        c.mask = static_cast<std::uint64_t>(n);
    }
    Physics::validateCollider(c);
    return c;
}

BodyConfig ComponentSchema::body(const Json &b, const Transform2D &transform) {
    const auto &colliders = b.at("colliders").array();
    require(!colliders.empty(), "Body needs a collider");
    auto c = collider(colliders.front());
    const auto type = b.text("type", "dynamic");
    if (type == "static")
        c.type = BodyType::Static;
    else if (type == "dynamic")
        c.type = BodyType::Dynamic;
    else if (type == "kinematic")
        c.type = BodyType::Kinematic;
    else
        throw std::invalid_argument("Unknown body type");
    c.position = transform.position;
    c.angle = transform.rotation;
    c.fixedRotation = b.flag("fixedRotation");
    c.bullet = b.flag("bullet");
    if (b.contains("velocity"))
        c.velocity = jsonVec2(b.at("velocity"));
    Physics::validateBody(c);
    for (std::size_t i = 1; i < colliders.size(); ++i)
        static_cast<void>(collider(colliders[i]));
    return c;
}
Json ComponentSchema::colliderDefaults(std::string_view shape) {
    require(shape == "box" || shape == "circle" || shape == "polygon", "Unknown collider shape");
    auto value = Json::parse(
        R"({"shape":"box","halfSize":[0.5,0.5],"radius":0.5,"offset":[0,0],"angle":0,"density":1,"friction":0.4,"restitution":0.2,"sensor":false,"vertices":[[-0.5,-0.5],[0.5,-0.5],[0,0.5]]})");
    value["shape"] = std::string(shape);
    return value;
}
void ComponentSchema::validateUI(const Json &u) {
    const auto kind = u.text("kind");
    require(kind == "Panel" || kind == "Label" || kind == "Image" || kind == "Button" || kind == "Slider" ||
                kind == "Layout" || kind == "Canvas",
            "Unknown UI component kind");
    if (kind == "Canvas") {
        const auto resolution = jsonVec2(u.at("referenceResolution"));
        require(resolution.x > 0 && resolution.y > 0, "Canvas reference resolution must be positive");
        require(u.num("match", .5) >= 0 && u.num("match", .5) <= 1, "Canvas match must be 0..1");
        require(u.text("parent").empty(), "Canvas must be a root UI object");
    }
    for (const char *key : {"anchorMin", "anchorMax", "pivot", "offset", "size"})
        static_cast<void>(jsonVec2(u.at(key)));
    const auto a = jsonVec2(u.at("anchorMin")), b = jsonVec2(u.at("anchorMax")), p = jsonVec2(u.at("pivot"));
    require(a.x >= 0 && a.y >= 0 && b.x <= 1 && b.y <= 1 && a.x <= b.x && a.y <= b.y, "Invalid UI anchors");
    require(p.x >= 0 && p.y >= 0 && p.x <= 1 && p.y <= 1, "UI pivot must be 0..1");
    for (const char *key : {"color", "textColor"})
        if (u.contains(key)) {
            const auto c = jsonVec4(u.at(key));
            for (int i = 0; i < 4; ++i)
                require(c[i] >= 0 && c[i] <= 1, "UI color must be 0..1");
        }
    require(u.num("fontScale", 2) > 0 && u.num("fontScale", 2) <= 10, "UI fontScale must be (0,10]");
    require(u.num("value", .5) >= 0 && u.num("value", .5) <= 1, "Slider value must be 0..1");
    require(u.num("padding", 8) >= 0 && u.num("spacing", 8) >= 0, "UI spacing must be positive");
    require(u.text("direction", "vertical") == "vertical" || u.text("direction") == "horizontal",
            "Unknown layout direction");
    static_cast<void>(u.text("target"));
    static_cast<void>(u.text("topic", "ui"));
    static_cast<void>(u.num("order"));
    static_cast<void>(u.flag("blockPointer", true));
    static_cast<void>(u.text("parent"));
    static_cast<void>(u.text("text"));
    static_cast<void>(u.flag("visible", true));
    static_cast<void>(u.flag("interactable", true));
}
Json ComponentSchema::uiDefaults(const char *kind) {
    Json result = Json::Object{{"kind", kind},
                               {"parent", ""},
                               {"anchorMin", Json::Array{0, 0}},
                               {"anchorMax", Json::Array{0, 0}},
                               {"pivot", Json::Array{0, 0}},
                               {"offset", Json::Array{24, 120}},
                               {"size", Json::Array{240, 48}},
                               {"visible", true},
                               {"interactable", true},
                               {"blockPointer", true},
                               {"text", kind},
                               {"font", "bitmap"},
                               {"fontScale", 2},
                               {"color", Json::Array{.12, .2, .28, 1}},
                               {"textColor", Json::Array{1, 1, 1, 1}},
                               {"texture", ""},
                               {"value", .5},
                               {"topic", "ui"},
                               {"target", ""},
                               {"direction", "vertical"},
                               {"padding", 8},
                               {"spacing", 8},
                               {"order", 0}};
    if (std::string(kind) == "Canvas") {
        result["anchorMax"] = Json::Array{1, 1};
        result["offset"] = Json::Array{0, 0};
        result["size"] = Json::Array{0, 0};
        result["referenceResolution"] = Json::Array{1280, 720};
        result["match"] = .5;
        result["text"] = "";
        result["blockPointer"] = false;
        result["interactable"] = false;
    }
    return result;
}

std::span<const ComponentDescriptor> ComponentSchema::components() {
    static const ComponentDescriptor entries[] = {
        {"sprite", "SpriteRenderer",
         +[] {
             return Json::parse(
                 R"({"texture":"","color":[1,1,1,1],"uv":[0,0,1,1],"layer":0,"depth":0,"parallax":[1,1]})");
         },
         sprite},
        {"body", "Rigidbody2D + BoxCollider",
         +[]() -> Json {
             return Json::Object{{"type", "dynamic"},
                                 {"fixedRotation", false},
                                 {"bullet", false},
                                 {"velocity", Json::Array{0, 0}},
                                 {"colliders", Json::Array{colliderDefaults("box")}}};
         },
         +[](const Json &b) { static_cast<void>(body(b, Transform2D{})); }},
        {"animation", "Animator",
         +[] {
             return Json::parse(R"({"state":"idle","states":{"idle":{"fps":8,"loop":true,"frames":[[0,0,1,1]]}}})");
         },
         animation},
        {"text", "World Text",
         +[] { return Json::parse(R"({"font":"bitmap","value":"TEXT","scale":0.03,"color":[1,1,1,1]})"); }, text},
        {"ui", "UI", +[] { return uiDefaults("Label"); }, validateUI},
        {"properties", "Properties", +[]() -> Json { return Json::Object{}; }, object}};
    return entries;
}
std::string ComponentSchema::label(std::string_view key) {
    for (const auto &entry : components())
        if (key == entry.key)
            return entry.label;
    return std::string(key);
}
Json ComponentSchema::defaults(std::string_view key) {
    for (const auto &entry : components())
        if (key == entry.key)
            return entry.defaults();
    throw std::invalid_argument("Unknown component");
}
void ComponentSchema::validateEntity(const Json &entity) {
    for (const auto &entry : components())
        if (entity.contains(entry.key))
            entry.validate(entity.at(entry.key));
}
} // namespace twodog
