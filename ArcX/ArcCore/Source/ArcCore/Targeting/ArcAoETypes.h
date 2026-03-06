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
};
