// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectInteractionComponent.h"

#include "Interaction/ArcInteractionContextData.h"
#include "ArcMass/ArcMassSmartObjectFragments.h"
#include "InstancedActorsComponent.h"
#include "MassAgentComponent.h"
#include "MassEntityManager.h"
#include "SmartObjectSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSmartObjectInteractionComponent)

UArcSmartObjectInteractionComponent::UArcSmartObjectInteractionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UArcSmartObjectInteractionComponent::AppendTargetConfiguration(const FInteractionContext& Context, FInteractionQueryResults& OutResults) const
{
	for (const FInstancedStruct& Config : TargetConfigs)
	{
		OutResults.AvailableInteractions.Add(Config);
	}
}

void UArcSmartObjectInteractionComponent::BeginInteraction(const FInteractionContext& Context)
{
	APawn* Pawn = nullptr;

	// Prefer the rich context data populated by UArcGA_Interact
	if (const FArcInteractionContextData* ArcCtxData = Context.ContextData.GetPtr<FArcInteractionContextData>())
	{
		Pawn = ArcCtxData->InteractorPawn.Get();
	}
	// Fallback to base instigator resolution
	else if (const FInteractionContextData* CtxData = Context.ContextData.GetPtr<FInteractionContextData>())
	{
		UObject* InstigatorObj = CtxData->Instigator.GetObject();
		Pawn = Cast<APawn>(InstigatorObj);
		if (!Pawn)
		{
			if (APlayerState* PS = Cast<APlayerState>(InstigatorObj))
			{
				Pawn = PS->GetPawn();
			}
		}
	}

	StartInteraction(Pawn);
}

bool UArcSmartObjectInteractionComponent::StartInteraction(APawn* Interactor)
{
	if (!Interactor)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	// Resolve Mass entity handle from the owning actor
	FMassEntityHandle EntityHandle;
	const FMassEntityManager* EntityManager = nullptr;

	if (UInstancedActorsComponent* IAComp = OwnerActor->FindComponentByClass<UInstancedActorsComponent>())
	{
		EntityHandle = IAComp->GetMassEntityHandle();
		TSharedPtr<FMassEntityManager> EM = IAComp->GetMassEntityManager();
		EntityManager = EM.Get();
	}
	else if (UMassAgentComponent* AgentComp = OwnerActor->FindComponentByClass<UMassAgentComponent>())
	{
		EntityHandle = AgentComp->GetEntityHandle();
	}

	if (!EntityHandle.IsValid())
	{
		return false;
	}

	// Get smart object handle from the entity's fragment
	const FArcSmartObjectOwnerFragment* SOFragment = nullptr;
	if (EntityManager)
	{
		SOFragment = EntityManager->GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(EntityHandle);
	}

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

	// Setup and activate the interaction context
	GameplayInteractionContext.SetSmartObjectEntity(EntityHandle);
	GameplayInteractionContext.SetContextActor(Interactor);
	GameplayInteractionContext.SetSmartObjectActor(OwnerActor);
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

bool UArcSmartObjectInteractionComponent::IsInteracting() const
{
	return GameplayInteractionContext.GetLastRunStatus() == EStateTreeRunStatus::Running;
}
