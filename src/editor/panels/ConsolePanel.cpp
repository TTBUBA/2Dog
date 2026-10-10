#include <algorithm>
#include <cmath>
#include <fstream>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <sstream>
#include <twodog/core/Utf8Path.hpp>
#include <twodog/editor/EditorActions.hpp>
#include <twodog/editor/EditorAssets.hpp>
#include <twodog/editor/EditorContext.hpp>
#include <twodog/editor/EditorDefaults.hpp>
#include <twodog/editor/EditorWidgets.hpp>
#include <twodog/editor/panels/ConsolePanel.hpp>
#include <utility>
namespace twodog {
void ConsolePanel::draw([[maybe_unused]] EditorContext &ctx) {
    ImGui::Begin("Console");
    ImGui::TextWrapped("%s", ctx.message_.c_str());
    if (ImGui::Button("Clear")) {
        std::lock_guard a(ctx.console_.out.mutex);
        std::lock_guard b(ctx.console_.err.mutex);
        ctx.console_.out.lines.clear();
        ctx.console_.err.lines.clear();
    }
    ImGui::BeginChild("Log", ImVec2(0, 0));
    {
        std::lock_guard lock(ctx.console_.out.mutex);
        for (const auto &line : ctx.console_.out.lines)
            ImGui::TextUnformatted(line.c_str());
    }
    {
        std::lock_guard lock(ctx.console_.err.mutex);
        for (const auto &line : ctx.console_.err.lines)
            ImGui::TextColored(ImVec4(1, .5F, .4F, 1), "%s", line.c_str());
    }
    ImGui::EndChild();
    ImGui::End();
}

} // namespace twodog
