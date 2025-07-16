/**
 * This file is part of ArcX.
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

#include "ArcPenetrationAvoidanceFeeler.generated.h"

/**
 * Struct defining a feeler ray used for camera penetration avoidance.
 */
USTRUCT()
struct FArcPenetrationAvoidanceFeeler
{
	GENERATED_BODY()

	/** FRotator describing deviance from main ray */
	UPROPERTY(EditAnywhere
		, Category = PenetrationAvoidanceFeeler)
	FRotator AdjustmentRot;

	/** how much this feeler affects the final position if it hits the world */
	UPROPERTY(EditAnywhere
		, Category = PenetrationAvoidanceFeeler)
	float WorldWeight;

	/** how much this feeler affects the final position if it hits a APawn (setting to 0
	 * will not attempt to collide with pawns at all) */
	UPROPERTY(EditAnywhere
		, Category = PenetrationAvoidanceFeeler)
	float PawnWeight;

	/** extent to use for collision when tracing this feeler */
	UPROPERTY(EditAnywhere
		, Category = PenetrationAvoidanceFeeler)
	float Extent;

	/** minimum frame interval between traces with this feeler if nothing was hit last
	 * frame */
	UPROPERTY(EditAnywhere
		, Category = PenetrationAvoidanceFeeler)
	int32 TraceInterval;

	/** number of frames since this feeler was used */
	UPROPERTY(transient)
	int32 FramesUntilNextTrace;

	FArcPenetrationAvoidanceFeeler()
		: AdjustmentRot(ForceInit)
		, WorldWeight(0)
		, PawnWeight(0)
		, Extent(0)
		, TraceInterval(0)
		, FramesUntilNextTrace(0)
	{
	}

	FArcPenetrationAvoidanceFeeler(const FRotator& InAdjustmentRot
								   , const float& InWorldWeight
								   , const float& InPawnWeight
								   , const float& InExtent
								   , const int32& InTraceInterval = 0
								   , const int32& InFramesUntilNextTrace = 0)
		: AdjustmentRot(InAdjustmentRot)
		, WorldWeight(InWorldWeight)
		, PawnWeight(InPawnWeight)
		, Extent(InExtent)
		, TraceInterval(InTraceInterval)
		, FramesUntilNextTrace(InFramesUntilNextTrace)
	{
	}
};
