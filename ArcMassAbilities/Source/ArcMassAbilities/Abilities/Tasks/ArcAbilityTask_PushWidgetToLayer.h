// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_PushWidgetToLayer.generated.h"

class UCommonActivatableWidget;

USTRUCT()
struct FArcAbilityTask_PushWidgetToLayerInstanceData
{
	GENERATED_BODY()

	/** Widget class to push onto the UI layer on EnterState. */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	TSoftClassPtr<UCommonActivatableWidget> WidgetClass;

	/** UI layer tag to push the widget onto (e.g. UI.Layer.GameMenu). */
	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag UILayerTag;

	/** Cached widget pushed to the layer; deactivated during ExitState. */
	UPROPERTY(VisibleAnywhere)
	TWeakObjectPtr<UCommonActivatableWidget> PushedWidget;

	/** True once a widget was actually pushed; gates the Tick completion check. */
	UPROPERTY(VisibleAnywhere)
	bool bWidgetPushed = false;

	/** Not reflected — valid only between EnterState and ExitState of one activation. */
	FDelegateHandle OnWidgetDeactivatedHandle;
};

/**
 * Pushes a UCommonActivatableWidget onto a gameplay-tag-selected UI layer on EnterState,
 * finishes (Succeeded) when the widget deactivates itself (player closes it), and
 * deactivates the widget on ExitState — with a double-deactivation guard so ExitState
 * can't re-deactivate a widget that already closed itself, and so our own DeactivateWidget()
 * in ExitState can't re-enter the deactivation handler.
 *
 * The local player is resolved from the ability context entity (GetEntity()): correct for
 * player-owned abilities. AI-owned abilities resolve no player and take the widget-less path
 * (EnterState returns Running without pushing; ending the state is the outer tree's job).
 */
USTRUCT(meta = (DisplayName = "Arc Push Widget To Layer", Category = "Ability|UI"))
struct ARCMASSABILITIES_API FArcAbilityTask_PushWidgetToLayer : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_PushWidgetToLayerInstanceData;

	FArcAbilityTask_PushWidgetToLayer()
	{
		// Signal-driven task: the OnDeactivated lambda fires AbilityStateTreeTickRequired
		// to wake the tree. Without bConsideredForScheduling = false, GetNextScheduledTick
		// short-circuits to MakeEveryFrames (bCachedRequestTick = bShouldCallTick &&
		// bConsideredForScheduling) and the tick processor re-signals every frame.
		bConsideredForScheduling = false;
	}

	virtual bool DrivesScheduledTick() const override { return true; }

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
