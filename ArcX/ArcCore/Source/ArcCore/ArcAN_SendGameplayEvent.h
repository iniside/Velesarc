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


#include "GameplayTagContainer.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "UObject/Object.h"
#include "ArcAN_SendGameplayEvent.generated.h"

/**
 * 
 */
UCLASS(const, hidecategories = Object, collapsecategories)
class ARCCORE_API UArcAN_SendGameplayEvent : public UAnimNotify
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(EditAnywhere
		, Category = "Config")
	FGameplayTag EventTag;

public:
	// Begin UAnimNotify interface
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(USkeletalMeshComponent* MeshComp
						, UAnimSequenceBase* Animation
						, const FAnimNotifyEventReference& EventReference) override;

	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload) override;
#if WITH_EDITOR
	virtual void ValidateAssociatedAssets() override;
#endif
};

/**
 *  Notify used to mark gameplay events on montage, which are then manually executed  in ability task.
 */
UCLASS(const, hidecategories = Object, collapsecategories)
class ARCCORE_API UArcAnimNotify_MarkGameplayEvent : public UAnimNotify
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(EditAnywhere, Category = "Config")
	FGameplayTag EventTag;

public:
	const FGameplayTag& GetEventTag() const
	{
		return EventTag;
	}
	// Begin UAnimNotify interface
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(USkeletalMeshComponent* MeshComp
						, UAnimSequenceBase* Animation
						, const FAnimNotifyEventReference& EventReference) override;

	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload) override;
#if WITH_EDITOR
	virtual void ValidateAssociatedAssets() override;
#endif
};

UCLASS(const, hidecategories = Object, collapsecategories)
class ARCCORE_API UArcAnimNotify_SetPlayRate : public UAnimNotify
{
	GENERATED_UCLASS_BODY()

private:
	UPROPERTY(EditAnywhere, Category = "Config")
	float NewPlayRate;

public:
	float GetNewPlayRate() const
	{
		return NewPlayRate;
	}
	// Begin UAnimNotify interface
	virtual FString GetNotifyName_Implementation() const override;

	virtual void Notify(USkeletalMeshComponent* MeshComp
						, UAnimSequenceBase* Animation
						, const FAnimNotifyEventReference& EventReference) override;

	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& BranchingPointPayload) override;
#if WITH_EDITOR
	virtual void ValidateAssociatedAssets() override;
#endif
};