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

#include "GameplayEffectComponent.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcGEC_TargetMassTags.generated.h"

struct FGameplayEffectRemovalInfo;

/** Adds Mass sparse element tags to the target actor's Mass entity while the effect is active. Removes them on effect removal. */
UCLASS(DisplayName="Grant Mass Tags to Target Entity")
class ARCCORE_API UArcGEC_TargetMassTags : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	virtual bool OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	void OnActiveGameplayEffectRemoved(const FGameplayEffectRemovalInfo& RemovalInfo) const;

	/** Mass tags to add as sparse elements to the target entity while this effect is active. */
	UPROPERTY(EditDefaultsOnly, Category = "Mass", meta = (BaseStruct = "/Script/MassEntity.MassSparseTag", ExcludeBaseStruct))
	TArray<FInstancedStruct> Tags;
};
