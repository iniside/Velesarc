// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Curve/DynamicGraph2.h"
#include "Layout/ArcTownRoadArrangement.h"

namespace ArcTownRoadArrangement
{
	ARCTOWN_API const UE::Geometry::FDynamicGraph2d& GetGraph(const FArrangement& Arrangement);
}
