#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <compare>
#include <memory>
#include <optional>
#include <vector>

namespace twodog {
struct BodyHandle {std::uint64_t value=0,world=0;auto operator<=>(const BodyHandle&) const = default;};
struct JointHandle {std::uint64_t value=0,world=0;};
enum class BodyType {Static, Kinematic, Dynamic};
enum class ColliderType {Box, Circle, Polygon};
struct BodyConfig {
    BodyType type=BodyType::Dynamic;
    ColliderType collider=ColliderType::Box;
    glm::vec2 position{0},halfSize{0.5F},velocity{0},localPosition{0};
    float localAngle=0;
    float angle=0,radius=0.5F,density=1,friction=0.4F,restitution=0.2F;
    bool sensor=false,fixedRotation=false,bullet=false;
    std::uint64_t category=1,mask=~std::uint64_t{0};
    std::vector<glm::vec2> vertices;
};
enum class JointType { Revolute, Prismatic, Weld };
struct JointConfig {
    JointType type=JointType::Revolute;
    glm::vec2 anchor{0},axis{1,0};
    bool limit=false,motor=false;
    float lower=0,upper=0,speed=0,maxForce=0;
};
struct BodyPose {glm::vec2 position{0};float angle=0;};
struct PhysicsEvent {
    enum class Kind {ContactBegin,ContactEnd,TriggerBegin,TriggerEnd};
    Kind kind; BodyHandle a,b;
};
struct RayHit {BodyHandle body;glm::vec2 point,normal;float fraction;};
class Physics final {
public:
    static void validateCollider(const BodyConfig& config);
    static void validateBody(const BodyConfig& config);
    explicit Physics(glm::vec2 gravity={0,-9.81F});
    ~Physics();
    Physics(const Physics&)=delete;
    Physics& operator=(const Physics&)=delete;
    BodyHandle create(const BodyConfig& config);
    void destroy(BodyHandle body);
    void addCollider(BodyHandle body,const BodyConfig& collider);
    void setPose(BodyHandle body,BodyPose pose);
    bool alive(BodyHandle body) const noexcept;
    void step(float dt);
    BodyPose pose(BodyHandle body,float alpha=1) const;
    glm::vec2 velocity(BodyHandle body) const;
    void setVelocity(BodyHandle body,glm::vec2 velocity);
    void impulse(BodyHandle body,glm::vec2 value);
    void force(BodyHandle body,glm::vec2 value);
    std::optional<RayHit> raycast(glm::vec2 origin,glm::vec2 translation,std::uint64_t mask=~std::uint64_t{0},BodyHandle ignore={}) const;
    JointHandle distanceJoint(BodyHandle a,BodyHandle b,float length,float hertz=0,float damping=0.7F);
    JointHandle joint(BodyHandle a,BodyHandle b,const JointConfig& config);
    void destroyJoint(JointHandle joint);
    const std::vector<PhysicsEvent>& events() const noexcept;
    std::size_t bodyCount() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
