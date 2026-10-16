// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcGI_BindCraftingStationTask.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "MassActorSubsystem.h"
#include "MassEntitySubsystem.h"
#include "StateTreeExecutionContext.h"
#include "ArcFrontendMVVMModelComponent.h"
#include "ViewModels/ArcCraftingViewModel.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcGIBindCraftingStation, Log, All);

namespace ArcGI_BindCraftingStation
{
	static APlayerState* ResolvePlayerState(FMassEntityManager& EntityManager, FMassEntityHandle ContextEntity)
	{
		if (!ContextEntity.IsValid())
		{
			return nullptr;
		}

		const FMassActorFragment* ActorFrag = EntityManager.GetFragmentDataPtr<FMassActorFragment>(ContextEntity);
		if (!ActorFrag)
		{
			return nullptr;
		}

		const AActor* Actor = ActorFrag->Get();
		if (!Actor)
		{
			return nullptr;
		}

		// Direct PlayerState case (some entity configs stamp the PS actor directly)
		APlayerState* PS = const_cast<APlayerState*>(Cast<APlayerState>(Actor));
		if (PS)
		{
			return PS;
		}

		// Standard case: actor is a Pawn
		APawn* Pawn = const_cast<APawn*>(Cast<APawn>(Actor));
		return Pawn ? Pawn->GetPlayerState() : nullptr;
	}
} // namespace ArcGI_BindCraftingStation

EStateTreeRunStatus FArcGI_BindCraftingStationTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.SmartObjectEntity.IsValid())
	{
		UE_LOG(LogArcGIBindCraftingStation, Warning, TEXT("EnterState: SmartObjectEntity is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.ContextEntity.IsValid())
	{
		UE_LOG(LogArcGIBindCraftingStation, Warning, TEXT("EnterState: ContextEntity is not valid."));
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.CraftingViewModelTag.IsValid())
	{
		UE_LOG(LogArcGIBindCraftingStation, Warning, TEXT("EnterState: CraftingViewModelTag is not set."));
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = Context.GetWorld();
	UMassEntitySubsystem* EntitySubsystem = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!EntitySubsystem)
	{
		UE_LOG(LogArcGIBindCraftingStation, Warning, TEXT("EnterState: No UMassEntitySubsystem found."));
		return EStateTreeRunStatus::Failed;
	}

	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();

	APlayerState* PS = ArcGI_BindCraftingStation::ResolvePlayerState(EntityManager, InstanceData.ContextEntity);
	if (!PS)
	{
		UE_LOG(LogArcGIBindCraftingStation, Warning, TEXT("EnterState: Could not resolve APlayerState from ContextEntity."));
		return EStateTreeRunStatus::Failed;
	}

	UArcFrontendMVVMModelComponent* MVVM = PS->FindComponentByClass<UArcFrontendMVVMModelComponent>();
	if (!MVVM)
	{
		UE_LOG(LogArcGIBindCraftingStation, Warning, TEXT("EnterState: PlayerState has no UArcFrontendMVVMModelComponent."));
		return EStateTreeRunStatus::Failed;
	}

	UMVVMViewModelBase* VMBase = nullptr;
	if (!MVVM->FindViewModel(InstanceData.CraftingViewModelTag, VMBase) || !VMBase)
	{
		UE_LOG(LogArcGIBindCraftingStation, Warning, TEXT("EnterState: Could not find ViewModel for tag '%s'."), *InstanceData.CraftingViewModelTag.ToString());
		return EStateTreeRunStatus::Failed;
	}

	UArcCraftingViewModel* VM = Cast<UArcCraftingViewModel>(VMBase);
	if (!VM)
	{
		UE_LOG(LogArcGIBindCraftingStation, Warning, TEXT("EnterState: ViewModel is not a UArcCraftingViewModel."));
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.CachedVM = VM;
	VM->SetBoundStation(InstanceData.SmartObjectEntity);

	return EStateTreeRunStatus::Running;
}

void FArcGI_BindCraftingStationTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// Restore the VM to the player's own entity.
	UArcCraftingViewModel* VM = InstanceData.CachedVM.Get();
	if (VM)
	{
		VM->SetBoundStation(VM->GetInstigatorEntity());
	}
}
