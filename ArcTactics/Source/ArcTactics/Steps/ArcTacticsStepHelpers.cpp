// Copyright Lukasz Baran. All Rights Reserved.

#include "Steps/ArcTacticsStepHelpers.h"

#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityStateTreeSubsystem.h"
#include "Events/ArcAbilityEventPayloads.h"
#include "Actions/ArcTacticsAction.h"
#include "Decision/ArcTacticsDecision.h"
#include "Engine/World.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Mass/EntityManager.h"
#include "MassCommonFragments.h"
#include "Navigation/ArcPathFollow.h"
#include "Steps/ArcTacticsUseAbilityStep.h"

namespace ArcTacticsStepHelpers
{
	bool ResolveTargetLocation(
		const FArcTacticsActionContext& Context,
		const FMassEntityHandle Target,
		const FVector& TargetLocation,
		const bool bUseTargetLocation,
		FVector& OutLocation)
	{
		if (bUseTargetLocation)
		{
			OutLocation = TargetLocation;
			return !OutLocation.ContainsNaN();
		}
		if (!Target.IsSet() || !Context.EntityManager || !Context.EntityManager->IsEntityActive(Target))
		{
			return false;
		}
		const FTransformFragment* Transform = Context.EntityManager->GetFragmentDataPtr<FTransformFragment>(Target);
		if (!Transform)
		{
			return false;
		}
		OutLocation = Transform->GetTransform().GetLocation();
		return true;
	}

	void ReleaseInput(FArcTacticsActionContext& Context, FArcTacticsUseAbilityStepInstanceData& Data)
	{
		if (Context.EntityManager && Data.bAbilityInputPending && Data.AbilityInputTag.IsValid())
		{
			ArcAbilities::ReleaseInput(*Context.EntityManager, Context.Entity, Data.AbilityInputTag);
		}
		Data.bAbilityInputPending = false;
		Data.AbilityInputTag = FGameplayTag();
	}

	void CancelExactAbility(FArcTacticsActionContext& Context, FArcTacticsUseAbilityStepInstanceData& Data)
	{
		ReleaseInput(Context, Data);
		if (!Context.EntityManager || !Context.AbilitySubsystem || !Data.AbilityHandle.IsValid())
		{
			return;
		}
		const FArcActiveAbility* Ability = ArcAbilities::FindAbility(
			*Context.EntityManager, Context.Entity, Data.AbilityHandle);
		if (Ability && Ability->bIsActive)
		{
			ArcAbilities::CancelAbility(
				*Context.EntityManager, Context.Entity, Data.AbilityHandle, *Context.AbilitySubsystem,
				EArcAbilityEndReason::Cancelled, Context.Entity);
		}
	}

	bool ApplyMovementPolicy(
		FArcTacticsActionContext& Context,
		FArcTacticsUseAbilityStepInstanceData& Data)
	{
		if (!Context.PathContext || !Context.HoldState)
		{
			return false;
		}
		return Data.Ability.MovementPolicy == EArcTacticsAbilityMovementPolicy::HoldPosition
			? ArcTacticsMovement::AcquireHold(*Context.PathContext, Data.MovementRuntime, *Context.HoldState)
			: ArcTacticsMovement::ReleaseHold(*Context.PathContext, Data.MovementRuntime, *Context.HoldState);
	}

	EArcTacticsActionStatus BeginAbility(
		FArcTacticsActionContext& Context,
		FArcTacticsUseAbilityStepInstanceData& Data,
		const float MaxRange,
		const uint8 WaitingStage,
		const uint8 UsingStage)
	{
		if (!Context.World || !Context.EntityManager)
		{
			return EArcTacticsActionStatus::Failed;
		}
		if ((Data.bUseTargetLocation || Data.Target.IsSet()) && MaxRange > 0.0f)
		{
			FVector TargetLocation = FVector::ZeroVector;
			const FTransformFragment* Transform =
				Context.EntityManager->GetFragmentDataPtr<FTransformFragment>(Context.Entity);
			if (!Transform
				|| !ResolveTargetLocation(Context, Data.Target, Data.TargetLocation, Data.bUseTargetLocation, TargetLocation)
				|| !ArcPathFollow::IsArrived(Transform->GetTransform().GetLocation(), TargetLocation, MaxRange))
			{
				return EArcTacticsActionStatus::Failed;
			}
		}
		FName RejectionReason;
		const FArcActiveAbility* Ability = ArcTacticsDecision::ResolveAbility(
			*Context.EntityManager, Context.Entity, Data.AbilityDefinition.Get(), true, RejectionReason);
		if (!Ability || !ArcAbilities::TryActivateAbility(*Context.EntityManager, Context.Entity, Ability->Handle))
		{
			return EArcTacticsActionStatus::Failed;
		}
		Data.AbilityHandle = Ability->Handle;
		Data.AbilityInputTag = Ability->InputTag;
		Data.bAbilityInputPending = true;
		Data.Stage = WaitingStage;
		Data.AbilityDeadline = Context.World->GetTimeSeconds() + FMath::Max(0.0f, Data.Ability.StartTimeout);
		Data.bAbilityDeadlineSet = true;
		if (Ability->bIsActive)
		{
			ReleaseInput(Context, Data);
			Data.Stage = UsingStage;
			Data.AbilityDeadline = Context.World->GetTimeSeconds() + FMath::Max(0.0f, Data.Ability.ExecutionTimeout);
		}
		return EArcTacticsActionStatus::Running;
	}

	EArcTacticsActionStatus UpdateAbility(
		FArcTacticsActionContext& Context,
		FArcTacticsUseAbilityStepInstanceData& Data,
		const uint8 WaitingStage,
		const uint8 UsingStage)
	{
		if (!Context.World || !Context.EntityManager)
		{
			return EArcTacticsActionStatus::Failed;
		}
		FName RejectionReason;
		const FArcActiveAbility* Ability = ArcTacticsDecision::ResolveAbility(
			*Context.EntityManager, Context.Entity, Data.AbilityDefinition.Get(), false, RejectionReason);
		if (!Ability || Ability->Handle != Data.AbilityHandle)
		{
			CancelExactAbility(Context, Data);
			return EArcTacticsActionStatus::Failed;
		}
		if (Ability->RunStatus == EStateTreeRunStatus::Succeeded)
		{
			ReleaseInput(Context, Data);
			return EArcTacticsActionStatus::Succeeded;
		}
		if (Ability->RunStatus == EStateTreeRunStatus::Failed
			|| Ability->RunStatus == EStateTreeRunStatus::Stopped)
		{
			ReleaseInput(Context, Data);
			return EArcTacticsActionStatus::Failed;
		}
		const double CurrentTime = Context.World->GetTimeSeconds();
		if (Data.Stage == WaitingStage)
		{
			if (Ability->bIsActive)
			{
				ReleaseInput(Context, Data);
				Data.Stage = UsingStage;
				Data.AbilityDeadline = CurrentTime + FMath::Max(0.0f, Data.Ability.ExecutionTimeout);
				Data.bAbilityDeadlineSet = true;
			}
			else if (Data.bAbilityDeadlineSet && CurrentTime >= Data.AbilityDeadline)
			{
				ReleaseInput(Context, Data);
				return EArcTacticsActionStatus::Failed;
			}
			return EArcTacticsActionStatus::Running;
		}
		if (Data.Stage == UsingStage)
		{
			ReleaseInput(Context, Data);
			if (!Ability->bIsActive)
			{
				return EArcTacticsActionStatus::Failed;
			}
			if (Data.bAbilityDeadlineSet && CurrentTime >= Data.AbilityDeadline)
			{
				CancelExactAbility(Context, Data);
				return EArcTacticsActionStatus::Failed;
			}
			return EArcTacticsActionStatus::Running;
		}
		return BeginAbility(Context, Data, 0.0f, WaitingStage, UsingStage);
	}

	EArcTacticsActionStatus TickAbility(
		FArcTacticsActionContext& Context,
		FArcTacticsUseAbilityStepInstanceData& Data,
		const uint8 AcquireHoldStage,
		const uint8 WaitingStage,
		const uint8 UsingStage)
	{
		if (Data.Stage == AcquireHoldStage)
		{
			if (!Context.PathContext)
			{
				return EArcTacticsActionStatus::Running;
			}
			if (!ApplyMovementPolicy(Context, Data))
			{
				return EArcTacticsActionStatus::Failed;
			}
			Data.Stage = WaitingStage;
			Context.RequestExecution.ExecuteIfBound();
			return EArcTacticsActionStatus::Running;
		}
		if (Data.Stage == WaitingStage && !Data.bAbilityDeadlineSet)
		{
			return BeginAbility(Context, Data, Data.MaxRange, WaitingStage, UsingStage);
		}
		return UpdateAbility(Context, Data, WaitingStage, UsingStage);
	}

	void ResetAbilityScratch(FArcTacticsUseAbilityStepInstanceData& Data)
	{
		Data.Stage = 0;
		Data.MovementRuntime = FArcTacticsMovementRuntimeData();
		Data.AbilityHandle = FArcAbilityHandle();
		Data.AbilityInputTag = FGameplayTag();
		Data.AbilityDeadline = 0.0;
		Data.bAbilityInputPending = false;
		Data.bAbilityDeadlineSet = false;
	}

	EArcTacticsActionStatus FinishAbilityStep(
		FArcTacticsActionContext& Context,
		FArcTacticsUseAbilityStepInstanceData& Data,
		const EArcTacticsActionStatus Status)
	{
		if (Status == EArcTacticsActionStatus::Succeeded)
		{
			ReleaseInput(Context, Data);
			ResetAbilityScratch(Data);
		}
		return Status;
	}
}
