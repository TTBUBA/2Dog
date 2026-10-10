#include <twodog/scripting/ManagedHost.hpp>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <thread>
#if defined(_WIN32) && defined(TWODOG_CSHARP)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <nethost.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>
#endif

namespace twodog {
#if defined(_WIN32) && defined(TWODOG_CSHARP)
    namespace {
        template<class T> T symbol(HMODULE module,const char* name) {
            const auto address=GetProcAddress(module,name);
            if(!address) throw std::runtime_error(std::string("Missing .NET export: ")+name);
            T result{};
            static_assert(sizeof(result)==sizeof(address));
            std::memcpy(&result,&address,sizeof(result));
            return result;
        }
        std::string utf8Path(const std::filesystem::path& path) {const auto value=std::filesystem::absolute(path).u8string();
            return {value.begin(),value.end()};
        }
    }
    struct ManagedHost::Impl {
        using Callback=int(__cdecl*)(void*,int,const char*,int,char*,int);
        using Entry=int(__cdecl*)(int,const char*,double,Callback,void*);
        Dispatch dispatch;
        Entry entry=nullptr;
        std::thread::id owner=std::this_thread::get_id();
        bool connected=false;
        static int __cdecl invoke(void* context,int code,const char* request,int length,char* response,int capacity) noexcept {
            auto& self=*static_cast<Impl*>(context);
            try {
                if(std::this_thread::get_id()!=self.owner) throw std::runtime_error("Engine API is main-thread only");
                if(!request||length<0||length>1024*1024||!response||capacity<128) throw std::runtime_error("Invalid managed ABI buffer");
                const auto result=self.dispatch(static_cast<ScriptApi>(code),Json::parse({request,static_cast<std::size_t>(length)})).dump();
                if(result.size()>=static_cast<std::size_t>(capacity)) throw std::runtime_error("API response exceeds 1 MiB; use smaller batches");
                std::memcpy(response,result.data(),result.size());
                response[result.size()]=0;
                return static_cast<int>(result.size());
            } catch(const std::exception& e) {
                if(response&&capacity>0) {const auto n=std::min(std::strlen(e.what()),static_cast<std::size_t>(capacity-1));
                    std::memcpy(response,e.what(),n);
                    response[n]=0;
                }
                return -1;
            } catch(...) {if(response&&capacity>0) response[0]=0;
                return -1;
            }
        }
        void command(int code,const std::string& data={},double dt=0) {
            if(std::this_thread::get_id()!=owner) throw std::runtime_error("Managed host is main-thread only");
            if(entry(code,data.c_str(),dt,&invoke,this)!=0) throw std::runtime_error("C# host operation failed; see [C#] console diagnostic");
        }
        Impl(const std::filesystem::path& directory,Dispatch callback):dispatch(std::move(callback)) {
            const auto root=std::filesystem::absolute(directory);
            const auto net=LoadLibraryExW((root/L"native"/L"nethost.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if(!net) throw std::runtime_error("Cannot load managed/native/nethost.dll; run build-csharp.ps1");
            std::wstring fxr(32768,L'\0');
            std::size_t size=fxr.size();
            const auto getPath=symbol<decltype(&get_hostfxr_path)>(net,"get_hostfxr_path");
            const auto privateRoot=root.parent_path()/L"dotnet";
            const auto privateRootText=privateRoot.wstring();
            const bool portable=std::filesystem::is_directory(privateRoot);
            const get_hostfxr_parameters parameters{sizeof(get_hostfxr_parameters),nullptr,privateRootText.c_str()};
            const auto located=getPath(fxr.data(),&size,portable?&parameters:nullptr);
            FreeLibrary(net);
            if(located!=0) throw std::runtime_error("Cannot locate x64 .NET runtime; install .NET 10 SDK x64");
            // The runtime and hostfxr remain loaded for process lifetime, per native hosting contract.
            const auto host=LoadLibraryExW(fxr.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if(!host) throw std::runtime_error("Cannot load hostfxr");
            const auto initialize=symbol<hostfxr_initialize_for_runtime_config_fn>(host,"hostfxr_initialize_for_runtime_config");
            const auto getDelegate=symbol<hostfxr_get_runtime_delegate_fn>(host,"hostfxr_get_runtime_delegate");
            const auto close=symbol<hostfxr_close_fn>(host,"hostfxr_close");
            hostfxr_handle handle=nullptr;
            const auto rc=initialize((root/L"TwoDog.ManagedHost.runtimeconfig.json").c_str(),nullptr,&handle);
            if(rc<0||!handle) {if(handle) close(handle);
                throw std::runtime_error("Cannot initialize .NET 10; inspect hostfxr diagnostics");
            }
            void* raw=nullptr;
            const auto dc=getDelegate(handle,hdt_load_assembly_and_get_function_pointer,&raw);
            close(handle);
            if(dc!=0||!raw) throw std::runtime_error("Cannot obtain .NET assembly loader");
            load_assembly_and_get_function_pointer_fn load{};
            static_assert(sizeof(load)==sizeof(raw));
            std::memcpy(&load,&raw,sizeof(load));
            void* pointer=nullptr;
            const auto lc=load((root/L"TwoDog.ManagedHost.dll").c_str(),L"TwoDog.ManagedHost.Entry, TwoDog.ManagedHost",L"Command",UNMANAGEDCALLERSONLY_METHOD,nullptr,&pointer);
            if(lc!=0||!pointer) throw std::runtime_error("Cannot load C# bootstrap entry point");
            static_assert(sizeof(entry)==sizeof(pointer));
            std::memcpy(&entry,&pointer,sizeof(entry));
            command(0);
            connected=true;
        }
        ~Impl() {if(connected) {try {command(6);
                }catch(...) {}}}
    };
    ManagedHost::ManagedHost(const std::filesystem::path& p,Dispatch d):impl_(std::make_unique<Impl>(p,std::move(d))) {}
    ManagedHost::~ManagedHost()=default;
    void ManagedHost::prepare(const std::filesystem::path& assembly,const Json& bindings) {impl_->command(1,Json(Json::Object{{"assembly",utf8Path(assembly)},{"bindings",bindings},{"api",1}}).dump());
    }
    void ManagedHost::detach() {impl_->command(8);
    }
    void ManagedHost::commit() {impl_->command(2);
    }
    void ManagedHost::sync(const Json& bindings) {impl_->command(7,bindings.dump());
    }
    void ManagedHost::update(double dt) {impl_->command(3,{},dt);
    }
    void ManagedHost::fixedUpdate(double dt) {impl_->command(4,{},dt);
    }
    void ManagedHost::message(const Json& value) {impl_->command(5,value.dump());
    }
    void ManagedHost::stop() {impl_->command(6);
    }
#else
    struct ManagedHost::Impl {};
    ManagedHost::ManagedHost(const std::filesystem::path&,Dispatch) {throw std::runtime_error("C# hosting in 0.3 requires Windows x64 and TWODOG_CSHARP=ON");
    }
    ManagedHost::~ManagedHost()=default;
    void ManagedHost::prepare(const std::filesystem::path&,const Json&) {throw std::runtime_error("C# unavailable");
    }
    void ManagedHost::detach() {}
    void ManagedHost::commit() {} void ManagedHost::sync(const Json&) {} void ManagedHost::update(double) {} void ManagedHost::fixedUpdate(double) {} void ManagedHost::message(const Json&) {} void ManagedHost::stop() {}
#endif
}
