// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "ArcGameplayInteractionStateTreeBases.h"

#include "ArcGI_BindCraftingStationTask.generated.h"

class UArcCraftingViewModel;

USTRUCT()
struct FArcGI_BindCraftingStationTaskInstanceData
{
	GENERATED_BODY()

	/** Entity handle for the smart object (crafting station). Auto-bound by StateTree schema. */
	UPROPERTY(EditAnywhere, Category = "Context")
	FMassEntityHandle SmartObjectEntity;

	/** Entity handle for the claiming pawn. Auto-bound by StateTree schema. */
	UPROPERTY(EditAnywhere, Category = "Context")
	FMassEntityHandle ContextEntity;

	/** Tag used to look up the UArcCraftingViewModel on the PlayerState's MVVM component. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag CraftingViewModelTag;

	/** Cached VM pointer set during EnterState and used during ExitState. */
	UPROPERTY(VisibleAnywhere)
	TWeakObjectPtr<UArcCraftingViewModel> CachedVM;
};

USTRUCT(meta = (DisplayName = "Arc Bind Crafting Station", Category = "Gameplay Interactions"))
struct ARCGAMEPLAYINTERACTION_API FArcGI_BindCraftingStationTask : public FArcGameplayInteractionMassTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGI_BindCraftingStationTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
