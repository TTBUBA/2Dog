#include <algorithm>
#include <set>
#include <stdexcept>
#include <twodog/data/Project.hpp>
#include <twodog/editor/SceneDocument.hpp>
#include <utility>
namespace twodog {
void SceneDocument::configure(Validator validate, std::function<bool()> canEdit) {
    validate_ = std::move(validate);
    canEdit_ = std::move(canEdit);
}
void SceneDocument::requireEditable() const {
    if (canEdit_ && !canEdit_())
        throw std::runtime_error("Stop Play before editing the scene");
}
void SceneDocument::validate(const Json &next) const {
    std::set<std::string> ids;
    for (const auto &entity : next.at("entities").array()) {
        const auto id = entity.at("id").string();
        if (id.empty() || !ids.insert(id).second)
            throw std::runtime_error("Entity IDs must be unique and nonempty");
        Project::validateEntity(entity);
    }
    if (validate_)
        validate_(next);
}
Json *SceneDocument::find(Json &document, const std::string &id) {
    for (auto &entity : document.at("entities").array())
        if (entity.at("id").string() == id)
            return &entity;
    return nullptr;
}
const Json *SceneDocument::find(const Json &document, const std::string &id) {
    for (const auto &entity : document.at("entities").array())
        if (entity.at("id").string() == id)
            return &entity;
    return nullptr;
}
void SceneDocument::select(std::string id) {
    selected_ = std::move(id);
    refreshSelection();
}
void SceneDocument::refreshSelection() {
    const auto *entity = document_.null() ? nullptr : find(document_, selected_);
    if (!entity)
        selected_.clear();
    draft_ = entity ? *entity : Json{};
}
void SceneDocument::load(Json next) {
    requireEditable();
    validate(next);
    document_ = std::move(next);
    saved_ = document_.dump();
    dirty_ = false;
    selected_.clear();
    draft_ = Json{};
    undo_.clear();
    redo_.clear();
}
void SceneDocument::markSaved() {
    requireEditable();
    saved_ = document_.dump();
    dirty_ = false;
}
void SceneDocument::commit(Json next, bool record, std::optional<std::string> selection) {
    requireEditable();
    const auto serialized = next.dump();
    if (serialized == document_.dump()) {
        if (selection)
            select(*selection);
        else
            refreshSelection();
        return;
    }
    validate(next); // Failure leaves document, selection, drafts and both stacks unchanged.
    if (record) {
        undo_.push_back({document_, selected_});
        if (undo_.size() > 64)
            undo_.pop_front();
        redo_.clear();
    }
    document_ = std::move(next);
    if (selection)
        selected_ = *selection;
    dirty_ = serialized != saved_;
    refreshSelection();
}
void SceneDocument::history(bool back) {
    requireEditable();
    auto &source = back ? undo_ : redo_;
    auto &destination = back ? redo_ : undo_;
    if (source.empty())
        return;
    auto next = source.back();
    validate(next.document);
    destination.push_back({document_, selected_});
    if (destination.size() > 64)
        destination.pop_front();
    source.pop_back();
    document_ = std::move(next.document);
    selected_ = std::move(next.selected);
    dirty_ = document_.dump() != saved_;
    refreshSelection();
}
void SceneDocument::replaceEntity(const std::string &id, Json replacement) {
    requireEditable();
    if (replacement.at("id").string() != id)
        throw std::runtime_error("Cannot change a persistent entity ID");
    auto next = document_;
    auto *entity = find(next, id);
    if (!entity)
        throw std::runtime_error("Select an existing object");
    *entity = std::move(replacement);
    commit(std::move(next));
}
void SceneDocument::duplicateSelected() {
    requireEditable();
    const auto *entity = find(document_, selected_);
    if (!entity)
        return;
    auto next = document_;
    auto copy = *entity;
    const auto id = newPersistentId();
    copy["id"] = id;
    copy["name"] = copy.text("name") + " Copy";
    auto transform = readTransform(copy.at("transform"));
    transform.position += glm::vec2(.5F);
    copy["transform"] = writeTransform(transform);
    next["entities"].array().push_back(std::move(copy));
    commit(std::move(next), true, id);
}
void SceneDocument::deleteSelected() {
    requireEditable();
    if (selected_.empty())
        return;
    auto next = document_;
    auto &entities = next["entities"].array();
    for (const auto &entity : entities)
        if (entity.contains("ui") && entity.at("ui").text("parent") == selected_)
            throw std::runtime_error("Reparent UI children before deleting their container");
    std::erase_if(entities, [&](const Json &entity) { return entity.at("id").string() == selected_; });
    if (next.contains("joints"))
        std::erase_if(next["joints"].array(),
                      [&](const Json &joint) { return joint.text("a") == selected_ || joint.text("b") == selected_; });
    if (next.contains("camera") && next.at("camera").text("follow") == selected_)
        next["camera"]["follow"] = "";
    commit(std::move(next), true, std::string{});
}
Json SceneDocument::scriptBinding(const std::string &type) {
    if (type.empty())
        throw std::runtime_error("C# Behaviour type cannot be empty");
    return Json::Object{
        {"slot", newPersistentId()}, {"language", "C#"}, {"type", type}, {"order", 0}, {"properties", Json::Object{}}};
}
void SceneDocument::attachScript(const std::string &id, const std::string &type) {
    requireEditable();
    auto next = document_;
    auto *entity = find(next, id);
    if (!entity)
        throw std::runtime_error("Select a game object");
    if (!entity->contains("scripts"))
        (*entity)["scripts"] = Json::Array{};
    for (const auto &script : entity->at("scripts").array())
        if (script.text("type") == type)
            throw std::runtime_error("This Behaviour is already assigned to the object");
    (*entity)["scripts"].array().push_back(scriptBinding(type));
    commit(std::move(next), true, id);
}
} // namespace twodog
