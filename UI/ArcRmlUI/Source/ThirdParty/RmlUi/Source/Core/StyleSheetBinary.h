#pragma once

#include "../../Include/RmlUi/Core/ID.h"
#include "../../Include/RmlUi/Core/StyleSheetBinary.h"
#include "../../Include/RmlUi/Core/StyleSheetTypes.h"
#include "../../Include/RmlUi/Core/Types.h"

namespace Rml {

struct PropertySource;

struct StyleSheetDeclaration {
	String name;
	String value;
};
using StyleSheetDeclarationMap = UnorderedMap<PropertyId, StyleSheetDeclaration>;

struct StyleSheetCompileRecord {
	struct RuleEntry {
		SharedPtr<const PropertySource> source;
		SharedPtr<const StyleSheetDeclarationMap> declarations;
	};
	struct KeyframeEntry {
		float normalized_time;
		SharedPtr<const StyleSheetDeclarationMap> declarations;
	};

	UnorderedMap<const PropertySource*, RuleEntry> rules;
	UnorderedMap<const KeyframesMap*, UnorderedMap<String, Vector<KeyframeEntry>>> keyframes;
};

namespace StyleSheetBinaryInternal {
	void Shutdown();
}

} // namespace Rml
