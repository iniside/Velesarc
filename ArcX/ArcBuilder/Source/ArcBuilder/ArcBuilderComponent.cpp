/**
* This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */


#include "ArcBuilderComponent.h"

#include "ArcBuildingDefinition.h"
#include "ArcBuildSnappingMethod.h"
#include "ArcBuildResourceConsumption.h"
#include "ArcBuildRequirement.h"
#include "ArcMass/ArcMassEntityVisualization.h"

#include "Engine/AssetManager.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemDefinition.h"
#include "MassEntityConfigAsset.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "TargetingSystem/TargetingSubsystem.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Build_Requirement_Item_Fail, "Build.Requirement.Item.Fail");

// Sets default values for this component's properties
UArcBuilderComponent::UArcBuilderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Called when the game starts
void UArcBuilderComponent::BeginPlay()
{
	Super::BeginPlay();
}

UArcBuildingDefinition* UArcBuilderComponent::ResolveBuildingDefinition(UArcItemDefinition* InItemDef) const
{
	if (!InItemDef)
	{
		return nullptr;
	}

	const FArcItemFragment_BuilderData* BuildFragment = InItemDef->FindFragment<FArcItemFragment_BuilderData>();
	if (!BuildFragment)
	{
		return nullptr;
	}

	if (!BuildFragment->BuildingDefinitionId.IsValid())
	{
		return nullptr;
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	UArcBuildingDefinition* BuildingDef = AssetManager.GetPrimaryAssetObject<UArcBuildingDefinition>(BuildFragment->BuildingDefinitionId);

	if (!BuildingDef)
	{
		// Try synchronous load.
		FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(BuildFragment->BuildingDefinitionId);
		if (AssetPath.IsValid())
		{
			BuildingDef = Cast<UArcBuildingDefinition>(AssetPath.TryLoad());
		}
	}

	return BuildingDef;
}

bool UArcBuilderComponent::DoesMeetItemRequirement(UArcItemDefinition* InItemDef) const
{
	UArcBuildingDefinition* BuildDef = ResolveBuildingDefinition(InItemDef);
	if (!BuildDef)
	{
		return false;
	}

	if (const FArcBuildResourceConsumption* Consumption = BuildDef->ResourceConsumption.GetPtr<FArcBuildResourceConsumption>())
	{
		return Consumption->CheckAndConsumeResources(BuildDef, this, false);
	}

	// No consumption strategy set — treat as free placement.
	return true;
}

void UArcBuilderComponent::SetPlacementOffsetLocation(const FVector& NewLocation)
{
	PlacementOffsetLocation = NewLocation;
}

void UArcBuilderComponent::BeginPlacement(UArcItemDefinition* InBuilderData)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (InBuilderData == CurrentItemDef.Get())
	{
		return;
	}

	if (TemporaryPlacementActor)
	{
		TemporaryPlacementActor->SetActorHiddenInGame(true);
		TemporaryPlacementActor->Destroy();
		TemporaryPlacementActor = nullptr;
	}

	CurrentItemDef = InBuilderData;

	UArcBuildingDefinition* BuildDef = ResolveBuildingDefinition(InBuilderData);
	if (!BuildDef)
	{
		return;
	}

	CurrentBuildingDef = BuildDef;

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

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// TODO:: Load as part of bundle.
	UClass* ActorClass = BuildDef->ResolvePreviewActorClass();
	if (!ActorClass)
	{
		return;
	}

	TemporaryPlacementActor = GetWorld()->SpawnActor<AActor>(ActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	TemporaryPlacementActor->SetActorEnableCollision(false);

	const bool bDoesMeetReq = DoesMeetItemRequirement(InBuilderData);

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	TemporaryPlacementActor->GetComponents(StaticMeshComponents);

	UMaterialInterface* Material = bDoesMeetReq
		? BuildDef->RequirementMeetMaterial
		: BuildDef->RequirementFailedMaterial;

	for (UStaticMeshComponent* SMComp : StaticMeshComponents)
	{
		int32 MaterialNum = SMComp->GetNumMaterials();
		for (int32 i = 0; i < MaterialNum; ++i)
		{
			SMComp->SetMaterial(i, Material);
		}
	}

	// Apply grid size from building definition.
	CurrentGridSize = BuildDef->DefaultGridSize;

	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	FArcTargetingSourceContext Context;
	Context.InstigatorActor = GetOwner();
	Context.SourceActor = Pawn;
	Context.SourceObject = TemporaryPlacementActor;

	if (Targeting)
	{
		if (PlacementTargetingPreset && !TargetingRequestHandle.IsValid())
		{
			TargetingRequestHandle = Arcx::MakeTargetRequestHandle(PlacementTargetingPreset, Context);

			FTargetingAsyncTaskData& AsyncTaskData = FTargetingAsyncTaskData::FindOrAdd(TargetingRequestHandle);
			AsyncTaskData.bRequeueOnCompletion = true;

			FTargetingRequestDelegate CompletionDelegate = FTargetingRequestDelegate::CreateUObject(this, &UArcBuilderComponent::HandleTargetingCompleted);

			Targeting->StartAsyncTargetingRequestWithHandle(TargetingRequestHandle, CompletionDelegate);
		}
	}
}

void UArcBuilderComponent::BeginPlacementFromDefinition(UArcBuildingDefinition* InBuildingDef)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!InBuildingDef)
	{
		return;
	}

	if (InBuildingDef == CurrentBuildingDef.Get())
	{
		return;
	}

	if (TemporaryPlacementActor)
	{
		TemporaryPlacementActor->SetActorHiddenInGame(true);
		TemporaryPlacementActor->Destroy();
		TemporaryPlacementActor = nullptr;
	}

	CurrentItemDef.Reset();
	CurrentBuildingDef = InBuildingDef;

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

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* ActorClass = InBuildingDef->ResolvePreviewActorClass();
	if (!ActorClass)
	{
		return;
	}

	TemporaryPlacementActor = GetWorld()->SpawnActor<AActor>(ActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
	TemporaryPlacementActor->SetActorEnableCollision(false);

	const bool bDoesMeetReq = [&]()
	{
		if (const FArcBuildResourceConsumption* Consumption = InBuildingDef->ResourceConsumption.GetPtr<FArcBuildResourceConsumption>())
		{
			return Consumption->CheckAndConsumeResources(InBuildingDef, this, false);
		}
		return true;
	}();

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	TemporaryPlacementActor->GetComponents(StaticMeshComponents);

	UMaterialInterface* Material = bDoesMeetReq
		? InBuildingDef->RequirementMeetMaterial
		: InBuildingDef->RequirementFailedMaterial;

	for (UStaticMeshComponent* SMComp : StaticMeshComponents)
	{
		int32 MaterialNum = SMComp->GetNumMaterials();
		for (int32 i = 0; i < MaterialNum; ++i)
		{
			SMComp->SetMaterial(i, Material);
		}
	}

	CurrentGridSize = InBuildingDef->DefaultGridSize;

	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	FArcTargetingSourceContext Context;
	Context.InstigatorActor = GetOwner();
	Context.SourceActor = Pawn;
	Context.SourceObject = TemporaryPlacementActor;

	if (Targeting)
	{
		if (PlacementTargetingPreset && !TargetingRequestHandle.IsValid())
		{
			TargetingRequestHandle = Arcx::MakeTargetRequestHandle(PlacementTargetingPreset, Context);

			FTargetingAsyncTaskData& AsyncTaskData = FTargetingAsyncTaskData::FindOrAdd(TargetingRequestHandle);
			AsyncTaskData.bRequeueOnCompletion = true;

			FTargetingRequestDelegate CompletionDelegate = FTargetingRequestDelegate::CreateUObject(this, &UArcBuilderComponent::HandleTargetingCompleted);

			Targeting->StartAsyncTargetingRequestWithHandle(TargetingRequestHandle, CompletionDelegate);
		}
	}
}

void UArcBuilderComponent::EndPlacement()
{
	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	if (Targeting)
	{
		Targeting->RemoveAsyncTargetingRequestWithHandle(TargetingRequestHandle);
		TargetingRequestHandle.Reset();
	}

	if (TemporaryPlacementActor)
	{
		TemporaryPlacementActor->SetActorHiddenInGame(true);
		TemporaryPlacementActor->Destroy();
		TemporaryPlacementActor = nullptr;
	}

	CurrentItemDef.Reset();
	CurrentBuildingDef.Reset();
}

namespace
{
	static FORCEINLINE FVector SnapWorld(const FVector& In, int32 GridSize)
	{
		if (GridSize <= 0) return In;
		const float G = static_cast<float>(GridSize);
		return FVector(
			FMath::GridSnap(In.X, G),
			FMath::GridSnap(In.Y, G),
			In.Z //FMath::GridSnap(In.Z, G)
		);
	}

	static FORCEINLINE FVector SnapRelative(const FVector& In, int32 GridSize, const FVector& Origin, const FQuat& Rotation)
	{
		if (GridSize <= 0)
		{
			return In;
		}
		const float G = static_cast<float>(GridSize);
		const FTransform GridXform(Rotation, Origin, FVector::OneVector);
		const FVector Local = GridXform.InverseTransformPosition(In);
		const FVector SnappedLocal(
			FMath::GridSnap(Local.X, G),
			FMath::GridSnap(Local.Y, G),
			FMath::GridSnap(Local.Z, G)
		);
		return GridXform.TransformPosition(SnappedLocal);
	}
}


void UArcBuilderComponent::HandleTargetingCompleted(FTargetingRequestHandle InTargetingRequestHandle)
{
	UTargetingSubsystem* Targeting = UTargetingSubsystem::Get(GetWorld());
	if (Targeting == nullptr)
	{
		return;
	}

	if (!TemporaryPlacementActor)
	{
		return;
	}

	if (!CurrentBuildingDef.IsValid())
	{
		return;
	}

	const UArcBuildingDefinition* BuildDef = CurrentBuildingDef.Get();

	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(InTargetingRequestHandle);

	if (TargetingResults.TargetResults.Num() > 0)
	{
		UStaticMeshComponent* TargetSMC = nullptr;
		FHitResult HitResult;
		FHitResult FirstHitResult = TargetingResults.TargetResults[0].HitResult;

		RelativeGridOrigin = FirstHitResult.GetActor() ? FirstHitResult.GetActor()->GetActorLocation() : FirstHitResult.ImpactPoint;

		FVector End = FirstHitResult.ImpactPoint + FirstHitResult.ImpactNormal * 200.f;
		DrawDebugPoint(GetWorld(), FirstHitResult.ImpactPoint, 30.f, FColor::Red, false, -1, 0);
		DrawDebugLine(GetWorld(), RelativeGridOrigin, End, FColor::Red, false, -1, 0, 2.0f);

		if (bAlignGridToSurfaceNormal)
		{
			const FVector Normal = FirstHitResult.ImpactNormal.GetSafeNormal();
			if (!Normal.IsNearlyZero())
			{
				RelativeGridRotation = FRotationMatrix::MakeFromZ(Normal).ToQuat();
			}
			else
			{
				RelativeGridRotation = FQuat::Identity;
			}
		}
		else
		{
			RelativeGridRotation = FQuat::Identity;
		}

		FVector EndRelative = FirstHitResult.ImpactPoint + RelativeGridRotation.Vector() * 200.f;
		DrawDebugLine(GetWorld(), RelativeGridOrigin, EndRelative, FColor::Green, false, -1, 0, 2.0f);

		for (const FTargetingDefaultResultData& ResultData : TargetingResults.TargetResults)
		{
			if (ResultData.HitResult.GetActor())
			{
				TargetSMC = ResultData.HitResult.GetActor()->FindComponentByClass<UStaticMeshComponent>();
				HitResult = ResultData.HitResult;
				if (TargetSMC)
				{
					break;
				}
			}
		}

		// Try snapping method from building definition.
		bool bSnappingApplied = false;
		if (const FArcBuildSnappingMethod* SnapMethod = BuildDef->SnappingMethod.GetPtr<FArcBuildSnappingMethod>())
		{
			UStaticMeshComponent* MySMC = TemporaryPlacementActor->FindComponentByClass<UStaticMeshComponent>();

			FGameplayTagContainer TargetTags;
			// TODO: Extract tags from target actor if needed.

			FTransform SnappedTransform;
			if (SnapMethod->ComputeSnappedTransform(FirstHitResult, MySMC, TargetSMC, TargetTags, SnappedTransform))
			{
				FinalPlacementLocation = SnappedTransform.GetLocation();
				FinalPlacementRotation = SnappedTransform.GetRotation();
				TemporaryPlacementActor->SetActorLocation(FinalPlacementLocation);
				TemporaryPlacementActor->SetActorRotation(FinalPlacementRotation);
				bSnappingApplied = true;
			}
		}

		// Grid / raw placement fallback.
		if (!bSnappingApplied)
		{
			FVector FinalLocation = FirstHitResult.ImpactPoint + PlacementOffsetLocation;

			if (bUsePlacementGrid)
			{
				if (bUseRelativeGrid)
				{
					FinalLocation = SnapRelative(FinalLocation, CurrentGridSize, RelativeGridOrigin, RelativeGridRotation);
				}
				else
				{
					const double Grid = static_cast<double>(CurrentGridSize);
					FinalLocation.X = FMath::GridSnap(FinalLocation.X, Grid);
					FinalLocation.Y = FMath::GridSnap(FinalLocation.Y, Grid);
				}
			}

			const FVector Normal = FirstHitResult.ImpactNormal.GetSafeNormal();

			FQuat DesiredRotationQuat = FQuat::Identity;
			if (!Normal.IsNearlyZero() && bUseRelativeGrid)
			{
				FRotator ViewRot = FRotator::ZeroRotator;
				FVector ViewLoc = FVector::ZeroVector;

				if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
				{
					PC->GetPlayerViewPoint(ViewLoc, ViewRot);
				}
				else if (APawn* PawnOwner = Cast<APawn>(GetOwner()))
				{
					PawnOwner->GetActorEyesViewPoint(ViewLoc, ViewRot);
				}

				const FVector ViewFwd = ViewRot.Vector();
				const FVector TangentFwd = (ViewFwd - FVector::DotProduct(ViewFwd, Normal) * Normal).GetSafeNormal();

				// If the projection degenerates (looking straight at the surface), fall back to "Up only"
				DesiredRotationQuat = TangentFwd.IsNearlyZero()
					? FRotationMatrix::MakeFromZ(Normal).ToQuat()
					: FRotationMatrix::MakeFromXZ(TangentFwd, Normal).ToQuat();
			}

			if (TemporaryPlacementActor)
			{
				TemporaryPlacementActor->SetActorRotation(DesiredRotationQuat);
			}

			FinalPlacementLocation = FinalLocation;
			FinalPlacementRotation = DesiredRotationQuat;
			TemporaryPlacementActor->SetActorLocation(FinalLocation);
		}

		// Check placement requirements.
		FTransform TM;
		TM.SetLocation(FinalPlacementLocation);
		TM.SetRotation(FinalPlacementRotation);

		bool bNewMeetsReq = true;
		for (const TInstancedStruct<FArcBuildRequirement>& Item : BuildDef->BuildRequirements)
		{
			if (const FArcBuildRequirement* ItemReq = Item.GetPtr<FArcBuildRequirement>())
			{
				if (!ItemReq->CanPlace(TM, BuildDef, this))
				{
					bNewMeetsReq = false;
					break;
				}
			}
		}

		if (bNewMeetsReq != bDidMeetPlaceRequirements)
		{
			bDidMeetPlaceRequirements = bNewMeetsReq;

			TArray<UStaticMeshComponent*> StaticMeshComponents;
			TemporaryPlacementActor->GetComponents(StaticMeshComponents);

			UMaterialInterface* Material = bDidMeetPlaceRequirements
				? BuildDef->RequirementMeetMaterial
				: BuildDef->RequirementFailedMaterial;

			for (UStaticMeshComponent* SMComp : StaticMeshComponents)
			{
				int32 MaterialNum = SMComp->GetNumOverrideMaterials();
				for (int32 i = 0; i < MaterialNum; ++i)
				{
					SMComp->SetMaterial(i, Material);
				}
			}
		}
	}
}

void UArcBuilderComponent::PlaceObject()
{
	if (!CurrentBuildingDef.IsValid())
	{
		return;
	}

	const UArcBuildingDefinition* BuildDef = CurrentBuildingDef.Get();

	// Check and consume resources via the building definition's consumption strategy.
	if (const FArcBuildResourceConsumption* Consumption = BuildDef->ResourceConsumption.GetPtr<FArcBuildResourceConsumption>())
	{
		if (!Consumption->CheckAndConsumeResources(BuildDef, this, true))
		{
			return;
		}
	}

	FTransform SpawnTransform;
	SpawnTransform.SetLocation(FinalPlacementLocation);
	SpawnTransform.SetRotation(FinalPlacementRotation);

	// Dual path: Mass entity or Actor.
	if (BuildDef->HasMassEntityConfig())
	{
		UArcVisEntitySpawnLibrary::SpawnVisEntity(this, BuildDef->MassEntityConfig, SpawnTransform);
	}
	else
	{
		UClass* ActorClass = BuildDef->ActorClass.LoadSynchronous();
		if (ActorClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = GetOwner();
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			GetWorld()->SpawnActor<AActor>(ActorClass, SpawnTransform.GetLocation(), SpawnTransform.GetRotation().Rotator(), SpawnParams);
		}
	}
}

UArcTargetingTask_BuildTrace::UArcTargetingTask_BuildTrace(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcTargetingTask_BuildTrace::Execute(const FTargetingRequestHandle& TargetingHandle) const
{
	Super::Execute(TargetingHandle);

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Executing);

	FArcTargetingSourceContext* Ctx = FArcTargetingSourceContext::Find(TargetingHandle);
	FTargetingDefaultResultsSet& TargetingResults = FTargetingDefaultResultsSet::FindOrAdd(TargetingHandle);

	if (!Ctx->SourceActor)
	{
		SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
		return;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	Ctx->SourceActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FVector EndLocation = EyeLocation + EyeRotation.Vector() * 10000.f;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Ctx->SourceActor);
	Params.bTraceComplex = true;
	if (AActor* Source = Cast<AActor>(Ctx->SourceObject))
	{
		Params.AddIgnoredActor(Source);
	}

	{
		FHitResult FirstHitResult;
		Ctx->InstigatorActor->GetWorld()->LineTraceSingleByChannel(FirstHitResult, EyeLocation, EndLocation, ECC_Visibility, Params);

		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = FirstHitResult;
	}

	TArray<FHitResult> HitResults;

	FCollisionResponseParams ResponseParams(ECollisionResponse::ECR_Overlap);

	Ctx->InstigatorActor->GetWorld()->SweepMultiByChannel(HitResults, EyeLocation, EndLocation, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(SphereRadius), Params, ResponseParams);

	DrawDebugCylinder(Ctx->InstigatorActor->GetWorld(), EyeLocation, EndLocation, SphereRadius, 10.f, FColor::Green, false, -1.f, 0, 0.2f);
	for (const FHitResult& HitResult : HitResults)
	{
		FTargetingDefaultResultData* ResultData = new(TargetingResults.TargetResults) FTargetingDefaultResultData();
		ResultData->HitResult = HitResult;
		DrawDebugPoint(Ctx->InstigatorActor->GetWorld(), HitResult.ImpactPoint, 10.f, FColor::Red, false);
	}

	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}
