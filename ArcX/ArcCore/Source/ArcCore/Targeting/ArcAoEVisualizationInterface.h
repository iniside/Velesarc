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
#include "UObject/Interface.h"
#include "ArcAoETypes.h"
#include "ArcAoEVisualizationInterface.generated.h"

class UGameplayAbility;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcAoEVisualizationContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AoE")
	FArcAoEShapeData ShapeData;

	UPROPERTY(BlueprintReadOnly, Category = "AoE")
	TObjectPtr<AActor> AvatarActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "AoE")
	TObjectPtr<UGameplayAbility> SourceAbility = nullptr;
};

UINTERFACE(MinimalAPI, BlueprintType)
class UArcAoEVisualizationInterface : public UInterface
{
	GENERATED_BODY()
};

class ARCCORE_API IArcAoEVisualizationInterface
{
	GENERATED_BODY()

public:
	/** Called once on spawn to initialize the AoE visualization with shape, avatar, and ability context. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Arc Core|AoE")
	void InitializeAoEShape(const FArcAoEVisualizationContext& Context);

	/** Called on targeting updates to update the visualization location and rotation. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Arc Core|AoE")
	void UpdateAoELocation(const FVector& Location, const FRotator& Rotation);
};
