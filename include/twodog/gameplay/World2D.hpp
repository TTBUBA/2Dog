#pragma once
#include <twodog/data/Project.hpp>
#include <twodog/gameplay/Physics.hpp>
#include <memory>

namespace twodog {
struct PersistentEntity {
    Json document;
    Entity handle;
    BodyHandle body;
};
// Native persistent scene state. A new World2D is fully constructed before swapping it into play.
class World2D final {
public:
    World2D(const Project& project,Json document);
    PersistentEntity& get(std::string_view id);
    const PersistentEntity& get(std::string_view id) const;
    bool alive(std::string_view id) const;
    std::string create(Json entity);
    void destroy(std::string_view id);
    void addCollider(std::string_view id,const Json& collider);
    std::string createJoint(Json joint);
    void destroyJoint(std::string_view id);
    std::string find(std::string_view name) const;
    Transform2D transform(std::string_view id,float alpha=1) const;
    void transform(std::string_view id,const Transform2D& value);
    Json snapshot() const;
    Json bindings() const;
    std::string entityFor(BodyHandle body) const;
    const std::map<std::string,PersistentEntity,std::less<>>& entities() const { return entities_; }
    Scene scene;
    Physics physics;
private:
    const Project& project_;
    Json document_;
    std::map<std::string,JointHandle,std::less<>> joints_;
    std::map<std::string,PersistentEntity,std::less<>> entities_;
};
}
