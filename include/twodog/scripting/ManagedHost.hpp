#pragma once
#include <twodog/data/Json.hpp>
#include <filesystem>
#include <functional>
#include <memory>

namespace twodog {
enum class ScriptApi : int {
#define TD_API(name,code,request,result) name=code,
#include <twodog/scripting/Api.def>
#undef TD_API
};
class ManagedHost final {
public:
    using Dispatch=std::function<Json(ScriptApi,const Json&)>;
    ManagedHost(const std::filesystem::path& directory,Dispatch dispatch);
    ~ManagedHost();
    ManagedHost(const ManagedHost&)=delete;
    ManagedHost& operator=(const ManagedHost&)=delete;
    // Prepare is transactional: invalid assemblies do not replace the running context.
    void prepare(const std::filesystem::path& assembly,const Json& bindings);
    void commit();
    void detach(); // Destroy active instances while retaining a prepared replacement.
    void sync(const Json& bindings);
    void update(double dt);
    void fixedUpdate(double dt);
    void message(const Json& message);
    void stop();
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
