#include <twodog/platform/Sandbox.hpp>
#include <twodog/platform/OpenGL.hpp>
#include <twodog/gameplay/Input.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cmath>

int main(int argc,char** argv) {
    try {
        if(argc<1) throw std::runtime_error("No executable path");
        twodog::WindowConfig config;config.visible=false;config.vsync=false;
        twodog::Window window(config);twodog::OpenGL gl(window);
        twodog::Sandbox demo(window,std::filesystem::absolute(argv[0]).parent_path()/"assets");
        auto require=[](bool b,const char* message){if(!b)throw std::runtime_error(message);};
        auto& input=window.input();const auto initial=demo.status();
        for(int i=0;i<120;++i) demo.fixedUpdate(1.0/60);
        auto grounded=demo.status();require(grounded.playerPosition.y<initial.playerPosition.y,"Player must fall onto ground");
        input.beginFrame();input.button(twodog::InputDevice::Keyboard,32,true);demo.fixedUpdate(1.0/60);
        std::cout<<"Jump diagnostic: before="<<grounded.playerPosition.y<<" after="<<demo.status().playerPosition.y<<'\n';
        require(demo.status().playerPosition.y>grounded.playerPosition.y,"Space must jump from ground");
        input.button(twodog::InputDevice::Keyboard,32,false);
        input.beginFrame();input.button(twodog::InputDevice::Keyboard,68,true);
        for(int i=0;i<30;++i) demo.fixedUpdate(1.0/60);
        require(demo.status().playerPosition.x>initial.playerPosition.x+1,"D must move player right");
        input.releaseAll();input.beginFrame();input.button(twodog::InputDevice::Keyboard,80,true);demo.poll();
        require(demo.status().paused,"P must pause");const auto stopped=demo.status().playerPosition;
        for(int i=0;i<30;++i) demo.fixedUpdate(1.0/60);
        require(glm::length(demo.status().playerPosition-stopped)<0.0001F,"Paused scene changed");
        input.button(twodog::InputDevice::Keyboard,80,false);input.beginFrame();input.button(twodog::InputDevice::Keyboard,78,true);demo.poll();demo.fixedUpdate(1.0/60);
        const auto stepped=demo.status().playerPosition;
        require(glm::length(stepped-stopped)>0.001F,"N must step once");
        for(int i=0;i<30;++i) demo.fixedUpdate(1.0/60);
        require(glm::length(demo.status().playerPosition-stepped)<0.0001F,"Single-step repeated");
        input.releaseAll();input.beginFrame();input.cursor(640,300);input.button(twodog::InputDevice::Mouse,0,true);demo.poll();
        require(demo.status().bodyCount==initial.bodyCount+1,"Click must create body");
        input.releaseAll();input.beginFrame();input.button(twodog::InputDevice::Keyboard,82,true);demo.poll();
        require(demo.status().bodyCount==initial.bodyCount,"Reset must restore initial scene");
        require(glm::length(demo.status().playerPosition-initial.playerPosition)<0.0001F,"Reset must restore player");
        gl.clear(0,0,0);demo.render(0);gl.checkError("Sandbox after input/reset");
        std::cout<<"PASS sandbox jump, movement, pause, single step, spawn and reset\n";return 0;
    } catch(const std::exception& e){std::cerr<<"FAIL sandbox: "<<e.what()<<'\n';return 1;}
}
