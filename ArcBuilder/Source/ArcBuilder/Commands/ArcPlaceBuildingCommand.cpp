// Copyright Lukasz Baran. All Rights Reserved.

#include "Commands/ArcPlaceBuildingCommand.h"

#include "ArcBuilder.h"
#include "ArcBuildingDefinition.h"
#include "ArcBuildPostPlaceAction.h"
#include "ArcBuildRequirement.h"
#include "ArcBuildResourceConsumption.h"

#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Spawner/ArcMassSpawnerSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Items/ArcItemSpec.h"
#include "MassEntitySubsystem.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcPlaceBuildingCommandLocal
{
	DEFINE_LOG_CATEGORY_STATIC(LogArcPlaceBuildingCommand, Log, All);

	/**
	 * Resolve the building definition from its primary asset id: registry object
	 * first, synchronous path load as fallback (mirrors the pattern the old
	 * component's ResolveBuildingDefinition used).
	 */
	UArcBuildingDefinition* ResolveBuildingDefinition(const FArcNamedPrimaryAssetId& BuildingDefinitionId)
	{
		if (!BuildingDefinitionId.IsValid())
		{
			return nullptr;
		}

		UAssetManager& AssetManager = UAssetManager::Get();
		UArcBuildingDefinition* BuildingDef =
			AssetManager.GetPrimaryAssetObject<UArcBuildingDefinition>(BuildingDefinitionId.AssetId);

		if (!BuildingDef)
		{
			// Try synchronous load.
			FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(BuildingDefinitionId.AssetId);
			if (AssetPath.IsValid())
			{
				BuildingDef = Cast<UArcBuildingDefinition>(AssetPath.TryLoad());
			}
		}

		return BuildingDef;
	}
} // namespace ArcPlaceBuildingCommandLocal

bool FArcPlaceBuildingCommand::CanSendCommand() const
{
	if (!BuildingDefinitionId.IsValid())
	{
		return false;
	}
	if (!InstigatorEntity.IsSet())
	{
		return false;
	}
	if (PlacementTransform.GetLocation().ContainsNaN()
		|| PlacementTransform.GetRotation().ContainsNaN()
		|| PlacementTransform.GetScale3D().ContainsNaN()
		|| !PlacementTransform.IsValid())
	{
		return false;
	}
	return true;
}

bool FArcPlaceBuildingCommand::Execute(UObject* WorldContext)
{
	using namespace ArcPlaceBuildingCommandLocal;

	// -----------------------------------------------------------------------
	// 1. Resolve world -> entity manager; instigator must be alive.
	// -----------------------------------------------------------------------
	UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	if (World == nullptr)
	{
		UE_LOG(LogArcPlaceBuildingCommand, Warning,
			TEXT("FArcPlaceBuildingCommand::Execute — WorldContext resolved to null world."));
		return false;
	}

	UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
	if (MassSub == nullptr)
	{
		UE_LOG(LogArcPlaceBuildingCommand, Warning,
			TEXT("FArcPlaceBuildingCommand::Execute — UMassEntitySubsystem not found."));
		return false;
	}

	FMassEntityManager& EntityManager = MassSub->GetMutableEntityManager();
	if (!EntityManager.IsEntityActive(InstigatorEntity))
	{
		UE_LOG(LogArcPlaceBuildingCommand, Warning,
			TEXT("FArcPlaceBuildingCommand::Execute — InstigatorEntity is not active."));
		return false;
	}

	// -----------------------------------------------------------------------
	// 2. Resolve the building definition.
	// -----------------------------------------------------------------------
	const UArcBuildingDefinition* BuildingDef = ResolveBuildingDefinition(BuildingDefinitionId);
	if (BuildingDef == nullptr)
	{
		UE_LOG(LogArcPlaceBuildingCommand, Warning,
			TEXT("FArcPlaceBuildingCommand::Execute — failed to resolve building definition %s."),
			*BuildingDefinitionId.AssetId.ToString());
		return false;
	}

	// Dev cheat: arc.Builder.DisableRequirements bypasses both requirements and resource
	// costs below. The cvar is per-process — on a dedicated server it's the SERVER's
	// value that governs Execute() here, not the placing client's.
	const bool bCheatDisableRequirements = CVarArcBuilderDisableRequirements.GetValueOnGameThread();

	// -----------------------------------------------------------------------
	// 3. Re-validate placement requirements — server never trusts the ghost.
	// -----------------------------------------------------------------------
	if (!bCheatDisableRequirements)
	{
		FArcBuildRequirementContext RequirementContext;
		RequirementContext.World = World;
		RequirementContext.EM = &EntityManager;
		RequirementContext.Instigator = InstigatorEntity;

		for (const TInstancedStruct<FArcBuildRequirement>& Requirement : BuildingDef->BuildRequirements)
		{
			const FArcBuildRequirement* RequirementPtr = Requirement.GetPtr();
			if (RequirementPtr != nullptr
				&& !RequirementPtr->CanPlace(PlacementTransform, RequirementContext, BuildingDef))
			{
				UE_LOG(LogArcPlaceBuildingCommand, Log,
					TEXT("FArcPlaceBuildingCommand::Execute — placement requirement failed for %s."),
					*BuildingDefinitionId.AssetId.ToString());
				return false;
			}
		}
	}

	// -----------------------------------------------------------------------
	// 4. Consume resources (re-validates internally). Unset = free placement.
	// Skipped entirely under the cheat cvar — ConsumedItems stays empty, so any
	// post-place stat actions keyed off consumed items simply contribute nothing.
	// -----------------------------------------------------------------------
	TArray<FArcItemSpec> ConsumedItems;
	if (!bCheatDisableRequirements)
	{
		if (const FArcBuildResourceConsumption* Consumption = BuildingDef->ResourceConsumption.GetPtr())
		{
			if (!Consumption->ConsumeResources(EntityManager, InstigatorEntity, BuildingDef, ConsumedItems))
			{
				UE_LOG(LogArcPlaceBuildingCommand, Log,
					TEXT("FArcPlaceBuildingCommand::Execute — resource consumption failed for %s."),
					*BuildingDefinitionId.AssetId.ToString());
				return false;
			}
		}
	}

	// -----------------------------------------------------------------------
	// 5. Spawn: Mass entity with post-place-action overrides, or actor fallback.
	// -----------------------------------------------------------------------
	FArcBuildPlacementContext PlacementContext;
	PlacementContext.Def = BuildingDef;
	PlacementContext.ConsumedItems = ConsumedItems;
	PlacementContext.Instigator = InstigatorEntity;
	PlacementContext.Transform = PlacementTransform;
	PlacementContext.World = World;

	TArray<FInstancedStruct> SpawnOverrides;
	for (const TInstancedStruct<FArcBuildPostPlaceAction>& Action : BuildingDef->PostPlaceActions)
	{
		if (const FArcBuildPostPlaceAction* ActionPtr = Action.GetPtr())
		{
			ActionPtr->BuildSpawnOverrides(PlacementContext, SpawnOverrides);
		}
	}

	if (BuildingDef->HasMassEntityConfig())
	{
		UArcMassSpawnerSubsystem* Spawner = World->GetSubsystem<UArcMassSpawnerSubsystem>();
		if (Spawner == nullptr)
		{
			UE_LOG(LogArcPlaceBuildingCommand, Warning,
				TEXT("FArcPlaceBuildingCommand::Execute — UArcMassSpawnerSubsystem not found."));
			return false;
		}

		const FMassEntityHandle PlacedEntity =
			Spawner->SpawnEntityWithOverrides(BuildingDef->MassEntityConfig, PlacementTransform, SpawnOverrides);
		if (!PlacedEntity.IsSet())
		{
			UE_LOG(LogArcPlaceBuildingCommand, Warning,
				TEXT("FArcPlaceBuildingCommand::Execute — entity spawn failed for %s."),
				*BuildingDefinitionId.AssetId.ToString());
			return false;
		}

		// -------------------------------------------------------------------
		// 6. Post-place pass with the live handle (Commit + creation observers
		//    have already run — command context is game thread, not mid-phase).
		// -------------------------------------------------------------------
		for (const TInstancedStruct<FArcBuildPostPlaceAction>& Action : BuildingDef->PostPlaceActions)
		{
			if (const FArcBuildPostPlaceAction* ActionPtr = Action.GetPtr())
			{
				ActionPtr->PostPlaced(EntityManager, PlacedEntity, PlacementContext);
			}
		}

		return true;
	}

	// Actor fallback for non-Mass definitions — Mass-only post steps are skipped.
	UClass* ActorClass = BuildingDef->ActorClass.LoadSynchronous();
	if (ActorClass == nullptr)
	{
		UE_LOG(LogArcPlaceBuildingCommand, Warning,
			TEXT("FArcPlaceBuildingCommand::Execute — %s has neither MassEntityConfig nor ActorClass."),
			*BuildingDefinitionId.AssetId.ToString());
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* PlacedActor = World->SpawnActor<AActor>(
		ActorClass,
		PlacementTransform.GetLocation(),
		PlacementTransform.GetRotation().Rotator(),
		SpawnParams);

	return PlacedActor != nullptr;
}
