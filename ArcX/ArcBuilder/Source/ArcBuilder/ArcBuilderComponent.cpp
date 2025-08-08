// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuilderComponent.h"

#include "GameFramework/PlayerState.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "TargetingSystem/TargetingSubsystem.h"

// Sets default values for this component's properties
UArcBuilderComponent::UArcBuilderComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

// Called when the game starts
void UArcBuilderComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UArcBuilderComponent::BeginPlacement(UArcBuilderData* InBuilderData)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	APawn* Pawn = nullptr;
	if (APlayerController* PC = GetOwner<APlayerController>())
	{
		Pawn = PC->GetPawn();
	}
	if (!Pawn)
	{
		if (APlayerState* PS = GetOwner<APlayerState>())
		{
			Pawn = PS->GetPawn();
		}
	}

	if (!Pawn)
	{
		return;
	}
	
	CurrentBuildData = InBuilderData;
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// TODO:: Load as part of bundle.
	UClass* ActorClass = InBuilderData ? InBuilderData->ActorClass.LoadSynchronous() : nullptr;
	if (!ActorClass)
	{
		return;
	}
	
	TemporaryPlacementActor = GetWorld()->SpawnActor<AActor>(ActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	
	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	FArcTargetingSourceContext Context;
	Context.InstigatorActor = GetOwner();
	Context.SourceActor = Pawn;
	Context.SourceObject = this;
	
	if (Targeting)
	{
		if (PlacementTargetingPreset)
		{
			TargetingRequestHandle = Arcx::MakeTargetRequestHandle(PlacementTargetingPreset, Context);

			FTargetingAsyncTaskData& AsyncTaskData = FTargetingAsyncTaskData::FindOrAdd(TargetingRequestHandle);
			AsyncTaskData.bRequeueOnCompletion = true;
			
			FTargetingRequestDelegate CompletionDelegate = FTargetingRequestDelegate::CreateUObject(this, &UArcBuilderComponent::HandleTargetingCompleted);
			
			Targeting->StartAsyncTargetingRequestWithHandle(TargetingRequestHandle, CompletionDelegate);	
		}
	}
}

void UArcBuilderComponent::HandleTargetingCompleted(FTargetingRequestHandle InTargetingRequestHandle)
{
	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	if (Targeting == nullptr)
	{
		return;
	}

	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(InTargetingRequestHandle);

	if (TargetingResults.TargetResults.Num() > 0)
	{
		FTargetingDefaultResultData& TargetData = TargetingResults.TargetResults[0];
		if (TargetData.HitResult.GetActor())
		{
			FVector FinalLocation = TargetData.HitResult.ImpactPoint + PlacementOffsetLocation;
			TemporaryPlacementActor->SetActorLocation(FinalLocation);
		}
	}
}

void UArcBuilderComponent::EndPlacement()
{
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingRequestHandle);

	if (TargetingResults.TargetResults.Num() > 0)
	{
		FTargetingDefaultResultData& TargetData = TargetingResults.TargetResults[0];
		if (TargetData.HitResult.GetActor())
		{
			UClass* ActorClass = CurrentBuildData.IsValid() ? CurrentBuildData->ActorClass.LoadSynchronous() : nullptr;
			if (!ActorClass)
			{
				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = GetOwner();
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				
				FVector FinalLocation = TargetData.HitResult.ImpactPoint + PlacementOffsetLocation;
				GetWorld()->SpawnActor<AActor>(ActorClass, FinalLocation, FRotator::ZeroRotator, SpawnParams);
			}
		}
	}

	if (TemporaryPlacementActor)
	{
		TemporaryPlacementActor->SetActorHiddenInGame(true);
		TemporaryPlacementActor->Destroy();
		TemporaryPlacementActor = nullptr;
	}

	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	if (Targeting)
	{
		Targeting->RemoveAsyncTargetingRequestWithHandle(TargetingRequestHandle);
	}
	
}

