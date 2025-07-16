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

#include "WorldCollision.h"
#include "Containers/EnumAsByte.h"
#include "ArcInteractionSearch.generated.h"

class AActor;
class UArcInteractionReceiverComponent;
class IArcInteractionObjectInterface;
/**
 * 
 */
USTRUCT()
struct ARCCORE_API FArcInteractionSearch
{
	GENERATED_BODY()
public:
	virtual void BeginSearch(UArcInteractionReceiverComponent* InReceiverComponent) const {}

	virtual ~FArcInteractionSearch() = default;
};

USTRUCT()
struct ARCCORE_API FArcInteractionSearch_AsyncSphere : public FArcInteractionSearch
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditDefaultsOnly, Category = "Arc Core")
	TEnumAsByte<EObjectTypeQuery> InteractableObjectType = EObjectTypeQuery::ObjectTypeQuery_MAX;

	UPROPERTY(EditDefaultsOnly, Category = "Arc Core")
	double SearchRadius = 190;

	UPROPERTY(EditDefaultsOnly, Category = "Arc Core")
	float SearchInterval = 0.2;
	
	mutable FTraceDelegate InteractableSearchDelegate;
	
public:
	virtual void BeginSearch(UArcInteractionReceiverComponent* InReceiverComponent) const override;

	virtual ~FArcInteractionSearch_AsyncSphere() override = default;
	
private:
	void MakeTrace(UArcInteractionReceiverComponent* InReceiverComponent) const;
	void HandleTraceFinished(const FTraceHandle& TraceHandle, FTraceDatum& Datum, UArcInteractionReceiverComponent* InReceiverComponent) const;

	void FilterTargets(const TArray<TPair<FHitResult, TScriptInterface<IArcInteractionObjectInterface>>>& InLocations, UArcInteractionReceiverComponent* InReceiverComponent) const;
};