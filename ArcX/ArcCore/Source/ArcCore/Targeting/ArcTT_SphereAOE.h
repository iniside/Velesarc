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

#include "Tasks/TargetingTask.h"

#include "GameplayTagContainer.h"
#include "ScalableFloat.h"

#include "ArcTT_SphereAOE.generated.h"
/**
 * 
 */
UCLASS()
class ARCCORE_API UArcTT_SphereAOE : public UTargetingTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Config")
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<AActor> ActorClass;

	UPROPERTY(EditAnywhere, Category = "Config")
	FGameplayTag GlobalTargetingSource;
	
	UPROPERTY(EditAnywhere, Category = "Config")
	FScalableFloat Radius;
	
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};
