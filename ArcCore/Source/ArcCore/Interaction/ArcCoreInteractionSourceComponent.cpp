// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCoreInteractionSourceComponent.h"

#include "ArcCoreAsyncMessageTypes.h"
#include "AsyncGameplayMessageSystem.h"
#include "AsyncMessageWorldSubsystem.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "TargetingSystem/TargetingPreset.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Components/PrimitiveComponent.h"
#include "InteractableTargetInterface.h"

UArcCoreInteractionSourceComponent::UArcCoreInteractionSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UArcCoreInteractionSourceComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UArcCoreInteractionSourceComponent::OnPawnDataReady(APawn* Pawn)
{
	Super::OnPawnDataReady(Pawn);

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	FArcTargetingSourceContext Context;
	Context.InstigatorActor = GetOwner();
	Context.SourceActor = Pawn;
	Context.SourceObject = this;

	if (Targeting)
	{
		if (InteractionTargetingPreset)
		{
			TargetingRequestHandle = Arcx::MakeTargetRequestHandle(InteractionTargetingPreset, Context);

			FTargetingAsyncTaskData& AsyncTaskData = FTargetingAsyncTaskData::FindOrAdd(TargetingRequestHandle);
			AsyncTaskData.bRequeueOnCompletion = true;

			FTargetingRequestDelegate CompletionDelegate = FTargetingRequestDelegate::CreateUObject(this, &UArcCoreInteractionSourceComponent::HandleTargetingCompleted);

			Targeting->StartAsyncTargetingRequestWithHandle(TargetingRequestHandle, CompletionDelegate);
		}
	}
}

void UArcCoreInteractionSourceComponent::HandleTargetingCompleted(FTargetingRequestHandle InTargetingRequestHandle)
{
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(InTargetingRequestHandle);

	// Extract the best target (first result after filtering + sorting)
	TScriptInterface<IInteractionTarget> NewTarget;
	if (TargetingResults.TargetResults.Num() > 0)
	{
		const FTargetingDefaultResultData& BestResult = TargetingResults.TargetResults[0];
		if (AActor* HitActor = BestResult.HitResult.GetActor())
		{
			// Try actor first
			NewTarget = TScriptInterface<IInteractionTarget>(HitActor);
			if (!NewTarget)
			{
				// Try the hit component
				NewTarget = TScriptInterface<IInteractionTarget>(BestResult.HitResult.GetComponent());
			}
			if (!NewTarget)
			{
				// Search actor's components for any IInteractionTarget implementor
				for (UActorComponent* Comp : HitActor->GetComponents())
				{
					TScriptInterface<IInteractionTarget> CompTarget(Comp);
					if (CompTarget)
					{
						NewTarget = CompTarget;
						break;
					}
				}
			}
		}
	}

	// No change — early out
	if (NewTarget == CurrentTarget)
	{
		return;
	}

	TSharedPtr<FAsyncGameplayMessageSystem> MessageSystem = UAsyncMessageWorldSubsystem::GetSharedMessageSystem<FAsyncGameplayMessageSystem>(GetWorld());

	// Invalidate old target
	if (CurrentTarget)
	{
		PreviousQueryResults = MoveTemp(CurrentQueryResults);

		if (MessageSystem.IsValid())
		{
			FArcInteractionInvalidatedMessage InvalidatedMsg;
			InvalidatedMsg.PreviousTarget = CurrentTarget;
			InvalidatedMsg.PreviousQueryResults = PreviousQueryResults;

			FAsyncMessageId MessageId(Arcx::InteractionMessages::Interaction_Invalidated);
			MessageSystem->QueueMessageForBroadcast(MessageId, FConstStructView::Make(InvalidatedMsg));
		}
	}

	// Set new target
	CurrentTarget = NewTarget;
	CurrentQueryResults.Reset();

	// Acquire new target
	if (CurrentTarget)
	{
		FInteractionContext Context;
		Context.Target = CurrentTarget;
		CurrentTarget->AppendTargetConfiguration(Context, CurrentQueryResults);

		if (MessageSystem.IsValid())
		{
			FArcInteractionAcquiredMessage AcquiredMsg;
			AcquiredMsg.Target = CurrentTarget;
			AcquiredMsg.QueryResults = CurrentQueryResults;

			FAsyncMessageId MessageId(Arcx::InteractionMessages::Interaction_Acquired);
			MessageSystem->QueueMessageForBroadcast(MessageId, FConstStructView::Make(AcquiredMsg));
		}
	}

	OnInteractionChanged();
	NativeOnInteractionChanged();
}
