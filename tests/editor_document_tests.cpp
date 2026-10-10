#include <iostream>
#include <stdexcept>
#include <twodog/data/Project.hpp>
#include <twodog/editor/SceneDocument.hpp>
using namespace twodog;
namespace {
void expect(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void rejects(F fn) {
    bool rejected = false;
    try {
        fn();
    } catch (const std::exception &) {
        rejected = true;
    }
    expect(rejected, "Expected rejection");
}
void run(const std::string &test, const std::filesystem::path &root) {
    Project project(root);
    SceneDocument scene;
    scene.load(project.scene("scene.demo"));
    scene.select("player");
    const auto original = scene.document().dump();
    if (test == "history_selection") {
        scene.duplicateSelected();
        const auto duplicate = scene.selected();
        expect(duplicate != "player" && scene.dirty(), "Duplicate must select new object");
        scene.history(true);
        expect(scene.selected() == "player" && scene.document().dump() == original && !scene.dirty(),
               "Undo must restore selection and saved state");
        scene.history(false);
        expect(scene.selected() == duplicate && scene.dirty(), "Redo must restore duplicate selection");
    } else if (test == "transaction") {
        auto bad = scene.document();
        bad["entities"].array().push_back(bad.at("entities").array().front());
        rejects([&] { scene.commit(bad); });
        expect(scene.document().dump() == original && !scene.canUndo() && scene.selected() == "player",
               "Rejected commit changed state");
        scene.configure([](const Json &) { throw std::runtime_error("Invalid external dependency"); }, {});
        rejects([&] { scene.duplicateSelected(); });
        rejects([&] { scene.load(bad); });
        expect(scene.document().dump() == original && !scene.dirty(), "Validator failure changed scene");
    } else if (test == "branch_and_saved") {
        scene.duplicateSelected();
        scene.markSaved();
        scene.history(true);
        expect(scene.dirty() && scene.canRedo(), "Undo from saved state must be dirty");
        scene.duplicateSelected();
        expect(!scene.canRedo(), "New edit must discard redo");
        scene.markSaved();
        expect(!scene.dirty(), "Save must clear dirty flag");
        scene.commit(scene.document());
        scene.history(true);
        expect(scene.selected() == "player", "No-op added undo entry");
    } else if (test == "script") {
        scene.attachScript("player", "Game.NewBehaviour");
        expect(scene.dirty(), "Attaching script must dirty scene");
        const auto attached = scene.document().dump();
        rejects([&] { scene.attachScript("player", "Game.NewBehaviour"); });
        rejects([&] { scene.attachScript("player", ""); });
        expect(scene.document().dump() == attached, "Rejected script altered scene");
        scene.history(true);
        expect(scene.document().dump() == original, "Script undo failed");
        scene.history(false);
        expect(scene.document().dump() == attached, "Script redo failed");
    } else if (test == "play_lock") {
        scene.duplicateSelected();
        const auto before = scene.document().dump();
        scene.configure({}, [] { return false; });
        rejects([&] { scene.history(true); });
        rejects([&] { scene.deleteSelected(); });
        rejects([&] { scene.attachScript(scene.selected(), "Game.Test"); });
        rejects([&] { scene.commit(scene.document()); });
        rejects([&] { scene.load(scene.document()); });
        rejects([&] { scene.markSaved(); });
        rejects([&] { scene.duplicateSelected(); });
        expect(scene.document().dump() == before && scene.canUndo(), "Play lock changed authoring state");
    } else if (test == "replace_identity") {
        auto entity = *SceneDocument::find(scene.document(), "player");
        entity["name"] = "Renamed";
        scene.replaceEntity("player", entity);
        expect(scene.draft().text("name") == "Renamed", "Inspector draft not refreshed");
        entity["id"] = "different";
        rejects([&] { scene.replaceEntity("player", entity); });
        scene.history(true);
        expect(scene.document().dump() == original, "Property undo failed");
    } else if (test == "delete_references") {
        auto doc = scene.document();
        doc["camera"]["follow"] = "player";
        doc["joints"] = Json::Array{Json::Object{{"a", "player"}, {"b", "ground"}}};
        scene.load(doc);
        scene.select("player");
        scene.deleteSelected();
        expect(!SceneDocument::find(scene.document(), "player"), "Object not deleted");
        expect(scene.document().at("joints").array().empty(), "Dangling joint");
        expect(scene.document().at("camera").text("follow").empty(), "Dangling camera target");
        scene.history(true);
        expect(scene.selected() == "player" && scene.document().dump() == doc.dump(), "Delete undo failed");
    } else if (test == "history_limit") {
        for (int i = 0; i < 70; ++i) {
            auto entity = *SceneDocument::find(scene.document(), "player");
            entity["name"] = std::to_string(i);
            scene.replaceEntity("player", entity);
        }
        int count = 0;
        while (scene.canUndo()) {
            scene.history(true);
            ++count;
        }
        expect(count == 64, "History must remain bounded");
    } else
        throw std::runtime_error("Unknown test");
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 3)
            throw std::runtime_error("Expected case and sample root");
        run(argv[1], argv[2]);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
