/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "CoreMinimal.h"
#include "ArcAoETypes.generated.h"

UENUM(BlueprintType)
enum class EArcAoEShape : uint8
{
	Sphere,
	Circle,
	Box
};

UENUM(BlueprintType)
enum class EArcAoEBoxAlignment : uint8
{
	/** Long edge faces the source (e.g., firewall). */
	LongEdgeFacingSource,
	/** Short edge faces the source (e.g., forward cone/attack). */
	ShortEdgeFacingSource
};

struct FArcItemData;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAoEShapeData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EArcAoEShape Shape = EArcAoEShape::Sphere;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Shape != EArcAoEShape::Box", EditConditionHides))
	float Radius = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Shape == EArcAoEShape::Box", EditConditionHides))
	FVector BoxExtent = FVector(100.0f, 50.0f, 50.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "Shape == EArcAoEShape::Box", EditConditionHides))
	EArcAoEBoxAlignment BoxAlignment = EArcAoEBoxAlignment::LongEdgeFacingSource;

	static FArcAoEShapeData FromItemData(const FArcItemData* ItemData);

	/**
	 * Compute box center and rotation from a direction vector (pitch zeroed).
	 * BoxExtent axes are already alignment-swapped by FromItemData, so no extra rotation is applied.
	 * @param SourceLocation  Origin point (e.g. hit location from line trace)
	 * @param Direction       Flat forward direction (typically source→target or eye yaw)
	 * @param OutCenter       Resulting box center (offset forward by BoxExtent.X)
	 * @param OutRotation     Yaw-only rotation matching the direction
	 */
	void ComputeBoxTransform(const FVector& SourceLocation, const FVector& Direction, FVector& OutCenter, FQuat& OutRotation) const;
};
