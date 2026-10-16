// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Targeting/ArcAoETypes.h"

struct FArcItemData;

namespace ArcAbilityKit
{
	/**
	 * Builds an FArcAoEShapeData from an item's targeting fragments
	 * (FArcItemFragment_TargetingShapeConfig + FArcScalableFloatItemFragment_TargetingShape).
	 *
	 * Item-aware bridge that lives in ArcAbilityKit so ArcCore stays item-agnostic on AoE math.
	 */
	ARCABILITYKIT_API FArcAoEShapeData MakeAoEShapeFromItemData(const FArcItemData* ItemData);
}
