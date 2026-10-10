#include <twodog/data/ComponentSchema.hpp>
#include <twodog/data/Project.hpp>
#include <twodog/core/Utf8Path.hpp>
#include <atomic>
#include <chrono>
#include <cmath>
#include <random>
#include <set>
#include <stdexcept>

namespace twodog {
    namespace {
        float scalar(const Json& v) {const auto n=v.number();
            if(std::abs(n)>100000) throw std::invalid_argument("Component value outside supported range");
            return static_cast<float>(n);
        }
        void require(bool condition,const char* message) {if(!condition) throw std::invalid_argument(message);
        }
    }
    glm::vec2 jsonVec2(const Json& v) {require(v.array().size()==2,"Expected two numbers");
        return {scalar(v.array()[0]),scalar(v.array()[1])};
    }
    glm::vec4 jsonVec4(const Json& v) {require(v.array().size()==4,"Expected four numbers");
        return {scalar(v.array()[0]),scalar(v.array()[1]),scalar(v.array()[2]),scalar(v.array()[3])};
    }
    Json toJson(glm::vec2 v) {return Json::Array{v.x,v.y};
    }
    Json toJson(glm::vec4 v) {return Json::Array{v.x,v.y,v.z,v.w};
    }
    Transform2D readTransform(const Json& v) {
        Transform2D t;
        if(v.contains("position")) t.position=jsonVec2(v.at("position"));
        if(v.contains("size")) t.size=jsonVec2(v.at("size"));
        if(v.contains("rotation")) t.rotation=scalar(v.at("rotation"));
        require(t.size.x>0 && t.size.y>0,"Transform size must be positive");
        return t;
    }
    Json writeTransform(const Transform2D& t) {return Json::Object{{"position",toJson(t.position)},{"size",toJson(t.size)},{"rotation",t.rotation}};
    }
    std::string newPersistentId() {
        static const auto seed=static_cast<std::uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count()) ^ (static_cast<std::uint64_t>(std::random_device{}())<<32);
        static std::atomic<std::uint64_t> sequence{1};
        // IDs are strings across the ABI, never floating-point integers.
        return "e-"+std::to_string(seed)+"-"+std::to_string(sequence.fetch_add(1));
    }
    Project::Project(const std::filesystem::path& directory):root_(std::filesystem::canonical(directory)),settings_(Json::read(root_/"project.json")) {
        require(settings_.at("version").integer()==1,"Unsupported project version");
        for(const auto& value:settings_.at("assets").array()) {
            AssetRecord asset{value.at("id").string(),value.at("type").string(),containedPath(root_,pathFromUtf8(value.at("path").string()))};
            require(!asset.id.empty(),"Empty asset ID");
            require(asset.type=="texture"||asset.type=="audio"||asset.type=="font"||asset.type=="scene"||asset.type=="prefab"||asset.type=="tilemap","Unknown asset type");
            require(std::filesystem::is_regular_file(asset.path),"Asset file is missing");
            require(assets_.emplace(asset.id,asset).second,"Duplicate asset ID");
        }
        static_cast<void>(asset(settings_.at("startScene").string(),"scene"));
    }
    const AssetRecord& Project::asset(std::string_view id,std::string_view type) const {
        const auto it=assets_.find(id);
        if(it==assets_.end()) throw std::invalid_argument("Unknown asset: "+std::string(id));
        if(!type.empty()&&it->second.type!=type) throw std::invalid_argument("Wrong asset type: "+std::string(id));
        return it->second;
    }
    void Project::validateEntity(const Json& e) {
        require(!e.at("id").string().empty(),"Entity ID is empty");
        require(e.at("id").string().size()<=128,"Entity ID too long");
        static_cast<void>(e.text("name"));
        static_cast<void>(readTransform(e.at("transform")));
        ComponentSchema::validateEntity(e);
        if(e.contains("scripts")) {
            std::set<std::string> slots;
            for(const auto& s:e.at("scripts").array()) {
                require(s.text("language","C#")=="C#","Only C# is enabled in this milestone");
                require(!s.at("type").string().empty(),"Script type is empty");
                require(slots.insert(s.at("slot").string()).second,"Duplicate script slot on entity");
                static_cast<void>(s.at("order").integer());
                if(s.contains("properties")) static_cast<void>(s.at("properties").object());
            }
        }
    }
    void Project::validateScene(const Json& scene) {
        require(scene.at("version").integer()==1,"Unsupported scene version");
        require(!scene.at("id").string().empty(),"Scene ID is empty");
        std::set<std::string> ids;
        require(scene.at("entities").array().size()<=10000,"Scene exceeds 10000 entities");
        for(const auto& e:scene.at("entities").array()) {validateEntity(e);
            require(ids.insert(e.at("id").string()).second,"Duplicate entity ID");
        }
        if(scene.contains("gravity")) static_cast<void>(jsonVec2(scene.at("gravity")));
    }
    Json Project::scene(std::string_view id) const {auto result=Json::read(asset(id,"scene").path);
        validateScene(result);
        return result;
    }
    Json Project::prefab(std::string_view id,glm::vec2 position) const {
        const auto data=Json::read(asset(id,"prefab").path);
        require(data.at("version").integer()==1,"Unsupported prefab version");
        auto e=data.at("entity");
        validateEntity(e);
        e["id"]=newPersistentId();
        auto transform=readTransform(e.at("transform"));
        transform.position=position;
        e["transform"]=writeTransform(transform);
        e["prefab"]=std::string(id);
        return e;
    }
}
