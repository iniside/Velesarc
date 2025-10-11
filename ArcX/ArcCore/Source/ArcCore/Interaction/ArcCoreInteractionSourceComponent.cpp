// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCoreInteractionSourceComponent.h"


#include "GameFramework/PlayerState.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "TargetingSystem/TargetingPreset.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
// Sets default values for this component's properties
UArcCoreInteractionSourceComponent::UArcCoreInteractionSourceComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

// Called when the game starts
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
	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	if (Targeting == nullptr)
	{
		return;
	}

	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(InTargetingRequestHandle);

	TArray<TScriptInterface<IInteractionTarget>> NewInteractableTargets;
	for (const FTargetingDefaultResultData& Result : TargetingResults.TargetResults)
	{
		if (!Result.HitResult.GetActor())
		{
			continue;
		}

		const TScriptInterface<IInteractionTarget> InteractableActor(Result.HitResult.GetActor());
		if (InteractableActor)
		{
			NewInteractableTargets.Add(InteractableActor);
		}

		TScriptInterface<IInteractionTarget> InteractableComponent(Result.HitResult.GetComponent());
		if (InteractableComponent)
		{
			NewInteractableTargets.AddUnique(InteractableComponent);
		}
	}

	bool bChanged = false;

	if (CurrentAvailableTargets.Num() != NewInteractableTargets.Num())
	{
		CurrentAvailableTargets.Reset(NewInteractableTargets.Num());
		CurrentAvailableTargets.Append(NewInteractableTargets);
		bChanged = true;
	}
	if (CurrentAvailableTargets.Num() == NewInteractableTargets.Num())
	{
		for (int32 i = 0; i < CurrentAvailableTargets.Num(); i++)
		{
			if (CurrentAvailableTargets[i] != NewInteractableTargets[i])
			{
				CurrentAvailableTargets[i] = NewInteractableTargets[i];
				bChanged = true;
			}
		}
	}

	if (bChanged)
	{
		OnAvailableInteractionsUpdated();
		NativeOnAvailableInteractionsUpdated();

		OnInteractionTargetsChanged.Broadcast(CurrentAvailableTargets);
	}
}

void UArcTargetingTask_SphereSweep::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);
	
	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	FVector SourceLocation = Ctx->SourceActor->GetActorLocation();
	UWorld* World = Ctx->SourceActor->GetWorld();

	FVector StartLocation = SourceLocation;
	FVector EndLocation = StartLocation + FVector(0,0,1);
	
	FCollisionQueryParams Params;
	Params.OwnerTag = GetFName();
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;
	
	ECollisionChannel CollisionChannel = UEngineTypes::ConvertToCollisionChannel(TraceChannel);

	FCollisionResponseParams ResponseParams;
	ResponseParams.CollisionResponse = ECollisionResponse::ECR_Overlap;

	TArray<FHitResult> HitResults;
	World->SweepMultiByChannel(HitResults, StartLocation, EndLocation, FQuat::Identity, CollisionChannel,
		FCollisionShape::MakeSphere(Radius.GetValue()), Params, ResponseParams);
	
	for (const FHitResult& Hit : HitResults)
	{
		FTargetingDefaultResultData* NewResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		NewResultData->HitResult = Hit;
	}
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

bool UArcTargetingFilterTask_Interactable::ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const
{
	if (AActor* Actor = TargetData.HitResult.GetActor())
	{
		bool bIsTargetInteractable = Actor->Implements<UInteractionTarget>();
		bool bIsTargetComponentInteractable = TargetData.HitResult.GetComponent()->Implements<UInteractionTarget>();
		
		if (bIsTargetInteractable)
		{
			return false;
		}

		if (bIsTargetComponentInteractable)
		{
			return false;
		}
	}
	
	return true;
}

float UArcTargetingFilterTask_SortByDistance::GetScoreForTarget(const FTargetingRequestHandle& TargetingHandle
																, const FTargetingDefaultResultData& TargetData) const
{
	if (AActor* TargetActor = TargetData.HitResult.GetActor())
	{
		FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
		
		FVector TargetLocation = TargetActor->GetActorLocation();
		FVector SourceLocation = Ctx->SourceActor->GetActorLocation();

		return FVector::DistSquared(SourceLocation, TargetLocation);
	}

	return 0.f;
}

bool UArcInteractableInterfaceLibrary::GetInteractionTargetConfiguration(const FInteractionQueryResults& Item
								   , UScriptStruct* InConfigType
								   , int32& OutFragment)
{
	checkNoEntry();
	return false;
}

DEFINE_FUNCTION(UArcInteractableInterfaceLibrary::execGetInteractionTargetConfiguration)
{
	P_GET_STRUCT(FInteractionQueryResults, Item);
	P_GET_OBJECT(UScriptStruct, InConfigType);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;
	bool bSuccess = true;

	P_NATIVE_BEGIN;
	for (const FInstancedStruct& Interaction : Item.AvailableInteractions)
	{
		if (Interaction.GetScriptStruct() && Interaction.GetScriptStruct() == InConfigType)
		{
			OutItemProp->Struct->CopyScriptStruct(OutItemDataPtr, Interaction.GetMemory());
			bSuccess = true;
			break;
		}
	}
	P_NATIVE_END;
	
	*(bool*)RESULT_PARAM = bSuccess;
}