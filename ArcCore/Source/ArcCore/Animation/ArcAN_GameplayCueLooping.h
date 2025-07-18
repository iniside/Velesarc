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


#include "GameplayCueInterface.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcAN_GameplayCueLooping.generated.h"

USTRUCT(BlueprintType)
struct ARCCORE_API FArcItemFragment_AnimMontageGameplayCue : public FArcItemFragment
{
	GENERATED_BODY()
	
public:
	//UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Client", ForceInlineRow))
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayCueTag Cue;
};


/**
 * 
 */
UCLASS()
class ARCCORE_API UArcAN_GameplayCueLoopingStart : public UAnimNotify
{
	GENERATED_BODY()

public:

	UArcAN_GameplayCueLoopingStart() = default;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	FString GetNotifyName_Implementation() const override;


protected:

	// GameplayCue tag to invoke.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameplayCue, meta = (Categories = "GameplayCue"))
	FGameplayCueTag GameplayCue;
};

UCLASS()
class ARCCORE_API UArcAN_GameplayCueLoopingFinish : public UAnimNotify
{
	GENERATED_BODY()

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	FString GetNotifyName_Implementation() const override;

protected:

	// GameplayCue tag to invoke.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameplayCue, meta = (Categories = "GameplayCue"))
	FGameplayCueTag GameplayCue;
};
