// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_PushWidgetToLayer.h"

#include "CommonActivatableWidget.h"
#include "CommonUIExtensions.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "MassActorSubsystem.h"
#include "Mass/EntityManager.h"
#include "MassSignalSubsystem.h"
#include "StateTreeExecutionContext.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Signals/ArcAbilitySignals.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcAbilityPushWidget, Log, All);

namespace ArcAbilityTask_PushWidgetToLayer_Private
{
	static ULocalPlayer* ResolveLocalPlayer(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
	{
		if (!Entity.IsValid())
		{
			return nullptr;
		}

		FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
		if (!ActorFragment || !ActorFragment->IsValid())
		{
			return nullptr;
		}

		AActor* Actor = ActorFragment->GetMutable();
		if (!Actor)
		{
			return nullptr;
		}

		// Some entity configs stamp the PlayerState actor directly; otherwise the actor is a Pawn.
		APlayerState* PlayerState = Cast<APlayerState>(Actor);
		if (!PlayerState)
		{
			APawn* Pawn = Cast<APawn>(Actor);
			PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
		}

		if (!PlayerState)
		{
			return nullptr;
		}

		APlayerController* PlayerController = PlayerState->GetPlayerController();
		if (!PlayerController)
		{
			return nullptr;
		}

		return Cast<ULocalPlayer>(PlayerController->Player);
	}
} // namespace ArcAbilityTask_PushWidgetToLayer_Private

EStateTreeRunStatus FArcAbilityTask_PushWidgetToLayer::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	// Misconfigured asset — fail loudly.
	if (InstanceData.WidgetClass.IsNull())
	{
		UE_LOGF(LogArcAbilityPushWidget, Error,"EnterState: WidgetClass is not set — failing.");
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.UILayerTag.IsValid())
	{
		UE_LOGF(LogArcAbilityPushWidget, Error,"EnterState: UILayerTag is not set — failing.");
		return EStateTreeRunStatus::Failed;
	}

	// Widget-less path: dedicated server / remote proxy / actor not yet spawned / AI-owned
	// ability. Keep running without pushing — ending the state is the outer tree's job.
	ULocalPlayer* LocalPlayer = ArcAbilityTask_PushWidgetToLayer_Private::ResolveLocalPlayer(EntityManager, Entity);
	if (!LocalPlayer)
	{
		UE_LOGF(LogArcAbilityPushWidget, Verbose,"EnterState: no LocalPlayer for entity=[%d:%d] — skipping widget push.",
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Running;
	}

	UClass* LoadedClass = InstanceData.WidgetClass.Get();
	if (!LoadedClass)
	{
		LoadedClass = InstanceData.WidgetClass.LoadSynchronous();
	}

	if (!LoadedClass)
	{
		UE_LOGF(LogArcAbilityPushWidget, Error,"EnterState: failed to load WidgetClass — failing.");
		return EStateTreeRunStatus::Failed;
	}

	UCommonActivatableWidget* Pushed =
		UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer, InstanceData.UILayerTag, LoadedClass);
	if (!Pushed)
	{
		UE_LOGF(LogArcAbilityPushWidget, Error,"EnterState: PushContentToLayer_ForPlayer returned null — failing.");
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.PushedWidget = Pushed;
	InstanceData.bWidgetPushed = true;

	// Wake the tree when the player closes the widget (back action, explicit close, layer pop).
	// The handler only signals — Tick reads the verdict. It must NOT touch instance data or
	// finish the task (mandated async-finish protocol; signal only from native callbacks).
	UWorld* World = AbilityContext.GetMassContext().GetWorld();
	TWeakObjectPtr<UMassSignalSubsystem> WeakSignals = UWorld::GetSubsystem<UMassSignalSubsystem>(World);

	InstanceData.OnWidgetDeactivatedHandle = Pushed->OnDeactivated().AddLambda(
		[WeakSignals, Entity]()
		{
			UMassSignalSubsystem* Signals = WeakSignals.Get();
			if (Signals && Entity.IsValid())
			{
				Signals->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, Entity);
			}
		});

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcAbilityTask_PushWidgetToLayer::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Only the widget path has a completion condition; without a pushed widget the task
	// runs until the outer tree ends the state.
	if (InstanceData.bWidgetPushed)
	{
		UCommonActivatableWidget* Widget = InstanceData.PushedWidget.Get();
		if (!Widget || !Widget->IsActivated())
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_PushWidgetToLayer::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Both exit paths converge here: user-close (widget already deactivated, Tick returned
	// Succeeded) and external abort (state aborted while the widget is still active). Unbind
	// FIRST so our own DeactivateWidget below cannot re-enter the handler.
	UCommonActivatableWidget* Widget = InstanceData.PushedWidget.Get();
	if (Widget)
	{
		if (InstanceData.OnWidgetDeactivatedHandle.IsValid())
		{
			Widget->OnDeactivated().Remove(InstanceData.OnWidgetDeactivatedHandle);
		}

		if (Widget->IsActivated())
		{
			Widget->DeactivateWidget();
		}
	}

	InstanceData.OnWidgetDeactivatedHandle.Reset();
	InstanceData.PushedWidget.Reset();
	InstanceData.bWidgetPushed = false;
}
