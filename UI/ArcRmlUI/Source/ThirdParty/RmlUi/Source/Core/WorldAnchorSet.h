#pragma once

#include "../../Include/RmlUi/Core/Types.h"
#include "../../Include/RmlUi/Core/WorldAnchors.h"

namespace Rml {

class Context;
class DataViewWorld;

class WorldAnchorSet : NonCopyMoveable {
public:
	static WorldAnchorSet& Get(Context& context);

	void SetProjector(WorldProjector in_projector);
	bool Project(const WorldPoint& world_point, Vector2f& out_context_point) const;

	void Add(DataViewWorld* view);
	void Remove(DataViewWorld* view);

	bool Reproject();

private:
	WorldProjector projector;
	Vector<DataViewWorld*> anchors;
};

} // namespace Rml
