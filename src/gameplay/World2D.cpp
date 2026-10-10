#include <twodog/data/ComponentSchema.hpp>
#include <twodog/gameplay/World2D.hpp>
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace twodog {
    World2D::World2D(const Project& p,Json d):physics(d.contains("gravity")?jsonVec2(d.at("gravity")):glm::vec2(0,-9.81F)),project_(p),document_(std::move(d)) {
        Project::validateScene(document_);
        for(const auto& e:document_.at("entities").array()) create(e);
        const auto initial=document_.contains("joints")?document_.at("joints").array():Json::Array{};
        document_["joints"]=Json::Array{};
        for(const auto& j:initial) createJoint(j);
    }
    void World2D::addCollider(std::string_view id,const Json& value) {
        auto& e=get(id);
        physics.addCollider(e.body,ComponentSchema::collider(value));
        e.document["body"]["colliders"].array().push_back(value);
    }
    std::string World2D::createJoint(Json j) {
        JointHandle handle{};
        const auto a=get(j.at("a").string()).body,b=get(j.at("b").string()).body;
        const auto type=j.at("type").string();
        if(type=="distance") handle=physics.distanceJoint(a,b,static_cast<float>(j.at("length").number()),static_cast<float>(j.num("hertz")),static_cast<float>(j.num("damping",.7)));
        else {
            JointConfig c;
            if(type=="revolute") c.type=JointType::Revolute;
            else if(type=="prismatic") c.type=JointType::Prismatic;
            else if(type=="weld") c.type=JointType::Weld;
            else throw std::invalid_argument("Unknown joint type");
            c.anchor=jsonVec2(j.at("anchor"));
            if(j.contains("axis")) c.axis=jsonVec2(j.at("axis"));
            c.limit=j.flag("limit");
            c.motor=j.flag("motor");
            c.lower=static_cast<float>(j.num("lower"));
            c.upper=static_cast<float>(j.num("upper"));
            c.speed=static_cast<float>(j.num("speed"));
            c.maxForce=static_cast<float>(j.num("maxForce"));
            handle=physics.joint(a,b,c);
        }

        const auto id=newPersistentId();
        j["id"]=id;
        joints_.emplace(id,handle);
        document_["joints"].array().push_back(std::move(j));
        return id;
    }
    void World2D::destroyJoint(std::string_view id) {
        const auto it=joints_.find(id);
        if(it==joints_.end()) throw std::invalid_argument("Expired joint");
        physics.destroyJoint(it->second);
        joints_.erase(it);
        std::erase_if(document_["joints"].array(),[id](const Json& j){return j.at("id").string()==id;});
    }
    std::string World2D::create(Json e) {
        Project::validateEntity(e);
        const auto id=e.at("id").string();
        if(entities_.contains(id)) throw std::invalid_argument("Duplicate entity ID");
        if(entities_.size()>=10000) throw std::invalid_argument("Entity limit reached");
        const auto t=readTransform(e.at("transform"));
        BodyHandle body{};
        if(e.contains("sprite")) {const auto texture=e.at("sprite").text("texture");
            if(!texture.empty()) static_cast<void>(project_.asset(texture,"texture"));
        }
        if(e.contains("body")) {
            const auto& b=e.at("body");
            const auto& colliders=b.at("colliders").array();
            if(colliders.empty()) throw std::invalid_argument("Body needs a collider");
            auto c=ComponentSchema::body(b,t);
            body=physics.create(c);
            try {for(std::size_t i=1;i<colliders.size();++i) physics.addCollider(body,ComponentSchema::collider(colliders[i]));
            }
            catch(...) {physics.destroy(body);
                throw;
            }
        }
        const auto handle=scene.create();
        scene.get(handle).transform=t;
        try {entities_.emplace(id,PersistentEntity{std::move(e),handle,body});
        }
        catch(...) {scene.destroy(handle);
            if(body.value) physics.destroy(body);
            throw;
        }
        return id;
    }
    PersistentEntity& World2D::get(std::string_view id) {return const_cast<PersistentEntity&>(std::as_const(*this).get(id));
    }
    const PersistentEntity& World2D::get(std::string_view id) const {const auto it=entities_.find(id);
        if(it==entities_.end()) throw std::invalid_argument("Expired entity: "+std::string(id));
        return it->second;
    }
    bool World2D::alive(std::string_view id) const {return entities_.contains(id);
    }
    void World2D::destroy(std::string_view id) {
        const auto e=get(id);
        if(e.body.value) physics.destroy(e.body);
        scene.destroy(e.handle);
        entities_.erase(std::string(id));
        std::erase_if(document_["joints"].array(),[this,id](const Json& j) {if(j.at("a").string()==id||j.at("b").string()==id) {joints_.erase(j.at("id").string());return true;}return false;});
    }
    std::string World2D::find(std::string_view name) const {for(const auto& [id,e]:entities_) if(e.document.text("name")==name) return id;
        return {};
    }
    Transform2D World2D::transform(std::string_view id,float alpha) const {const auto& e=get(id);
        auto t=scene.get(e.handle).transform;
        if(e.body.value) {const auto p=physics.pose(e.body,alpha);
            t.position=p.position;
            t.rotation=p.angle;
        }return t;
    }
    void World2D::transform(std::string_view id,const Transform2D& t) {auto& e=get(id);
        const auto checked=readTransform(writeTransform(t));
        if(e.body.value) physics.setPose(e.body,{checked.position,checked.rotation});
        scene.get(e.handle).transform=checked;
    }
    Json World2D::snapshot() const {
        auto result=document_;
        Json::Array entities;
        for(const auto& [id,e]:entities_) {auto value=e.document;
            value["transform"]=writeTransform(transform(id));
            if(e.body.value) value["body"]["velocity"]=toJson(physics.velocity(e.body));
            entities.push_back(std::move(value));
        }
        result["entities"]=std::move(entities);
        if(result.contains("joints")) std::erase_if(result.at("joints").array(),[this](const Json& j){return !alive(j.at("a").string())||!alive(j.at("b").string());});
        return result;
    }
    Json World2D::bindings() const {
        Json::Array result;
        for(const auto& [id,e]:entities_) if(e.document.contains("scripts")) for(auto binding:e.document.at("scripts").array()) {binding["entity"]=id;
            result.push_back(std::move(binding));
        }
        return result;
    }
    std::string World2D::entityFor(BodyHandle body) const {for(const auto& [id,e]:entities_) if(e.body==body) return id;
        return {};
    }
}
