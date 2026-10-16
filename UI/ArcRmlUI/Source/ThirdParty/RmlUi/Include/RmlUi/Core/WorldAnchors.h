#pragma once

#include "Header.h"
#include "Types.h"

namespace Rml {

class Context;

using WorldPoint = Vector3<double>;

/// Projects a world point to the context: writes the point in context device pixels and returns whether the point is in front of the camera.
using WorldProjector = Function<bool(const WorldPoint& world_point, Vector2f& out_context_point)>;

RMLUICORE_API void SetWorldProjector(Context& context, WorldProjector projector);

/// @return True if any 'data-world' element's translation or visibility changed.
RMLUICORE_API bool ReprojectWorldAnchors(Context& context);

} // namespace Rml
