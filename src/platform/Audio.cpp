#include <twodog/platform/Audio.hpp>
#include <miniaudio.h>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
namespace twodog {
    struct Audio::Impl {
        struct Voice {ma_sound sound{};
            bool initialized=false;
            ~Voice() {if(initialized) ma_sound_uninit(&sound);
            }};
        ma_engine engine{};
        ma_sound_group sfx{},music{};
        bool ready=false;
        std::uint64_t sequence=1;
        std::map<std::string,std::unique_ptr<Voice>,std::less<>> voices;
        Impl() {
            if(ma_engine_init(nullptr,&engine)!=MA_SUCCESS) {std::cerr<<"[2Dog][audio] No playback device: continuing silently\n";
                return;
            }
            if(ma_sound_group_init(&engine,0,nullptr,&sfx)!=MA_SUCCESS) {ma_engine_uninit(&engine);
                throw std::runtime_error("Cannot initialize SFX bus");
            }
            if(ma_sound_group_init(&engine,0,nullptr,&music)!=MA_SUCCESS) {ma_sound_group_uninit(&sfx);
                ma_engine_uninit(&engine);
                throw std::runtime_error("Cannot initialize music bus");
            }
            ready=true;
        }
        ~Impl() {voices.clear();
            if(ready) {ma_sound_group_uninit(&music);
                ma_sound_group_uninit(&sfx);
                ma_engine_uninit(&engine);
            }}
    };
    Audio::Audio():impl_(std::make_unique<Impl>()) {} Audio::~Audio()=default;
    bool Audio::available() const {return impl_->ready;
    }
    std::string Audio::play(const std::filesystem::path& file,std::string_view bus,bool loop) {
        if(bus!="sfx"&&bus!="music") throw std::invalid_argument("Audio bus must be sfx or music");
        if(!impl_->ready) return {};
        collect();
        if(impl_->voices.size()>=64) throw std::runtime_error("64 simultaneous audio voices reached");
        // Initialize into separately allocated memory; failed initialization is not uninitialized twice.
        auto owned=std::make_unique<Impl::Voice>();
        auto* voice=owned.get();
        const ma_uint32 flags=bus=="music"?MA_SOUND_FLAG_STREAM:MA_SOUND_FLAG_DECODE;
#ifdef _WIN32
        const auto rc=ma_sound_init_from_file_w(&impl_->engine,file.c_str(),flags,bus=="sfx"?&impl_->sfx:&impl_->music,nullptr,&voice->sound);
#else
        const auto rc=ma_sound_init_from_file(&impl_->engine,file.c_str(),flags,bus=="sfx"?&impl_->sfx:&impl_->music,nullptr,&voice->sound);
#endif
        if(rc!=MA_SUCCESS) throw std::runtime_error("Cannot decode audio: "+file.string());
        voice->initialized=true;
        ma_sound_set_looping(&voice->sound,loop?MA_TRUE:MA_FALSE);
        if(ma_sound_start(&voice->sound)!=MA_SUCCESS) throw std::runtime_error("Cannot start audio voice");
        const auto id=std::to_string(impl_->sequence++);
        impl_->voices.emplace(id,std::move(owned));
        return id;
    }
    void Audio::stop(std::string_view voice) {impl_->voices.erase(std::string(voice));
    }
    void Audio::collect() {std::erase_if(impl_->voices,[](const auto& item){return ma_sound_at_end(&item.second->sound)==MA_TRUE;});
    }
    void Audio::volume(std::string_view bus,float value) {
        if(!std::isfinite(value)||value<0||value>1) throw std::invalid_argument("Audio volume must be 0..1");
        if(bus!="master"&&bus!="sfx"&&bus!="music") throw std::invalid_argument("Unknown audio bus");
        if(!impl_->ready) return;
        if(bus=="master") ma_engine_set_volume(&impl_->engine,value);
        else ma_sound_group_set_volume(bus=="sfx"?&impl_->sfx:&impl_->music,value);
    }
    void Audio::pause(bool value) {if(impl_->ready) {if(value) ma_engine_stop(&impl_->engine);
            else ma_engine_start(&impl_->engine);
        }}
    void Audio::reset() {impl_->voices.clear();
        volume("master",1);
        volume("music",1);
        volume("sfx",1);
        pause(false);
    }
}
