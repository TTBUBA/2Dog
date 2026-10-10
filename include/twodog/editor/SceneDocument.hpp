#pragma once
#include <deque>
#include <functional>
#include <optional>
#include <string>
#include <twodog/data/Json.hpp>
namespace twodog {
// Authoring state and transactions. No window, renderer, ImGui, or managed host.
class SceneDocument final {
  public:
    using Validator = std::function<void(const Json &)>;
    void configure(Validator validate, std::function<bool()> canEdit);
    const Json &document() const { return document_; }
    const std::string &selected() const { return selected_; }
    Json &draft() { return draft_; }
    bool dirty() const { return dirty_; }
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    void select(std::string id);
    void refreshSelection();
    void load(Json next);
    void markSaved();
    void commit(Json next, bool record = true, std::optional<std::string> selection = std::nullopt);
    void history(bool undo);
    void replaceEntity(const std::string &id, Json entity);
    void duplicateSelected();
    void deleteSelected();
    void attachScript(const std::string &id, const std::string &type);
    static Json scriptBinding(const std::string &type);
    static Json *find(Json &document, const std::string &id);
    static const Json *find(const Json &document, const std::string &id);

  private:
    struct Snapshot {
        Json document;
        std::string selected;
    };
    Json document_, draft_;
    std::string selected_, saved_;
    bool dirty_ = false;
    std::deque<Snapshot> undo_, redo_;
    Validator validate_;
    std::function<bool()> canEdit_;
    void requireEditable() const;
    void validate(const Json &next) const;
};
} // namespace twodog
