#include <twodog/gameplay/World2D.hpp>
#include <twodog/core/LaunchOptions.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    using namespace twodog;
    void expect(bool value,const char* message) {if(!value) throw std::runtime_error(message);
    }
    template<class F> void rejects(F operation) {bool caught=false;
        try {operation();
        }catch(const std::exception&) {caught=true;
        }expect(caught,"Expected validation failure");
    }
    void run(const std::string& test,const std::filesystem::path& root) {
        if(test=="json_roundtrip") {
            const auto j=Json::parse(R"({"array":[null,true,false,1.25e2],"text":"line\n\"quoted\"","unicode":"\u00e8\ud83d\udc15"})");
            expect(Json::parse(j.dump()).dump()==j.dump(),"JSON roundtrip changed values");
            expect(j.at("array").array()[3].number()==125,"Exponent parsing");
            return;
        }
        if(test=="json_reject") {
            for(const auto* bad:{"{\"x\":1,\"x\":2}","[1,]","01","NaN","1e999","\"\\ud800\"","\"\\udc00\"","true false","{\"x\":}","[.5]"}) rejects([bad]{Json::parse(bad);});
            return;
        }
        if(test=="json_limits") {
            rejects([]{Json::parse(std::string(70,'[')+"0"+std::string(70,']'));});
            rejects([]{Json(1.2).integer();});
            rejects([]{static_cast<void>(Json::Object{}.at("missing"));});
            return;
        }
        if(test=="path_containment") {
            rejects([&]{containedPath(root,"../escape.json");});
            rejects([&]{containedPath(root,std::filesystem::absolute(root)/"x.json");});
            expect(containedPath(root,"scenes/demo.scene.json")==std::filesystem::canonical(root/"scenes/demo.scene.json"),"Contained path mismatch");
            return;
        }
        Project project(root);
        auto document=project.scene("scene.demo");
        if(test=="scene_roundtrip") {
            World2D first(project,document);
            auto snapshot=first.snapshot();
            World2D second(project,Json::parse(snapshot.dump()));
            expect(first.entities().size()==second.entities().size(),"Lost entities");
            expect(second.find("Player")=="player","Stable ID/name lost");
            expect(first.physics.bodyCount()==second.physics.bodyCount(),"Lost bodies");
            return;
        }
        if(test=="scene_duplicate") {document["entities"].array().push_back(document.at("entities").array().front());
            rejects([&]{World2D world(project,document);});
            return;
        }
        if(test=="prefab_instances") {
            World2D world(project,document);
            const auto a=world.create(project.prefab("prefab.crate",{1,2}));
            const auto b=world.create(project.prefab("prefab.crate",{3,4}));
            expect(a!=b,"Prefab IDs alias");
            expect(world.transform(a).position.x==1,"Prefab position lost");
            world.destroy(a);
            expect(!world.alive(a)&&world.alive(b),"Destroy corrupted sibling prefab");
            return;
        }
        if(test=="scene_transaction") {
            auto active=std::make_unique<World2D>(project,document);
            const auto count=active->entities().size();
            auto bad=document;
            bad["entities"].array().front()["body"]["colliders"].array().front()["halfSize"]=Json::Array{-1,1};
            rejects([&]{auto candidate=std::make_unique<World2D>(project,bad);active=std::move(candidate);});
            expect(active->entities().size()==count,"Failed load modified active scene");
            return;
        }
        if(test=="physics_compound") {
            Physics world({0,0});
            BodyConfig c;
            c.position={0,0};
            const auto body=world.create(c);
            BodyConfig sensor;
            sensor.sensor=true;
            sensor.localPosition={0,-1};
            world.addCollider(body,sensor);
            expect(world.bodyCount()==1,"Adding collider created another body");
            world.setPose(body,{{4,2},.25F});
            expect(world.pose(body,0).position.x==4,"Teleport left stale interpolation");
            world.destroy(body);
            expect(!world.alive(body),"Compound body not destroyed");
            return;
        }
        if(test=="physics_more_joints") {
            for(const auto type:{JointType::Revolute,JointType::Prismatic,JointType::Weld}) {
                Physics world;
                BodyConfig c;
                c.type=BodyType::Static;
                const auto a=world.create(c);
                c.type=BodyType::Dynamic;
                c.position={0,-2};
                const auto b=world.create(c);
                JointConfig config;
                config.type=type;
                config.anchor={0,0};
                config.axis={1,0};
                config.limit=true;
                config.lower=-1;
                config.upper=1;
                const auto joint=world.joint(a,b,config);
                for(int i=0;i<20;++i) world.step(1.0F/60);
                expect(std::isfinite(world.pose(b).position.y),"Joint produced non-finite state");
                world.destroyJoint(joint);
                rejects([&]{world.destroyJoint(joint);});
            }
            return;
        }
        if(test=="raycast_ignore") {
            Physics world;
            BodyConfig c;
            const auto player=world.create(c);
            c.type=BodyType::Static;
            c.position={0,-2};
            const auto floor=world.create(c);
            const auto hit=world.raycast({0,2},{0,-5},~std::uint64_t{0},player);
            expect(hit&&hit->body==floor,"Ignored body intercepted ray");
            return;
        }
        if(test=="project_cli") {
            const std::string_view args[]={"--project","bundled","--managed","managed","--no-scripts"};
            const auto opts=parseOptions(args);
            expect(opts.projectPath=="bundled"&&opts.noScripts,"Project options not parsed");
            rejects([]{const std::string_view invalid[]={"--project","bundled","--window-only"};static_cast<void>(parseOptions(invalid));});
            return;
        }
        throw std::invalid_argument("Unknown test case");
    }
}
int main(int argc,char** argv) {
    try {if(argc!=3) throw std::invalid_argument("Usage: TwoDogDataTests CASE PROJECT");
        run(argv[1],argv[2]);
        std::cout<<"PASS "<<argv[1]<<'\n';
        return 0;
    }
    catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';
        return 1;
    }
}
