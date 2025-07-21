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

#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "GameplayTagContainer.h"

#include "ArcAnimNotifyState_ComboWindow.generated.h"

UCLASS()
class ARCCORE_API UArcAnimNotify_ComboEvent : public UAnimNotify
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	FGameplayTag EventTag;

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp
						, UAnimSequenceBase* Animation
						, const FAnimNotifyEventReference& EventReference) override;
};

UCLASS()
class ARCCORE_API UArcAnimNotify_ComboWindowStart : public UAnimNotify
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	FGameplayTag EventTag;

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp
						, UAnimSequenceBase* Animation
						, const FAnimNotifyEventReference& EventReference) override;
};

UCLASS()
class ARCCORE_API UArcAnimNotify_ComboWindowEnd : public UAnimNotify
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere
		, Category = "Arc Core")
	FGameplayTag EventTag;

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp
						, UAnimSequenceBase* Animation
						, const FAnimNotifyEventReference& EventReference) override;
};
