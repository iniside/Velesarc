// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuilderSubsystem.h"

#include "ArcBuilder.h"
#include "ArcBuilderGhostPreview.h"
#include "ArcBuildingDefinition.h"
#include "ArcBuildSnappingMethod.h"
#include "ArcBuildResourceConsumption.h"
#include "ArcBuildRequirement.h"
#include "ArcBuilderSettings.h"
#include "Commands/ArcPlaceBuildingCommand.h"

#include "ArcTargetingSubsystem.h"
#include "ArcTargetingContext.h"
#include "ArcTargetingPreset.h"
#include "ArcTargetingPipelineTypes.h"

#include "ArcNamedPrimaryAssetId.h"
#include "Commands/ArcReplicatedCommandHelpers.h"

#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "MassAgentComponent.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Materials/MaterialInterface.h"

namespace ArcBuilderSubsystem_Local
{
	FVector SnapRelative(const FVector& In, int32 GridSize, const FVector& Origin, const FQuat& Rotation)
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
			FMath::GridSnap(Local.Z, G));
		return GridXform.TransformPosition(SnappedLocal);
	}
}

void UArcBuilderSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UArcBuilderSubsystem::OnWorldBeginTearDown);
}

void UArcBuilderSubsystem::Deinitialize()
{
	UnregisterTool();

	if (WorldTearDownHandle.IsValid())
	{
		FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
		WorldTearDownHandle.Reset();
	}

	Super::Deinitialize();
}

UWorld* UArcBuilderSubsystem::GetOwnerWorld() const
{
	const ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>();
	return LocalPlayer != nullptr ? LocalPlayer->GetWorld() : nullptr;
}

APlayerController* UArcBuilderSubsystem::GetOwnerPlayerController() const
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer<ULocalPlayer>();
	UWorld* World = LocalPlayer != nullptr ? LocalPlayer->GetWorld() : nullptr;
	return LocalPlayer != nullptr ? LocalPlayer->GetPlayerController(World) : nullptr;
}

void UArcBuilderSubsystem::OnWorldBeginTearDown(UWorld* InWorld)
{
	if (ActiveWorld.IsValid() && ActiveWorld.Get() == InWorld)
	{
		CancelPlacement();
		UnregisterTool();
	}
}

void UArcBuilderSubsystem::RegisterTool(FArcBuilderToolContext&& Context)
{
	ToolContext = MoveTemp(Context);
	bToolRegistered = true;
	ActiveWorld = GetOwnerWorld();

	OnBuilderStateChanged.Broadcast();
}

void UArcBuilderSubsystem::UnregisterTool()
{
	// Cleanup, not a gate: a tool going away takes its placement ghost with it.
	CancelPlacement();

	const bool bWasRegistered = bToolRegistered;

	ToolContext = FArcBuilderToolContext();
	bToolRegistered = false;

	if (bWasRegistered)
	{
		OnBuilderStateChanged.Broadcast();
	}
}

void UArcBuilderSubsystem::SetGridMode(EArcBuilderGridMode Mode)
{
	GridMode = Mode;
	bRelativeGridAnchored = false;
}

void UArcBuilderSubsystem::StartPlacement(UArcBuildingDefinition* Def)
{
	if (Def == nullptr)
	{
		return;
	}

	if (Def == CurrentBuildingDef.Get())
	{
		return;
	}

	// Replace any in-progress placement.
	CancelPlacement();

	UWorld* World = GetOwnerWorld();
	APlayerController* PC = GetOwnerPlayerController();
	APawn* Pawn = PC != nullptr ? PC->GetPawn() : nullptr;
	if (World == nullptr || PC == nullptr || Pawn == nullptr)
	{
		return;
	}

	// Placement preset (per-building override, else subsystem default).
	UArcTargetingPreset* Preset = Def->PlacementTargetingPreset;
	if (Preset == nullptr)
	{
		Preset = GetDefault<UArcBuilderSettings>()->DefaultPlacementPreset.LoadSynchronous();
	}
	if (Preset == nullptr)
	{
		return;
	}

	UArcTargetingSubsystem* Targeting = UArcTargetingSubsystem::Get(World);
	if (Targeting == nullptr)
	{
		return;
	}

	// Resolve the placing pawn's Mass entity.
	PawnEntity = FMassEntityHandle();
	if (UMassAgentComponent* Agent = Pawn->FindComponentByClass<UMassAgentComponent>())
	{
		PawnEntity = Agent->GetEntityHandle();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PC;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	GhostActor = ArcBuilderGhostPreview::SpawnGhostActor(*World, *Def, SpawnParams);
	if (GhostActor == nullptr)
	{
		return;
	}
	GhostActor->SetActorEnableCollision(false);

	ActiveWorld = World;
	CurrentBuildingDef = Def;
	CurrentGridSize = Def->DefaultGridSize;
	FinalPlacementTransform = FTransform::Identity;

	// Relative grid re-anchors to the first object hit in this placement.
	bRelativeGridAnchored = false;

	// Ghost starts in the "unmet" state until the first targeting completion confirms it.
	bDidMeetPlaceRequirements = false;
	ApplyGhostMaterial(false);

	// Start the continuous placement targeting loop.
	FArcTargetingContext Seed;
	Seed.SourceActor = Pawn;
	Seed.InstigatorActor = PC;
	Seed.SourceObject = GhostActor;
	Seed.SourceEntity = PawnEntity;

	TargetingHandle = Targeting->MakeRequest(Preset, Seed);
	Targeting->StartAsync(
		TargetingHandle,
		FArcTargetingCompletedDelegate::CreateUObject(this, &UArcBuilderSubsystem::HandleTargetingCompleted),
		/*bRequeueOnCompletion=*/true);

	OnBuilderStateChanged.Broadcast();
}

void UArcBuilderSubsystem::CancelPlacement()
{
	// Cancel + release the targeting request against the cached active world
	// BEFORE any state reset (skipped when the world is already dead).
	if (TargetingHandle.IsValid())
	{
		UWorld* World = ActiveWorld.Get();
		if (World != nullptr)
		{
			if (UArcTargetingSubsystem* Targeting = UArcTargetingSubsystem::Get(World))
			{
				Targeting->Cancel(TargetingHandle);
				Targeting->Release(TargetingHandle);
			}
		}
		TargetingHandle.Reset();
	}

	DestroyGhost();

	const bool bWasPlacing = CurrentBuildingDef.IsValid();

	CurrentBuildingDef.Reset();
	PawnEntity = FMassEntityHandle();
	FinalPlacementTransform = FTransform::Identity;
	bRelativeGridAnchored = false;
	bDidMeetPlaceRequirements = false;

	if (bWasPlacing)
	{
		OnBuilderStateChanged.Broadcast();
	}
}

void UArcBuilderSubsystem::ConfirmPlacement()
{
	const bool bCheatDisableRequirements = CVarArcBuilderDisableRequirements.GetValueOnGameThread();
	if (!IsPlacing() || (!bDidMeetPlaceRequirements && !bCheatDisableRequirements))
	{
		return;
	}

	const UArcBuildingDefinition* Def = CurrentBuildingDef.Get();
	UWorld* World = GetOwnerWorld();
	if (Def == nullptr || World == nullptr)
	{
		return;
	}

	// Server-authoritative place. Client never spawns the building; it arrives via Mass replication.
	Arcx::SendServerCommand<FArcPlaceBuildingCommand>(
		World,
		FArcNamedPrimaryAssetId(Def->GetPrimaryAssetId()),
		FinalPlacementTransform,
		PawnEntity);

	OnBuilderStateChanged.Broadcast();
}

bool UArcBuilderSubsystem::IsPlacing() const
{
	return CurrentBuildingDef.IsValid() && GhostActor != nullptr;
}

void UArcBuilderSubsystem::DestroyGhost()
{
	if (GhostActor != nullptr)
	{
		GhostActor->SetActorHiddenInGame(true);
		GhostActor->Destroy();
		GhostActor = nullptr;
	}
}

void UArcBuilderSubsystem::ApplyGhostMaterial(bool bMet)
{
	if (GhostActor == nullptr || !CurrentBuildingDef.IsValid())
	{
		return;
	}

	const UArcBuildingDefinition* Def = CurrentBuildingDef.Get();
	UMaterialInterface* Material = bMet ? Def->RequirementMeetMaterial : Def->RequirementFailedMaterial;
	if (Material == nullptr)
	{
		return;
	}

	TArray<UMeshComponent*> MeshComponents;
	GhostActor->GetComponents(MeshComponents);
	for (UMeshComponent* MeshComp : MeshComponents)
	{
		const int32 MaterialNum = MeshComp->GetNumMaterials();
		for (int32 i = 0; i < MaterialNum; ++i)
		{
			MeshComp->SetMaterial(i, Material);
		}
	}
}

void UArcBuilderSubsystem::HandleTargetingCompleted(FArcTargetingHandle InHandle)
{
	UWorld* World = ActiveWorld.Get();
	if (World == nullptr || GhostActor == nullptr || !CurrentBuildingDef.IsValid())
	{
		return;
	}

	UArcTargetingSubsystem* Targeting = UArcTargetingSubsystem::Get(World);
	if (Targeting == nullptr)
	{
		return;
	}

	const FArcTargetingContext* Ctx = Targeting->GetContext(InHandle);
	if (Ctx == nullptr || Ctx->Candidates.Num() == 0)
	{
		return;
	}

	const UArcBuildingDefinition* Def = CurrentBuildingDef.Get();
	const FHitResult FirstHitResult = Ctx->Candidates[0].HitResult;

	// Find the first candidate hit actor carrying a static mesh (snap target).
	UStaticMeshComponent* TargetSMC = nullptr;
	for (const FArcTargetCandidate& Candidate : Ctx->Candidates)
	{
		if (AActor* HitActor = Candidate.HitResult.GetActor())
		{
			TargetSMC = HitActor->FindComponentByClass<UStaticMeshComponent>();
			if (TargetSMC != nullptr)
			{
				break;
			}
		}
	}

	// Snapping method first, then grid / raw fallback.
	bool bSnappingApplied = false;
	if (const FArcBuildSnappingMethod* SnapMethod = Def->SnappingMethod.GetPtr<FArcBuildSnappingMethod>())
	{
		UStaticMeshComponent* MySMC = GhostActor->FindComponentByClass<UStaticMeshComponent>();
		const FGameplayTagContainer TargetTags;

		FTransform SnappedTransform;
		if (SnapMethod->ComputeSnappedTransform(FirstHitResult, MySMC, TargetSMC, TargetTags, SnappedTransform))
		{
			FinalPlacementTransform = SnappedTransform;
			GhostActor->SetActorLocationAndRotation(SnappedTransform.GetLocation(), SnappedTransform.GetRotation());
			bSnappingApplied = true;
		}
	}

	if (!bSnappingApplied)
	{
		FVector FinalLocation = FVector(FirstHitResult.ImpactPoint);
		FQuat DesiredRotationQuat = FQuat::Identity;

		switch (GridMode)
		{
			case EArcBuilderGridMode::Off:
				break;

			case EArcBuilderGridMode::Global:
			{
				const double Grid = static_cast<double>(CurrentGridSize);
				FinalLocation.X = FMath::GridSnap(FinalLocation.X, Grid);
				FinalLocation.Y = FMath::GridSnap(FinalLocation.Y, Grid);
				break;
			}

			case EArcBuilderGridMode::Relative:
			{
				// Anchor once to the first object hit after the latch reset; the frame
				// then stays fixed until the placement ends or the mode changes.
				if (!bRelativeGridAnchored)
				{
					AActor* HitActor = FirstHitResult.GetActor();
					RelativeGridOrigin = HitActor != nullptr ? HitActor->GetActorLocation() : FVector(FirstHitResult.ImpactPoint);
					RelativeGridRotation = HitActor != nullptr ? HitActor->GetActorQuat() : FQuat::Identity;
					bRelativeGridAnchored = true;
				}

				FinalLocation = ArcBuilderSubsystem_Local::SnapRelative(FinalLocation, CurrentGridSize, RelativeGridOrigin, RelativeGridRotation);
				DesiredRotationQuat = RelativeGridRotation;
				break;
			}
		}

		FinalPlacementTransform = FTransform(DesiredRotationQuat, FinalLocation);
		GhostActor->SetActorLocationAndRotation(FinalLocation, DesiredRotationQuat);
	}

	// Evaluate placement requirements + resource availability against local Mass state.
	// Dev cheat: arc.Builder.DisableRequirements forces the verdict to "met" while still
	// running the snap/ghost logic above.
	bool bNewMeetsReq = true;

	if (CVarArcBuilderDisableRequirements.GetValueOnGameThread())
	{
		bNewMeetsReq = true;
	}
	else
	{
		UMassEntitySubsystem* MassSS = World->GetSubsystem<UMassEntitySubsystem>();
		if (MassSS != nullptr)
		{
			FMassEntityManager& EM = MassSS->GetMutableEntityManager();

			FArcBuildRequirementContext ReqCtx;
			ReqCtx.World = World;
			ReqCtx.EM = &EM;
			ReqCtx.Instigator = PawnEntity;

			for (const TInstancedStruct<FArcBuildRequirement>& Requirement : Def->BuildRequirements)
			{
				const FArcBuildRequirement* Req = Requirement.GetPtr();
				if (Req != nullptr && !Req->CanPlace(FinalPlacementTransform, ReqCtx, Def))
				{
					bNewMeetsReq = false;
					break;
				}
			}

			if (bNewMeetsReq)
			{
				if (const FArcBuildResourceConsumption* Consumption = Def->ResourceConsumption.GetPtr())
				{
					bNewMeetsReq = Consumption->CheckResources(EM, PawnEntity, Def, nullptr);
				}
			}
		}
		else
		{
			bNewMeetsReq = false;
		}
	}

	if (bNewMeetsReq != bDidMeetPlaceRequirements)
	{
		bDidMeetPlaceRequirements = bNewMeetsReq;
		ApplyGhostMaterial(bDidMeetPlaceRequirements);
		OnBuilderStateChanged.Broadcast();
	}
}
