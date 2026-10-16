// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEntityInteractionComponent.h"

#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Interaction/ArcInteractionContextData.h"
#include "Interaction/ArcInteractionDisplayData.h"
#include "MassAgentComponent.h"
#include "Mass/EntityManager.h"
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

bool UArcEntityInteractionComponent::ExecuteEntityInteraction(FMassEntityHandle Entity, const FArcInteractionContextData& ContextData, FGameplayTag OptionTag, int32 SlotIndex)
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

	// Resolve which slot to claim: use SlotIndex when provided, default to 0 for legacy path
	const int32 ClaimIndex = (SlotIndex == INDEX_NONE) ? 0 : SlotIndex;
	if (!Slots.IsValidIndex(ClaimIndex))
	{
		UE_LOG(LogGameplayInteractions, Warning, TEXT("ExecuteEntityInteraction: SlotIndex %d is out of range (%d slots available)."), ClaimIndex, Slots.Num());
		return false;
	}

	FSmartObjectClaimHandle ClaimHandle = SOSubsystem->MarkSlotAsClaimed(Slots[ClaimIndex], ESmartObjectClaimPriority::High);
	if (!ClaimHandle.IsValid())
	{
		return false;
	}

	// Get the default behavior definition for this slot
	const UArcInteractGameplayInteractionSmartObjectBehaviorDefinition* DefaultBehaviorDef =
		SOSubsystem->GetBehaviorDefinition<UArcInteractGameplayInteractionSmartObjectBehaviorDefinition>(ClaimHandle);
	if (!DefaultBehaviorDef)
	{
		SOSubsystem->Release(ClaimHandle);
		return false;
	}

	// Resolve which behavior to activate: default unless an OptionTag selects an override
	const UArcGameplayInteractionSmartObjectBehaviorDefinition* BehaviorToActivate = DefaultBehaviorDef;

	if (OptionTag.IsValid())
	{
		// Walk the SO definition to find the matching option on the claimed slot
		const FArcSmartObjectDefinitionSharedFragment* SODef = EM.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>(Entity);
		if (SODef && SODef->SmartObjectDefinition)
		{
			TConstArrayView<FSmartObjectSlotDefinition> SlotDefs = SODef->SmartObjectDefinition->GetSlots();
			if (SlotDefs.IsValidIndex(ClaimIndex))
			{
				const FSmartObjectSlotDefinition& SlotDef = SlotDefs[ClaimIndex];
				const FArcInteractionDisplayData* DisplayData = SlotDef.GetDefinitionDataPtr<FArcInteractionDisplayData>();
				if (DisplayData)
				{
					const FArcInteractionOption* MatchedOption = nullptr;
					for (const FArcInteractionOption& Option : DisplayData->Options)
					{
						if (Option.OptionTag == OptionTag)
						{
							MatchedOption = &Option;
							break;
						}
					}

					if (MatchedOption == nullptr)
					{
						UE_LOG(LogGameplayInteractions, Warning,
							TEXT("ExecuteEntityInteraction: OptionTag '%s' not found in slot %d display data — interaction aborted."),
							*OptionTag.ToString(), ClaimIndex);
						SOSubsystem->Release(ClaimHandle);
						return false;
					}

					const UArcGameplayInteractionSmartObjectBehaviorDefinition* OverrideDef =
						Cast<UArcGameplayInteractionSmartObjectBehaviorDefinition>(MatchedOption->OverrideBehavior);
					if (OverrideDef)
					{
						BehaviorToActivate = OverrideDef;
					}
				}
			}
		}
	}

	TOptional<FTransform> SlotTransform = SOSubsystem->GetSlotTransform(ClaimHandle);
	if (SlotTransform.IsSet())
	{
		GameplayInteractionContext.SetSlotTransform(SlotTransform.GetValue());
	}

	// Setup and activate the interaction context
	GameplayInteractionContext.SetSmartObjectEntity(Entity);
	GameplayInteractionContext.SetContextActor(InteractorPawn);
	if (const UMassAgentComponent* InteractorAgent = InteractorPawn->FindComponentByClass<UMassAgentComponent>())
	{
		const FMassEntityHandle InteractorEntity = InteractorAgent->GetEntityHandle();
		if (InteractorEntity.IsValid())
		{
			GameplayInteractionContext.SetContextEntity(InteractorEntity);
		}
	}
	GameplayInteractionContext.SetClaimedHandle(ClaimHandle);
	GameplayInteractionContext.SetOptionTag(OptionTag);

	if (!GameplayInteractionContext.Activate(*BehaviorToActivate))
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
