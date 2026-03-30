// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEntityInteractionComponent.h"

#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Interaction/ArcInteractionContextData.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "SmartObjectSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEntityInteractionComponent)

UArcEntityInteractionComponent::UArcEntityInteractionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UArcEntityInteractionComponent::ExecuteEntityInteraction(FMassEntityHandle Entity, const FArcInteractionContextData& ContextData)
{
	APawn* InteractorPawn = ContextData.InteractorPawn.Get();
	if (!InteractorPawn || !Entity.IsValid())
	{
		return false;
	}

	UMassEntitySubsystem* EntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return false;
	}

	const FMassEntityManager& EM = EntitySubsystem->GetEntityManager();

	// Get smart object handle from the entity's fragment
	const FArcSmartObjectOwnerFragment* SOFragment = EM.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Entity);
	if (!SOFragment || !SOFragment->SmartObjectHandle.IsValid())
	{
		return false;
	}

	USmartObjectSubsystem* SOSubsystem = GetWorld()->GetSubsystem<USmartObjectSubsystem>();
	if (!SOSubsystem)
	{
		return false;
	}

	// Get available slots
	TArray<FSmartObjectSlotHandle> Slots;
	SOSubsystem->GetAllSlots(SOFragment->SmartObjectHandle, Slots);
	if (Slots.IsEmpty())
	{
		return false;
	}

	// Claim the first slot
	FSmartObjectClaimHandle ClaimHandle = SOSubsystem->MarkSlotAsClaimed(Slots[0], ESmartObjectClaimPriority::High);
	if (!ClaimHandle.IsValid())
	{
		return false;
	}

	// Get the behavior definition
	const UArcInteractGameplayInteractionSmartObjectBehaviorDefinition* BehaviorDef =
		SOSubsystem->GetBehaviorDefinition<UArcInteractGameplayInteractionSmartObjectBehaviorDefinition>(ClaimHandle);
	if (!BehaviorDef)
	{
		SOSubsystem->Release(ClaimHandle);
		return false;
	}

	TOptional<FTransform> SlotTransform = SOSubsystem->GetSlotTransform(ClaimHandle);
	if (SlotTransform.IsSet())
	{
		GameplayInteractionContext.SetSlotTransform(SlotTransform.GetValue());
	}

	// Setup and activate the interaction context
	GameplayInteractionContext.SetSmartObjectEntity(Entity);
	GameplayInteractionContext.SetContextActor(InteractorPawn);
	GameplayInteractionContext.SetClaimedHandle(ClaimHandle);

	if (!GameplayInteractionContext.Activate(*BehaviorDef))
	{
		SOSubsystem->Release(ClaimHandle);
		return false;
	}

	// Tick the interaction until completion
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this,
			[this, ClaimHandle, SOSubsystem](float DeltaTime)
			{
				const bool bKeepTicking = GameplayInteractionContext.Tick(DeltaTime);
				if (!bKeepTicking)
				{
					SOSubsystem->Release(ClaimHandle);
					return false;
				}
				return true;
			}));

	return true;
}

bool UArcEntityInteractionComponent::IsInteracting() const
{
	return GameplayInteractionContext.GetLastRunStatus() == EStateTreeRunStatus::Running;
}
