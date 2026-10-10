#include <twodog/data/ComponentSchema.hpp>
#include <twodog/editor/SceneDocument.hpp>
#include <twodog/gameplay/World2D.hpp>
#include <iostream>
#include <stdexcept>
using namespace twodog;
namespace {
void expect(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void rejects(F fn) {
    bool caught = false;
    try {
        fn();
    } catch (const std::exception &) {
        caught = true;
    }
    expect(caught, "Expected rejection");
}
Json entity() {
    return Json::Object{{"id", "object"}, {"name", "Object"}, {"transform", writeTransform(Transform2D{})}};
}
void run(const std::string &test, const std::filesystem::path &root) {
    if (test == "defaults") {
        for (const auto &descriptor : ComponentSchema::components()) {
            auto e = entity();
            e[descriptor.key] = descriptor.defaults();
            Project::validateEntity(e);
        }
        for (const char *kind : {"Canvas", "Panel", "Label", "Image", "Button", "Slider", "Layout"})
            ComponentSchema::validateUI(ComponentSchema::uiDefaults(kind));
        for (const char *shape : {"box", "circle", "polygon"})
            static_cast<void>(ComponentSchema::collider(ComponentSchema::colliderDefaults(shape)));
        rejects([] { ComponentSchema::defaults("unknown"); });
        return;
    }
    if (test == "collider_geometry") {
        auto box = ComponentSchema::colliderDefaults("box");
        box["halfSize"] = Json::Array{0, .5};
        rejects([&] { ComponentSchema::collider(box); });
        auto circle = ComponentSchema::colliderDefaults("circle");
        circle["radius"] = -1;
        rejects([&] { ComponentSchema::collider(circle); });
        auto polygon = ComponentSchema::colliderDefaults("polygon");
        polygon["vertices"] = Json::Array{Json::Array{0, 0}, Json::Array{1, 0}, Json::Array{2, 0}};
        rejects([&] { ComponentSchema::collider(polygon); });
        Physics physics;
        BodyConfig invalid;
        invalid.radius = 0;
        invalid.collider = ColliderType::Circle;
        rejects([&] { Physics::validateCollider(invalid); });
        rejects([&] { physics.create(invalid); });
        expect(physics.bodyCount() == 0, "Invalid body was published");
        return;
    }
    if (test == "material_and_body") {
        auto b = ComponentSchema::defaults("body");
        b["colliders"].array().front()["friction"] = 2;
        rejects([&] { ComponentSchema::body(b, Transform2D{}); });
        b = ComponentSchema::defaults("body");
        b["type"] = "unknown";
        rejects([&] { ComponentSchema::body(b, Transform2D{}); });
        b = ComponentSchema::defaults("body");
        b["colliders"] = Json::Array{};
        rejects([&] { ComponentSchema::body(b, Transform2D{}); });
        return;
    }
    if (test == "visual_rules") {
        auto e = entity();
        e["sprite"] = ComponentSchema::defaults("sprite");
        e["sprite"]["color"] = Json::Array{2, 0, 0, 1};
        rejects([&] { Project::validateEntity(e); });
        e = entity();
        e["animation"] = ComponentSchema::defaults("animation");
        e["animation"]["state"] = "missing";
        rejects([&] { Project::validateEntity(e); });
        e = entity();
        e["text"] = ComponentSchema::defaults("text");
        e["text"]["scale"] = 0;
        rejects([&] { Project::validateEntity(e); });
        e = entity();
        e["ui"] = ComponentSchema::uiDefaults("Canvas");
        e["ui"]["match"] = 2;
        rejects([&] { Project::validateEntity(e); });
        return;
    }
    if (test == "shared_transaction") {
        Project project(root);
        auto doc = project.scene("scene.demo");
        SceneDocument editor;
        editor.load(doc);
        auto bad = doc;
        auto &first = bad["entities"].array().front();
        first["body"] = ComponentSchema::defaults("body");
        first["body"]["colliders"].array().front()["density"] = -1;
        rejects([&] { Project::validateScene(bad); });
        rejects([&] { editor.commit(bad); });
        rejects([&] { World2D world(project, bad); });
        expect(editor.document().dump() == doc.dump() && !editor.canUndo() && !editor.dirty(),
               "Editor accepted invalid schema");
        return;
    }
    throw std::runtime_error("Unknown test");
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 3)
            throw std::runtime_error("Expected case and sample root");
        run(argv[1], argv[2]);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
