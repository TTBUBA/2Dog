#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <twodog/core/Utf8Path.hpp>
#include <twodog/data/Json.hpp>
#include <twodog/data/ProjectLayout.hpp>
#include <twodog/editor/ScriptWorkspace.hpp>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace twodog {
namespace {
std::string bytes(const std::filesystem::path &p) {
    std::ifstream in(p, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot read script file");
    if (std::filesystem::file_size(p) > 4 * 1024 * 1024)
        throw std::runtime_error("Script exceeds 4 MiB");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
void write(const std::filesystem::path &p, const std::string &value) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << value;
    out.flush();
    if (!out)
        throw std::runtime_error("Cannot write script workspace");
}
std::string utf(const std::filesystem::path &p) {
    const auto u = p.generic_u8string();
    return {u.begin(), u.end()};
}
std::string xml(std::string s) {
    std::string out;
    for (char c : s) {
        switch (c) {
        case '&':
            out += "&amp;";
            break;
        case '<':
            out += "&lt;";
            break;
        case '>':
            out += "&gt;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
        }
    }
    return out;
}
// MSBuild expands special characters before resolving Include/Link metadata.
std::string itemPath(const std::string &value) {
    std::string escaped;
    constexpr char hex[] = "0123456789ABCDEF";
    for (unsigned char c : value) {
        if (std::string("%$@;\"'()?*").find(static_cast<char>(c)) != std::string::npos) {
            escaped += '%';
            escaped += hex[c >> 4];
            escaped += hex[c & 15];
        } else {
            escaped += static_cast<char>(c);
        }
    }
    return xml(escaped);
}
std::vector<std::filesystem::path> sources(const std::filesystem::path &dir) {
    std::vector<std::filesystem::path> result;
    if (!std::filesystem::exists(dir))
        return result;
    for (auto i = std::filesystem::recursive_directory_iterator(dir);
         i != std::filesystem::recursive_directory_iterator(); ++i) {
        if (i->is_symlink()) {
            if (i->is_directory())
                i.disable_recursion_pending();
            continue;
        }
        if (i->is_directory()) {
            const auto name = i->path().filename();
            if (name == "bin" || name == "obj" || name == ".twodog" || name == ".vs" || name == ".git" ||
                name == "build")
                i.disable_recursion_pending();
            continue;
        }
        if (i->is_regular_file() && i->path().extension() == ".cs")
            result.push_back(i->path());
    }
    std::sort(result.begin(), result.end());
    return result;
}
std::vector<std::string> scriptTypes(const std::filesystem::path &path) {
    auto text = bytes(path);
    // Generated scripts use a file-scoped namespace and directly inherit Behaviour.
    text = std::regex_replace(text, std::regex(R"(/\*[\s\S]*?\*/|//[^\r\n]*)"), " ");
    std::smatch match;
    std::string ns;
    if (std::regex_search(text, match, std::regex(R"(\bnamespace\s+([A-Za-z_][A-Za-z_0-9.]*)\s*[;{])")))
        ns = match[1].str() + ".";
    const std::regex cls(R"(\bclass\s+([A-Za-z_][A-Za-z_0-9]*)\s*:\s*(?:global::)?(?:TwoDog\.)?Behaviour\b)");
    std::vector<std::string> result;
    for (std::sregex_iterator i(text.begin(), text.end(), cls), end; i != end; ++i)
        result.push_back(ns + (*i)[1].str());
    return result;
}
#ifdef _WIN32
std::wstring quote(const std::wstring &s) {
    std::wstring out = L"\"";
    std::size_t slashes = 0;
    for (wchar_t c : s) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        if (c == L'"') {
            out.append(slashes * 2 + 1, L'\\');
            out += c;
        } else {
            out.append(slashes, L'\\');
            out += c;
        }
        slashes = 0;
    }
    out.append(slashes * 2, L'\\');
    out += L'"';
    return out;
}
#endif
class Process final {
#ifdef _WIN32
    HANDLE process_ = nullptr, job_ = nullptr;
#endif
  public:
    ~Process() {
#ifdef _WIN32
        if (job_)
            CloseHandle(job_); // Only the compiler tree owned by this editor is cancelled.
        if (process_)
            CloseHandle(process_);
#endif
    }
    bool running() const {
#ifdef _WIN32
        return process_ != nullptr;
#else
        return false;
#endif
    }
    void launch(const std::filesystem::path &exe, const std::vector<std::filesystem::path> &args,
                const std::filesystem::path &cwd, const std::filesystem::path &log, bool ownTree) {
#ifdef _WIN32
        if (process_)
            throw std::runtime_error("A compiler process is already running");
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE output = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
        if (output == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot create compiler log");
        HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        if (input == INVALID_HANDLE_VALUE) {
            CloseHandle(output);
            throw std::runtime_error("Cannot create compiler stdin");
        }
        std::wstring command = quote(exe.wstring());
        for (const auto &a : args)
            command += L" " + quote(a.wstring());
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = input;
        si.hStdOutput = output;
        si.hStdError = output;
        PROCESS_INFORMATION pi{};
        const BOOL ok = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
                                       CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, cwd.c_str(), &si, &pi);
        CloseHandle(output);
        CloseHandle(input);
        if (!ok)
            throw std::runtime_error("Cannot start compiler/IDE helper; verify .NET SDK and PowerShell are installed");
        if (ownTree) {
            job_ = CreateJobObjectW(nullptr, nullptr);
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (!job_ || !SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits)) ||
                !AssignProcessToJobObject(job_, pi.hProcess)) {
                TerminateProcess(pi.hProcess, 1);
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                if (job_)
                    CloseHandle(job_);
                job_ = nullptr;
                throw std::runtime_error("Cannot contain compiler process");
            }
        }
        process_ = pi.hProcess;
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);
#else
        static_cast<void>(exe);
        static_cast<void>(args);
        static_cast<void>(cwd);
        static_cast<void>(log);
        static_cast<void>(ownTree);
        throw std::runtime_error("Automatic C# build and IDE integration require Windows");
#endif
    }
    std::optional<unsigned long> poll() {
#ifdef _WIN32
        if (!process_ || WaitForSingleObject(process_, 0) == WAIT_TIMEOUT)
            return {};
        DWORD code = 1;
        GetExitCodeProcess(process_, &code);
        CloseHandle(process_);
        process_ = nullptr;
        if (job_)
            CloseHandle(job_);
        job_ = nullptr;
        return code;
#else
        return {};
#endif
    }
};
} // namespace
struct ScriptWorkspace::Impl {
    std::filesystem::path root, managed, support, work, project, runtimeProject, dll, log;
    Process compiler, ide;
    std::string observed, building, status = "Preparing C# workspace";
    double scanAt = 0, changedAt = 0;
    bool pending = true, failed = false, includeDemo = false;
#ifdef NDEBUG
    std::string configuration = "Release";
#else
    std::string configuration = "Debug";
#endif
    Impl(const std::filesystem::path &r, const std::filesystem::path &m, const std::filesystem::path &s)
        : root(std::filesystem::canonical(r)), managed(std::filesystem::absolute(m)), support(s),
          work(root / ".twodog"), project(work / "GameScripts.csproj"),
          runtimeProject(ProjectLayout(root).scriptBuild() / "RuntimeScripts.csproj"),
          dll(ProjectLayout(root).scriptBuild() / "runtime" / "GameScripts.dll"),
          log(ProjectLayout(root).logs() / "compile.log") {
        std::filesystem::create_directories(root / "assets");
        std::filesystem::create_directories(work / "Builtin");
        std::filesystem::create_directories(dll.parent_path());
        std::filesystem::create_directories(log.parent_path());
        if (!std::filesystem::exists(support / "Defaults"))
            throw std::runtime_error("EditorSupport missing; rebuild the editor");
        includeDemo = Json::read(root / "project.json").flag("includeDemoScripts", true);
        for (const auto &file : includeDemo ? sources(support / "Defaults") : std::vector<std::filesystem::path>{}) {
            const auto dst = work / "Builtin" / file.filename();
            const auto content = bytes(file);
            if (!std::filesystem::exists(dst) || bytes(dst) != content)
                write(dst, content);
        }
        syncProject();
        std::string solution =
            "Microsoft Visual Studio Solution File, Format Version 12.00\n# Visual Studio Version 17\n";
        const std::string gameId = "{8B5674AD-EE54-4FCD-91B2-45B5AB4D991F}";
        const std::string apiId = "{9B6785BE-FF65-40DE-A2C3-56C6BC5E002A}";
        auto addProject = [&](const std::string &name, const std::string &path, const std::string &id) {
            solution += "Project(\"{9A19103F-16F7-4668-BE54-9A1E7A4F7556}\") = \"" + name + "\", \"" + path + "\", \"" +
                        id + "\"\nEndProject\n";
        };
        addProject("GameScripts", "GameScripts.csproj", gameId);
        addProject("TwoDog.API", "EngineAPI/TwoDog.API.csproj", apiId);
        solution += "Global\n\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n"
                    "\t\tDebug|Any CPU = Debug|Any CPU\n\t\tRelease|Any CPU = Release|Any CPU\n\tEndGlobalSection\n"
                    "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n";
        for (const auto &id : {gameId, apiId})
            for (const auto *config : {"Debug", "Release"}) {
                const std::string entry = id + "." + config + "|Any CPU";
                solution += "\t\t" + entry + ".ActiveCfg = " + config + "|Any CPU\n";
                solution += "\t\t" + entry + ".Build.0 = " + config + "|Any CPU\n";
            }
        solution += "\tEndGlobalSection\nEndGlobal\n";
        if (!std::filesystem::exists(work / "GameScripts.sln") || bytes(work / "GameScripts.sln") != solution)
            write(work / "GameScripts.sln", solution);
        observed = signature();
        if (std::filesystem::exists(dll) && std::filesystem::exists(work / "last-success.txt") &&
            bytes(work / "last-success.txt") == observed) {
            pending = false;
            status = "C# ready";
        }
    }
    void syncProject() const {
        auto save = [](const std::filesystem::path &path, const std::string &content) {
            if (!std::filesystem::exists(path) || bytes(path) != content) {
                std::filesystem::create_directories(path.parent_path());
                auto temporary = path;
                temporary += ".new";
                write(temporary, content);
#ifdef _WIN32
                if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                    throw std::runtime_error("Cannot refresh generated workspace; close pending IDE dialogs.");
#else
                std::filesystem::rename(temporary, path);
#endif
            }
        };
        // Keep VS2022's language service on a supported SDK/TFM.
        // Runtime builds are a separate project, SDK and set of intermediates.
        save(work / "global.json",
             R"({"sdk":{"version":"8.0.100","rollForward":"latestFeature","allowPrerelease":false}})");
        save(work / "Directory.Build.props",
             "<Project><PropertyGroup>"
             "<BaseIntermediateOutputPath>$(MSBuildThisFileDirectory)cache/ide/$(MSBuildProjectName)/obj/</"
             "BaseIntermediateOutputPath>"
             "<BaseOutputPath>$(MSBuildThisFileDirectory)cache/ide/$(MSBuildProjectName)/bin/</BaseOutputPath>"
             "<LangVersion>12.0</LangVersion><NuGetAudit>false</NuGetAudit>"
             "</PropertyGroup></Project>");
        save(work / "Directory.Build.targets", "<Project />");
        save(runtimeProject.parent_path() / "global.json",
             R"({"sdk":{"version":"10.0.100","rollForward":"latestFeature","allowPrerelease":false}})");
        save(runtimeProject.parent_path() / "Directory.Build.props",
             "<Project><PropertyGroup><BaseIntermediateOutputPath>obj/</BaseIntermediateOutputPath>"
             "<BaseOutputPath>build/</BaseOutputPath><LangVersion>12.0</LangVersion>"
             "</PropertyGroup></Project>");
        save(runtimeProject.parent_path() / "Directory.Build.targets", "<Project />");
        const auto apiFiles = sources(support / "API");
        if (apiFiles.empty())
            throw std::runtime_error("EditorSupport/API missing; rebuild the editor");
        const auto apiDirectory = work / "EngineAPI";
        std::string api =
            "<Project Sdk=\"Microsoft.NET.Sdk\"><PropertyGroup>"
            "<TargetFramework>net8.0</TargetFramework><AssemblyName>TwoDog.API</AssemblyName>"
            "<ImplicitUsings>enable</ImplicitUsings><Nullable>enable</Nullable><AllowUnsafeBlocks>true</"
            "AllowUnsafeBlocks>"
            "<EnableDefaultItems>false</EnableDefaultItems><GenerateDocumentationFile>true</GenerateDocumentationFile>"
            "<NoWarn>$(NoWarn);1591</NoWarn></PropertyGroup><ItemGroup>";
        for (const auto &file : apiFiles) {
            const auto relative = file.lexically_relative(support / "API");
            save(apiDirectory / "Sources" / relative, bytes(file));
            api += "<Compile Include=\"" + itemPath(utf(std::filesystem::path("Sources") / relative)) + "\"><Link>" +
                   itemPath(utf(relative)) + "</Link></Compile>";
        }
        api += "</ItemGroup></Project>";
        save(apiDirectory / "TwoDog.API.csproj", api);
        std::string items;
        auto append = [&](const std::filesystem::path &file, std::string link) {
            std::replace(link.begin(), link.end(), '/', '\\');
            items += "<Compile Include=\"" + itemPath(utf(std::filesystem::absolute(file))) + "\"><Link>" +
                     itemPath(link) + "</Link><Visible>true</Visible></Compile>\n";
        };
        for (const auto &file : sources(root))
            append(file, utf(file.lexically_relative(root)));
        if (includeDemo)
            for (const auto &file : sources(work / "Builtin"))
                append(file, "Engine Demo/" + utf(file.lexically_relative(work / "Builtin")));
        const std::string common =
            "<AssemblyName>GameScripts</AssemblyName><RootNamespace>GameScripts</RootNamespace>"
            "<ImplicitUsings>enable</ImplicitUsings><Nullable>enable</Nullable><LangVersion>12.0</LangVersion>"
            "<EnableDefaultItems>false</EnableDefaultItems><DebugType>portable</DebugType>";
        std::string ideProjectXml =
            "<Project Sdk=\"Microsoft.NET.Sdk\"><PropertyGroup><TargetFramework>net8.0</TargetFramework>" + common +
            "</PropertyGroup><ItemGroup>" + items +
            "<None Include=\"../project.json\" Link=\"project.json\" />"
            "<None Include=\"../assets/**/*\" "
            "Exclude=\"../assets/**/*.cs;../assets/**/obj/**;../assets/**/bin/**;../assets/**/build/**;../assets/**/"
            ".twodog/**;../assets/**/.vs/**;../assets/**/.git/**\">"
            "<Link>assets/%(RecursiveDir)%(Filename)%(Extension)</Link></None>"
            "<ProjectReference Include=\"EngineAPI/TwoDog.API.csproj\"><Private>false</Private></ProjectReference>"
            "</ItemGroup></Project>";
        save(project, ideProjectXml);
        const std::string runtime =
            "<Project Sdk=\"Microsoft.NET.Sdk\"><PropertyGroup><TargetFramework>net10.0</TargetFramework>" + common +
            "</PropertyGroup><ItemGroup>" + items + "<Reference Include=\"TwoDog.API\"><HintPath>" +
            itemPath(utf(managed / "TwoDog.API.dll")) +
            "</HintPath><Private>false</Private></Reference></ItemGroup></Project>";
        save(runtimeProject, runtime);
    }
    std::string signature() const {
        std::uint64_t hash = 14695981039346656037ULL;
        auto add = [&](const std::string &v) {
            for (unsigned char c : v) {
                hash ^= c;
                hash *= 1099511628211ULL;
            }
        };
        for (const auto &folder : {root, work / "Builtin"})
            for (const auto &p : sources(folder)) {
                add(utf(p.lexically_relative(root)));
                add(bytes(p));
            }
        add(bytes(project));
        add(bytes(runtimeProject));
        for (const auto &file : sources(support / "API")) {
            add(utf(file));
            add(bytes(file));
        }
        add(configuration);
        if (std::filesystem::exists(managed / "TwoDog.API.dll"))
            add(bytes(managed / "TwoDog.API.dll"));
        return std::to_string(hash);
    }
    void publish() {
        const auto stage = runtimeProject.parent_path() / "build" / configuration;
        // The runtime loads DLL/PDB from memory. Publish DLL last; failed builds never touch runtime files.
        for (const char *name : {"GameScripts.pdb", "GameScripts.deps.json"})
            if (std::filesystem::exists(stage / name))
                std::filesystem::copy_file(stage / name, dll.parent_path() / name,
                                           std::filesystem::copy_options::overwrite_existing);
        auto temporary = dll;
        temporary += ".new";
        std::filesystem::copy_file(stage / "GameScripts.dll", temporary,
                                   std::filesystem::copy_options::overwrite_existing);
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), dll.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot publish C# assembly; previous DLL retained");
#else
        std::filesystem::rename(temporary, dll);
#endif
        write(work / "last-success.txt", building);
    }
};
ScriptWorkspace::ScriptWorkspace(const std::filesystem::path &r, const std::filesystem::path &m,
                                 const std::filesystem::path &s)
    : impl_(std::make_unique<Impl>(r, m, s)) {}
ScriptWorkspace::~ScriptWorkspace() = default;
bool ScriptWorkspace::ready() const {
    return !impl_->pending && !impl_->failed && !impl_->compiler.running() && std::filesystem::exists(impl_->dll);
}
bool ScriptWorkspace::busy() const { return impl_->pending || impl_->compiler.running(); }
const std::string &ScriptWorkspace::status() const { return impl_->status; }
std::filesystem::path ScriptWorkspace::assembly() const { return impl_->dll; }
void ScriptWorkspace::refreshIDE() { impl_->syncProject(); }
void ScriptWorkspace::rebuild() {
    impl_->pending = true;
    impl_->failed = false;
    impl_->changedAt = 0;
    impl_->status = "C# build queued...";
}
void ScriptWorkspace::tick(double now, bool forceScan) {
    auto &s = *impl_;
    try {
        if (auto code = s.ide.poll()) {
            if (*code != 0) {
                std::cerr << "[C# IDE] " << bytes(s.log.parent_path() / "ide.log") << '\n';
                s.status = "Cannot open Visual Studio: see Console";
            }
        }
        if (forceScan || now >= s.scanAt) {
            s.scanAt = now + .4;
            s.syncProject();
            auto current = s.signature();
            if (current != s.observed) {
                s.observed = std::move(current);
                s.pending = true;
                s.failed = false;
                s.changedAt = now;
                s.status = "C# changes detected...";
            }
        }
        if (auto code = s.compiler.poll()) {
            std::cout << "[C# compiler]\n" << bytes(s.log) << '\n';
            s.syncProject();
            const auto current = s.signature();
            if (current != s.observed) {
                s.observed = current;
                s.pending = true;
                s.changedAt = now;
            }
            if (current != s.building) {
                s.pending = true;
                s.status = "Saved during build: queued latest sources";
            } else if (*code == 0) {
                s.publish();
                s.failed = false;
                s.status = "C# compiled - ready / automatic reload";
            } else {
                s.failed = true;
                s.status = "C# compilation failed - see Console; running code retained";
            }
        }
        if (s.pending && !s.compiler.running() && now - s.changedAt >= .8) {
            if (!std::filesystem::exists(s.managed / "TwoDog.API.dll"))
                throw std::runtime_error("TwoDog.API.dll missing; compile the engine first");
            s.pending = false;
            s.building = s.observed;
            s.status = "Compiling C# in background...";
            s.compiler.launch("dotnet.exe",
                              {"build", s.runtimeProject, "--configuration", s.configuration, "--output",
                               s.runtimeProject.parent_path() / "build" / s.configuration, "--nologo",
                               "--disable-build-servers", "-m:1", "-p:UseSharedCompilation=false"},
                              s.runtimeProject.parent_path(), s.log, true);
        }
    } catch (const std::exception &e) {
        s.pending = false;
        s.failed = true;
        s.status = e.what();
        std::cerr << "[C#] " << e.what() << '\n';
    }
}
std::vector<std::string> ScriptWorkspace::types(const std::filesystem::path &p) const { return scriptTypes(p); }
std::filesystem::path ScriptWorkspace::create(const std::filesystem::path &folder, const std::string &name) {
    if (!std::regex_match(name, std::regex("[A-Z][A-Za-z0-9_]*")))
        throw std::runtime_error("Script name must start with A-Z and contain only letters, digits or underscore");
    const auto path =
        containedPath(impl_->root / "assets", folder.lexically_relative(impl_->root / "assets") / (name + ".cs"));
    if (std::filesystem::exists(path))
        throw std::runtime_error("A file with that name already exists");
    for (const auto &dir : {impl_->root, impl_->work / "Builtin"})
        for (const auto &p : sources(dir))
            for (const auto &type : scriptTypes(p))
                if (type == "GameScripts." + name)
                    throw std::runtime_error("A Behaviour with that name already exists");
    std::filesystem::create_directories(path.parent_path());
    write(path, "using TwoDog;\n\nnamespace GameScripts;\n\npublic sealed class " + name +
                    " : Behaviour\n{\n    public override void OnCreate()\n    {\n        Debug.Log(\"" + name +
                    " ready\");\n    }\n\n    public override void OnUpdate(float deltaTime)\n    {\n        // Game "
                    "logic runs here while Play is active.\n    }\n\n    public override void OnFixedUpdate(float "
                    "fixedDeltaTime)\n    {\n        // Physics logic goes here.\n    }\n}\n");
    impl_->syncProject();
    impl_->observed = impl_->signature();
    impl_->changedAt = 0;
    impl_->pending = true;
    impl_->failed = false;
    impl_->status = "C# script created - build queued...";
    return path;
}
void ScriptWorkspace::open(const std::filesystem::path &file) {
    auto &s = *impl_;
    if (s.ide.running())
        return;
    const auto path = containedPath(s.root, file.lexically_relative(s.root));
    s.syncProject();
    s.ide.launch("powershell.exe",
                 {"-NoProfile", "-STA", "-ExecutionPolicy", "Bypass", "-File", s.support / "open-script.ps1",
                  "-Solution", s.work / "GameScripts.sln", "-Source", path, "-CheckWorkspace"},
                 s.root, s.log.parent_path() / "ide.log", false);
}
} // namespace twodog
