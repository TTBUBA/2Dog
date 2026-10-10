#include <twodog/gameplay/Scene.hpp>
#include <twodog/gameplay/Input.hpp>
#include <twodog/gameplay/Physics.hpp>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <limits>

namespace {
using namespace twodog;
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
void near(float a,float b,float e=0.001F){require(std::isfinite(a)&&std::abs(a-b)<e,"Numeric mismatch");}
template<class F> void rejects(F f){try{f();}catch(const std::invalid_argument&){return;}throw std::runtime_error("Invalid input accepted");}
const std::map<std::string,std::function<void()>> cases={
{"entity_generation",[]{Scene s;auto a=s.create();s.destroy(a);auto b=s.create();require(a.index==b.index&&a.generation!=b.generation,"Generation must change");require(!s.alive(a)&&s.alive(b),"Stale entity aliased new one");rejects([&]{s.get(a);});}},
{"scene_destroy",[]{Scene s;auto a=s.create(),b=s.create();s.get(a).sprite=Sprite{};s.destroy(a);require(s.entities().size()==1&&s.entities()[0]==b,"Scene membership");rejects([&]{s.destroy(a);});require(!s.get(s.create()).sprite,"Component persisted across reuse");}},
{"camera_roundtrip",[]{Camera2D c;c.center={5,-2};c.zoom=1.7F;c.width=900;c.height=600;const auto p=c.screenToWorld({235,410});const auto q=c.worldToScreen(p);near(q.x,235);near(q.y,410);c.zoom=0;rejects([&]{c.projection();});}},
{"animation",[]{SpriteAnimation a({{0,0,0.5F,1},{0.5F,0,1,1}},10);a.advance(0.1);require(a.frame()==1,"Frame advance");a.advance(0.1);require(a.frame()==0,"Loop boundary");a.advance(100);require(a.frame()==0,"Large delta loop");SpriteAnimation once({{0,0,1,1}},10,false);once.advance(0.2);require(once.finished(),"Non-loop animation");once.reset();require(!once.finished(),"Animation reset");rejects([&]{a.advance(-1);});}},
{"input_edges",[]{Input i;i.bind("jump",{{InputDevice::Keyboard,32}});i.button(InputDevice::Keyboard,32,true);i.button(InputDevice::Keyboard,32,false);i.beginFrame();auto s=i.consumeFixed().at("jump");require(!s.down&&s.pressed&&s.released,"Quick tap lost before a fixed update");s=i.consumeFixed().at("jump");require(!s.pressed&&!s.released,"Edge delivered twice");}},
{"input_multibind",[]{Input i;i.bind("move",{{InputDevice::Keyboard,65},{InputDevice::Keyboard,263}});i.button(InputDevice::Keyboard,65,true);i.button(InputDevice::Keyboard,263,true);i.consumeFixed();i.beginFrame();i.button(InputDevice::Keyboard,65,false);auto s=i.consumeFixed().at("move");require(s.down&&!s.released,"Releasing one binding released action");i.button(InputDevice::Keyboard,263,false);require(i.action("move").released,"Final binding release lost");}},
{"input_focus",[]{Input i;i.bind("fire",{{InputDevice::Mouse,0}});i.button(InputDevice::Mouse,0,true);i.gamepadAxis(0,1);i.releaseAll();auto s=i.consumeFixed().at("fire");require(!s.down&&!s.pressed&&s.released,"Focus loss must cancel held and pending press");near(i.axis(0),0);}},
{"input_validation",[]{Input i;rejects([&]{i.bind("bad",{{InputDevice::Keyboard,999}});});rejects([&]{i.button(InputDevice::Mouse,-1,true);});rejects([&]{i.gamepadAxis(0,std::numeric_limits<float>::quiet_NaN());});i.gamepadAxis(0,0.1F);near(i.axis(0),0);}},
{"physics_gravity",[]{Physics p;BodyConfig c;c.position={0,10};auto b=p.create(c);for(int i=0;i<60;++i)p.step(1.0F/60);near(p.pose(b).position.y,5.1F,0.2F);near(p.velocity(b).y,-9.81F,0.1F);const auto x=p.pose(b,0),y=p.pose(b,1);require(x.position.y>y.position.y,"Interpolation history");}},
{"physics_bounce",[]{Physics p;BodyConfig floor;floor.type=BodyType::Static;floor.halfSize={10,0.5F};p.create(floor);BodyConfig ball;ball.collider=ColliderType::Circle;ball.position={0,3};ball.restitution=0.9F;auto b=p.create(ball);bool bounce=false,contact=false;for(int i=0;i<150;++i){p.step(1.0F/60);bounce=bounce||p.velocity(b).y>1;for(const auto& e:p.events())contact=contact||e.kind==PhysicsEvent::Kind::ContactBegin;}require(bounce&&contact,"Collision response and event required");require(p.pose(b).position.y>0.8F,"Body tunneled through floor");}},
{"physics_trigger",[]{Physics p({0,0});BodyConfig s;s.type=BodyType::Static;s.sensor=true;s.halfSize={1,1};p.create(s);BodyConfig c;c.position={0,0};auto b=p.create(c);p.step(1.0F/60);bool begin=false,end=false;for(const auto& e:p.events())begin=begin||e.kind==PhysicsEvent::Kind::TriggerBegin;p.setVelocity(b,{10,0});for(int i=0;i<60;++i){p.step(1.0F/60);for(const auto& e:p.events())end=end||e.kind==PhysicsEvent::Kind::TriggerEnd;}require(begin&&end,"Trigger enter/exit");require(p.pose(b).position.x>5,"Sensor blocked motion");}},
{"physics_raycast",[]{Physics p({0,0});BodyConfig c;c.type=BodyType::Static;c.position={3,0};c.category=4;auto b=p.create(c);auto hit=p.raycast({0,0},{6,0});require(hit&&hit->body==b,"Closest hit");near(hit->point.x,2.5F,0.02F);require(!p.raycast({0,0},{6,0},2),"Ray mask ignored");require(!p.raycast({0,5},{6,0}),"Ray miss");}},
{"physics_destroy",[]{Physics a,b;auto h=a.create({});require(a.alive(h)&&!b.alive(h),"World ownership");rejects([&]{b.destroy(h);});a.destroy(h);rejects([&]{a.velocity(h);});rejects([&]{a.destroy(h);});a.step(1.0F/60);require(a.bodyCount()==0,"Destroy failed");}},
{"physics_joint",[]{Physics p;BodyConfig c;c.type=BodyType::Static;auto anchor=p.create(c);c={};c.position={2,0};auto ball=p.create(c);auto joint=p.distanceJoint(anchor,ball,2);for(int i=0;i<90;++i)p.step(1.0F/60);near(glm::length(p.pose(ball).position),2,0.05F);p.destroyJoint(joint);rejects([&]{p.destroyJoint(joint);});}},
{"physics_validation",[]{Physics p;BodyConfig c;c.radius=-1;c.collider=ColliderType::Circle;rejects([&]{p.create(c);});c={};c.collider=ColliderType::Polygon;c.vertices={{0,0},{1,0},{2,0}};rejects([&]{p.create(c);});c.vertices={{0,0},{1,0},{0,1}};auto b=p.create(c);require(p.alive(b),"Valid polygon rejected");rejects([&]{p.step(0);});rejects([&]{p.setVelocity(b,{std::numeric_limits<float>::infinity(),0});});rejects([&]{p.pose(b,2);});}},
{"physics_force",[]{Physics p({0,0});auto b=p.create({});p.impulse(b,{2,0});near(p.velocity(b).x,2);p.force(b,{0,60});p.step(1.0F/60);near(p.velocity(b).y,1,0.01F);p.step(1.0F/60);near(p.velocity(b).y,1,0.01F);}},
{"physics_kinematic",[]{Physics p;BodyConfig c;c.type=BodyType::Kinematic;c.velocity={2,0};auto b=p.create(c);for(int i=0;i<60;++i)p.step(1.0F/60);near(p.pose(b).position.x,2,0.02F);near(p.pose(b).position.y,0);c.type=BodyType::Static;auto a=p.create(c);auto j=p.distanceJoint(a,b,2);p.destroy(a);rejects([&]{p.destroyJoint(j);});}}
};
}
int main(int argc,char** argv){try{if(argc!=2||!cases.contains(argv[1]))throw std::runtime_error("Unknown test");cases.at(argv[1])();std::cout<<"PASS "<<argv[1]<<'\n';return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
