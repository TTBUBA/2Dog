#include <twodog/editor/EditorDefaults.hpp>
#include <twodog/data/ComponentSchema.hpp>
namespace twodog { Json defaultUI(const char* kind) { return ComponentSchema::uiDefaults(kind); } }
