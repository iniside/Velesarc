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

#include "GameFramework/PlayerState.h"
#include "Items/ArcItemDefinition.h"
#include "Targeting/ArcTargetingSourceContext.h"
#include "TargetingSystem/TargetingSubsystem.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_Build_Item_Requirement_Fail, "Build.Requirement.Item.Fail");

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

bool UArcBuilderComponent::DoesMeetItemRequirement(UArcItemDefinition* InItemDef) const
{
	const FArcItemFragment_BuilderData* BuildFragment = InItemDef->FindFragment<FArcItemFragment_BuilderData>();
	if (!BuildFragment)
	{
		return false;
	}

	if (const FArcConsumeItemsRequirement* ItemReq = BuildFragment->ConsumeItemRequirement.GetPtr<FArcConsumeItemsRequirement>())
	{
		if (ItemReq->CheckAndConsumeItems(InItemDef, this, false))
		{
			return true;
		}
		return false;
	}

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
	
	const FArcItemFragment_BuilderData* BuildFragment = CurrentItemDef->FindFragment<FArcItemFragment_BuilderData>();
	if (!BuildFragment)
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
	
	CurrentItemDef = InBuilderData;
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// TODO:: Load as part of bundle.
	UClass* ActorClass = InBuilderData ? BuildFragment->ActorClass.LoadSynchronous() : nullptr;
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
		? BuildFragment->RequirementMeetMaterial
		: BuildFragment->RequirementFailedMaterial;
	
	for (UStaticMeshComponent* SMComp : StaticMeshComponents)
	{
		int32 MaterialNum = SMComp->GetNumMaterials();
		for (int32 i = 0; i < MaterialNum; ++i)
		{
			SMComp->SetMaterial(i, Material);	
		}	
	}
	
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
		
		if (TargetSMC && !bUsePlacementGrid && bIsSocketSnappingEnabled)
		{
			AActor* TargetActor = HitResult.GetActor();
			
			UStaticMeshComponent* MySMC = TemporaryPlacementActor->FindComponentByClass<UStaticMeshComponent>();
			
			if (TargetSMC && MySMC)
			{
				TArray<FName> TargetSockets = TargetSMC->GetAllSocketNames();
				TArray<FName> MySockets = MySMC->GetAllSocketNames();

				if (TargetSockets.Num() > 0 && MySockets.Num() > 0)
				{
					float SqDistance = FLT_MAX;

					FName TargetSocketName;
				
					for (const FName& TargetSocket : TargetSockets)
					{
						FVector TargetSocketLocation = TargetSMC->GetSocketLocation(TargetSocket);
													
						float NewSqDistance = FVector::DistSquared(TargetSocketLocation, FirstHitResult.ImpactPoint);

						if (NewSqDistance < SqDistance)
						{
							SqDistance = NewSqDistance;
							TargetSocketName = TargetSocket;
						}
						
					}

					if (TargetSocketName.IsValid())
					{
						FVector TargetSocketLocation = TargetSMC->GetSocketLocation(TargetSocketName);
						FVector TargetSocketDir = MySMC->GetSocketRotation(TargetSocketName).Vector();
						
						FBoxSphereBounds Bounds = MySMC->Bounds;
						double Half = Bounds.BoxExtent.X * 0.5;

						SqDistance = FLT_MAX;
						FName MySocketName;
						
						for (const FName& MySocket : MySockets)
						{
							FVector MySocketLocation = MySMC->GetSocketLocation(MySocket);
													
							float NewSqDistance = FVector::DistSquared(TargetSocketLocation, MySocketLocation);

							if (NewSqDistance < SqDistance)
							{
								SqDistance = NewSqDistance;
								MySocketName = MySocket;
							}
						}
						
						FVector MySocketLocation = MySMC->GetSocketLocation(MySocketName);
						float Dist = FVector::Dist(TemporaryPlacementActor->GetActorLocation(), MySocketLocation);
						FVector NewLocation = TargetSocketLocation + (TargetSocketDir * Dist);
						TemporaryPlacementActor->SetActorLocation(NewLocation);
						FinalPlacementLocation = NewLocation;
						return;
					}
				}
			}
		}
		
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

		// Apply rotation to the preview actor (so you can see the orientation while placing)
		if (TemporaryPlacementActor)
		{
			TemporaryPlacementActor->SetActorRotation(DesiredRotationQuat);
		}

		
		FinalPlacementLocation = FinalLocation;
		TemporaryPlacementActor->SetActorLocation(FinalLocation);

		const FArcItemFragment_BuilderData* BuildFragment = CurrentItemDef->FindFragment<FArcItemFragment_BuilderData>();
		if (!BuildFragment)
		{
			return;
		}

		FTransform TM;
		TM.SetLocation(FinalLocation);
		TM.SetRotation(DesiredRotationQuat);
		
		bool bNewMeetsReq = true;
		for (const TInstancedStruct<FArcBuildRequirement>& Item : BuildFragment->BuildRequirements)
		{
			if (const FArcBuildRequirement* ItemReq = Item.GetPtr<FArcBuildRequirement>())
			{
				if (!ItemReq->CanPlace(TM, CurrentItemDef.Get(), this))
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
				? BuildFragment->RequirementMeetMaterial
				: BuildFragment->RequirementFailedMaterial;
	
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
	const FArcItemFragment_BuilderData* BuildFragment = CurrentItemDef->FindFragment<FArcItemFragment_BuilderData>();
	if (!BuildFragment)
	{
		return;
	}

	if (const FArcConsumeItemsRequirement* ItemReq = BuildFragment->ConsumeItemRequirement.GetPtr<FArcConsumeItemsRequirement>())
	{
		if (!ItemReq->CheckAndConsumeItems(CurrentItemDef.Get(), this, true))
		{
			return;
		}
	}
	
	UClass* ActorClass = BuildFragment->ActorClass.LoadSynchronous();
	if (ActorClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		
		GetWorld()->SpawnActor<AActor>(ActorClass, FinalPlacementLocation, FRotator::ZeroRotator, SpawnParams);
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
	
	//DrawDebugLine(Ctx->InstigatorActor->GetWorld()
	//, EyeLocation, EndLocation
	//, FColor::Blue, false, 0.1, 0, 0.2f);
	
	SetTaskAsyncState(TargetingHandle, ETargetingTaskAsyncState::Completed);
}

