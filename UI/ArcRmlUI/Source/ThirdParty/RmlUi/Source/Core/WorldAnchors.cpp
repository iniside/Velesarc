#include "../../Include/RmlUi/Core/WorldAnchors.h"
#include "../../Include/RmlUi/Core/Context.h"
#include "DataViewDefault.h"
#include "WorldAnchorSet.h"
#include <algorithm>

namespace Rml {

WorldAnchorSet& WorldAnchorSet::Get(Context& context)
{
	if (!context.world_anchors)
		context.world_anchors = MakeUnique<WorldAnchorSet>();
	return *context.world_anchors;
}

void WorldAnchorSet::SetProjector(WorldProjector in_projector)
{
	projector = std::move(in_projector);
}

bool WorldAnchorSet::Project(const WorldPoint& world_point, Vector2f& out_context_point) const
{
	return projector && projector(world_point, out_context_point);
}

void WorldAnchorSet::Add(DataViewWorld* view)
{
	anchors.push_back(view);
}

void WorldAnchorSet::Remove(DataViewWorld* view)
{
	const Vector<DataViewWorld*>::iterator it = std::find(anchors.begin(), anchors.end(), view);
	if (it == anchors.end())
		return;

	*it = anchors.back();
	anchors.pop_back();
}

bool WorldAnchorSet::Reproject()
{
	bool changed = false;
	bool has_dead_anchors = false;
	for (DataViewWorld* view : anchors)
	{
		if (!view->IsValid() || view->IsRemoved())
		{
			has_dead_anchors = true;
			continue;
		}
		changed |= view->Place();
	}

	if (has_dead_anchors)
	{
		anchors.erase(std::remove_if(anchors.begin(), anchors.end(),
						  [](const DataViewWorld* view) -> bool { return !view->IsValid() || view->IsRemoved(); }),
			anchors.end());
	}

	return changed;
}

void SetWorldProjector(Context& context, WorldProjector projector)
{
	WorldAnchorSet::Get(context).SetProjector(std::move(projector));
}

bool ReprojectWorldAnchors(Context& context)
{
	return WorldAnchorSet::Get(context).Reproject();
}

} // namespace Rml
