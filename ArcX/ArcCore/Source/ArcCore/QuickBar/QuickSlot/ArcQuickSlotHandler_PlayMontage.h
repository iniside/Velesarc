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


#include "ArcQuickSlotHandler.h"

#include "ArcQuickSlotHandler_PlayMontage.generated.h"

class UAnimMontage;

USTRUCT(meta = (DisplayName = "Arc Play Montage"))
struct ARCCORE_API FArcQuickSlotHandler_PlayMontage : public FArcQuickSlotHandler
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UAnimMontage> SelectedMontageToPlay;

	UPROPERTY(EditAnywhere)
	TSoftObjectPtr<UAnimMontage> DeselectedMontageToPlay;

	UPROPERTY(EditAnywhere)
	bool bReliable = true;

public:
	virtual void OnSlotSelected(UArcCoreAbilitySystemComponent* InArcASC
								, UArcQuickBarComponent* QuickBar
								, const FArcItemData* InSlot
								, const FArcQuickBar* InQuickBar
								, const FArcQuickBarSlot* InQuickSlot) const override;

	virtual void OnSlotDeselected(UArcCoreAbilitySystemComponent* InArcASC
								  , UArcQuickBarComponent* QuickBar
								  , const FArcItemData* InSlot
								  , const FArcQuickBar* InQuickBar
								  , const FArcQuickBarSlot* InQuickSlot) const override;

	virtual ~FArcQuickSlotHandler_PlayMontage() override = default;
};
