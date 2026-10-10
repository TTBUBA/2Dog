#include <twodog/gameplay/Physics.hpp>
#include <box2d/box2d.h>
#include <atomic>
#include <cmath>
#include <map>
#include <stdexcept>

namespace twodog {
namespace {
std::atomic<std::uint64_t> worldSequence{1};
void finite(float v,float limit=100000.0F) {if(!std::isfinite(v)||std::abs(v)>limit) throw std::invalid_argument("Invalid physics number");}
void finite(glm::vec2 v) {finite(v.x);finite(v.y);}
b2Vec2 native(glm::vec2 v) {return {v.x,v.y};}
glm::vec2 vec(b2Vec2 v) {return {v.x,v.y};}
BodyPose current(b2BodyId id) {return {vec(b2Body_GetPosition(id)),b2Rot_GetAngle(b2Body_GetRotation(id))};}
}
namespace {
b2Polygon colliderGeometry(const BodyConfig& c) {
    finite(c.localPosition);finite(c.localAngle);finite(c.density);finite(c.friction);finite(c.restitution);
    if(c.density<=0||c.friction<0||c.friction>1||c.restitution<0||c.restitution>1) throw std::invalid_argument("Invalid collider material");
    b2Polygon polygon{};
    if(c.collider==ColliderType::Box) {
        finite(c.halfSize);if(c.halfSize.x<0.005F||c.halfSize.y<0.005F) throw std::invalid_argument("Box too small");
        polygon=b2MakeOffsetBox(c.halfSize.x,c.halfSize.y,native(c.localPosition),b2MakeRot(c.localAngle));
    } else if(c.collider==ColliderType::Circle) {
        finite(c.radius);if(c.radius<0.005F) throw std::invalid_argument("Circle too small");
    } else if(c.collider==ColliderType::Polygon) {
        if(c.vertices.size()<3||c.vertices.size()>8) throw std::invalid_argument("Convex polygon needs 3..8 vertices");
        b2Vec2 points[8]{};
        for(std::size_t i=0;i<c.vertices.size();++i) {finite(c.vertices[i]);points[i]=b2Add(native(c.localPosition),b2RotateVector(b2MakeRot(c.localAngle),native(c.vertices[i])));}
        const auto hull=b2ComputeHull(points,static_cast<int>(c.vertices.size()));
        if(hull.count!=static_cast<int>(c.vertices.size())||!b2ValidateHull(&hull)) throw std::invalid_argument("Polygon must be strictly convex and non-degenerate");
        polygon=b2MakePolygon(&hull,0);
    } else throw std::invalid_argument("Invalid collider type");
    return polygon;
}
}
void Physics::validateCollider(const BodyConfig& c) { static_cast<void>(colliderGeometry(c)); }
void Physics::validateBody(const BodyConfig& c) {
    finite(c.position); finite(c.velocity); finite(c.angle);
    if(c.type!=BodyType::Static && c.type!=BodyType::Dynamic && c.type!=BodyType::Kinematic) throw std::invalid_argument("Invalid body type");
    validateCollider(c);
}
struct Physics::Impl {
    struct Body {b2BodyId id{};BodyHandle handle;BodyPose previous;};
    b2WorldId id{};
    std::uint64_t token=worldSequence.fetch_add(1),nextBody=1,nextJoint=1;
    std::map<std::uint64_t,Body> bodies;
    std::map<std::uint64_t,b2JointId> joints;
    std::vector<PhysicsEvent> events;
    explicit Impl(glm::vec2 gravity) {
        finite(gravity);auto def=b2DefaultWorldDef();def.gravity=native(gravity);def.workerCount=0;
        id=b2CreateWorld(&def);
        if(!b2World_IsValid(id)) throw std::runtime_error("Box2D world creation failed");
    }
    ~Impl() {if(b2World_IsValid(id)) b2DestroyWorld(id);}
    const Body& get(BodyHandle h) const {
        const auto it=bodies.find(h.value);
        if(h.world!=token || it==bodies.end()) throw std::invalid_argument("Expired or foreign physics handle");
        return it->second;
    }
    void event(PhysicsEvent::Kind kind,b2ShapeId a,b2ShapeId b) {
        if(!b2Shape_IsValid(a)||!b2Shape_IsValid(b)) return;
        const auto* pa=static_cast<Body*>(b2Body_GetUserData(b2Shape_GetBody(a)));
        const auto* pb=static_cast<Body*>(b2Body_GetUserData(b2Shape_GetBody(b)));
        if(pa&&pb) events.push_back({kind,pa->handle,pb->handle});
    }
};
Physics::Physics(glm::vec2 gravity):impl_(std::make_unique<Impl>(gravity)){}
Physics::~Physics()=default;
BodyHandle Physics::create(const BodyConfig& c) {
    validateBody(c);
    auto def=b2DefaultBodyDef();
    switch(c.type) {case BodyType::Static:def.type=b2_staticBody;break;case BodyType::Kinematic:def.type=b2_kinematicBody;break;case BodyType::Dynamic:def.type=b2_dynamicBody;break;default:throw std::invalid_argument("Invalid body type");}
    def.position=native(c.position);def.rotation=b2MakeRot(c.angle);def.linearVelocity=native(c.velocity);
    def.fixedRotation=c.fixedRotation;def.isBullet=c.bullet;
    const BodyHandle handle{impl_->nextBody++,impl_->token};
    auto [it,inserted]=impl_->bodies.emplace(handle.value,Impl::Body{{},handle,{c.position,c.angle}});
    static_cast<void>(inserted);def.userData=&it->second;
    it->second.id=b2CreateBody(impl_->id,&def);
    try { addCollider(handle,c); }
    catch(...) { b2DestroyBody(it->second.id);impl_->bodies.erase(it);throw; }
    return handle;
}
void Physics::addCollider(BodyHandle h,const BodyConfig& c) {
    const auto id=impl_->get(h).id;
    const auto polygon=colliderGeometry(c);
    auto shape=b2DefaultShapeDef();shape.density=c.density;shape.material.friction=c.friction;shape.material.restitution=c.restitution;
    shape.isSensor=c.sensor;shape.enableSensorEvents=true;shape.enableContactEvents=true;
    shape.filter.categoryBits=c.category;shape.filter.maskBits=c.mask;
    if(c.collider==ColliderType::Circle) {const b2Circle circle{native(c.localPosition),c.radius};b2CreateCircleShape(id,&shape,&circle);}
    else b2CreatePolygonShape(id,&shape,&polygon);
}
void Physics::setPose(BodyHandle h,BodyPose pose) {
    finite(pose.position);finite(pose.angle);auto& b=impl_->bodies.at(impl_->get(h).handle.value);
    b2Body_SetTransform(b.id,native(pose.position),b2MakeRot(pose.angle));b.previous=pose;
}
JointHandle Physics::joint(BodyHandle a,BodyHandle b,const JointConfig& c) {
    if(a==b) throw std::invalid_argument("A joint needs two different bodies");
    finite(c.anchor);finite(c.axis);finite(c.lower);finite(c.upper);finite(c.speed);finite(c.maxForce);
    if(c.lower>c.upper||c.maxForce<0) throw std::invalid_argument("Invalid joint limits or force");
    const auto ia=impl_->get(a).id,ib=impl_->get(b).id;
    const auto aa=b2Body_GetLocalPoint(ia,native(c.anchor)),ab=b2Body_GetLocalPoint(ib,native(c.anchor));
    const auto angle=current(ib).angle-current(ia).angle;b2JointId id{};
    switch(c.type) {
    case JointType::Revolute: {
        auto d=b2DefaultRevoluteJointDef();d.bodyIdA=ia;d.bodyIdB=ib;d.localAnchorA=aa;d.localAnchorB=ab;d.referenceAngle=angle;
        d.enableLimit=c.limit;d.lowerAngle=c.lower;d.upperAngle=c.upper;d.enableMotor=c.motor;d.motorSpeed=c.speed;d.maxMotorTorque=c.maxForce;
        id=b2CreateRevoluteJoint(impl_->id,&d);break;
    }
    case JointType::Prismatic: {
        const auto length=glm::length(c.axis);if(length<0.001F) throw std::invalid_argument("Joint axis is zero");
        auto d=b2DefaultPrismaticJointDef();d.bodyIdA=ia;d.bodyIdB=ib;d.localAnchorA=aa;d.localAnchorB=ab;d.referenceAngle=angle;
        d.localAxisA=b2Body_GetLocalVector(ia,native(c.axis/length));d.enableLimit=c.limit;d.lowerTranslation=c.lower;d.upperTranslation=c.upper;
        d.enableMotor=c.motor;d.motorSpeed=c.speed;d.maxMotorForce=c.maxForce;id=b2CreatePrismaticJoint(impl_->id,&d);break;
    }
    case JointType::Weld: {
        auto d=b2DefaultWeldJointDef();d.bodyIdA=ia;d.bodyIdB=ib;d.localAnchorA=aa;d.localAnchorB=ab;d.referenceAngle=angle;
        id=b2CreateWeldJoint(impl_->id,&d);break;
    }
    default:throw std::invalid_argument("Unknown joint type");
    }
    const auto token=impl_->nextJoint++;impl_->joints.emplace(token,id);return {token,impl_->token};
}
void Physics::destroy(BodyHandle h) {
    const auto id=impl_->get(h).id;b2DestroyBody(id);impl_->bodies.erase(h.value);
    std::erase_if(impl_->joints,[](const auto& entry){return !b2Joint_IsValid(entry.second);});
}
bool Physics::alive(BodyHandle h) const noexcept {return h.world==impl_->token&&impl_->bodies.contains(h.value);}
void Physics::step(float dt) {
    finite(dt);if(dt<=0||dt>0.1F) throw std::invalid_argument("Physics step must be in (0, 0.1]");
    for(auto& [key,b]:impl_->bodies) {static_cast<void>(key);b.previous=current(b.id);}
    impl_->events.clear();b2World_Step(impl_->id,dt,4);
    const auto contacts=b2World_GetContactEvents(impl_->id);
    for(int i=0;i<contacts.beginCount;++i) impl_->event(PhysicsEvent::Kind::ContactBegin,contacts.beginEvents[i].shapeIdA,contacts.beginEvents[i].shapeIdB);
    for(int i=0;i<contacts.endCount;++i) impl_->event(PhysicsEvent::Kind::ContactEnd,contacts.endEvents[i].shapeIdA,contacts.endEvents[i].shapeIdB);
    const auto sensors=b2World_GetSensorEvents(impl_->id);
    for(int i=0;i<sensors.beginCount;++i) impl_->event(PhysicsEvent::Kind::TriggerBegin,sensors.beginEvents[i].sensorShapeId,sensors.beginEvents[i].visitorShapeId);
    for(int i=0;i<sensors.endCount;++i) impl_->event(PhysicsEvent::Kind::TriggerEnd,sensors.endEvents[i].sensorShapeId,sensors.endEvents[i].visitorShapeId);
}
BodyPose Physics::pose(BodyHandle h,float alpha) const {
    finite(alpha);if(alpha<0||alpha>1) throw std::invalid_argument("Interpolation alpha must be in [0,1]");
    const auto& b=impl_->get(h);const auto now=current(b.id);
    const float delta=std::remainder(now.angle-b.previous.angle,6.28318530718F);
    return {glm::mix(b.previous.position,now.position,alpha),b.previous.angle+delta*alpha};
}
glm::vec2 Physics::velocity(BodyHandle h) const {return vec(b2Body_GetLinearVelocity(impl_->get(h).id));}
void Physics::setVelocity(BodyHandle h,glm::vec2 v) {finite(v);b2Body_SetLinearVelocity(impl_->get(h).id,native(v));}
void Physics::impulse(BodyHandle h,glm::vec2 v) {finite(v);b2Body_ApplyLinearImpulseToCenter(impl_->get(h).id,native(v),true);}
void Physics::force(BodyHandle h,glm::vec2 v) {finite(v);b2Body_ApplyForceToCenter(impl_->get(h).id,native(v),true);}
std::optional<RayHit> Physics::raycast(glm::vec2 origin,glm::vec2 translation,std::uint64_t mask,BodyHandle ignore) const {
    finite(origin);finite(translation);if(ignore.value) static_cast<void>(impl_->get(ignore));
    auto filter=b2DefaultQueryFilter();filter.maskBits=mask;
    struct Query {BodyHandle ignore;std::optional<RayHit> result;};Query query{ignore,{}};
    const auto callback=[](b2ShapeId shape,b2Vec2 point,b2Vec2 normal,float fraction,void* context)->float {
        auto& q=*static_cast<Query*>(context);
        const auto* body=static_cast<Impl::Body*>(b2Body_GetUserData(b2Shape_GetBody(shape)));
        if(!body||body->handle==q.ignore||b2Shape_IsSensor(shape)) return -1.0F;
        q.result=RayHit{body->handle,vec(point),vec(normal),fraction};return fraction;
    };
    b2World_CastRay(impl_->id,native(origin),native(translation),filter,callback,&query);return query.result;
}
JointHandle Physics::distanceJoint(BodyHandle a,BodyHandle b,float length,float hertz,float damping) {
    finite(length);finite(hertz);finite(damping);
    if(a==b||length<0.01F||hertz<0||hertz>60||damping<0||damping>10) throw std::invalid_argument("Invalid distance joint");
    auto d=b2DefaultDistanceJointDef();d.bodyIdA=impl_->get(a).id;d.bodyIdB=impl_->get(b).id;
    d.length=length;d.enableSpring=hertz>0;d.hertz=hertz;d.dampingRatio=damping;
    const auto id=b2CreateDistanceJoint(impl_->id,&d);const auto token=impl_->nextJoint++;
    impl_->joints.emplace(token,id);return {token,impl_->token};
}
void Physics::destroyJoint(JointHandle h) {
    const auto it=impl_->joints.find(h.value);
    if(h.world!=impl_->token||it==impl_->joints.end()||!b2Joint_IsValid(it->second)) throw std::invalid_argument("Expired or foreign joint");
    b2DestroyJoint(it->second);impl_->joints.erase(it);
}
const std::vector<PhysicsEvent>& Physics::events() const noexcept {return impl_->events;}
std::size_t Physics::bodyCount() const noexcept {return impl_->bodies.size();}
}
