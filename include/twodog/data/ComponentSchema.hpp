#pragma once
#include <twodog/data/Project.hpp>
#include <twodog/gameplay/Physics.hpp>
#include <span>
namespace twodog {
struct ComponentDescriptor {
    const char *key;
    const char *label;
    Json (*defaults)();
    void (*validate)(const Json &);
};
// Data-only schema shared by project loading, runtime and editor authoring.
class ComponentSchema final {
  public:
    static std::span<const ComponentDescriptor> components();
    static std::string label(std::string_view key);
    static Json defaults(std::string_view key);
    static void validateEntity(const Json &entity);
    static BodyConfig collider(const Json &value);
    static BodyConfig body(const Json &value, const Transform2D &transform);
    static Json colliderDefaults(std::string_view shape);
    static Json uiDefaults(const char *kind);
    static void validateUI(const Json &value);
};
} // namespace twodog
