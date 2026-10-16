// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commands/ArcReplicatedCommand.h"
#include "ArcNamedPrimaryAssetId.h"
#include "Mass/EntityHandle.h"

#include "ArcPlaceBuildingCommand.generated.h"

/**
 * Server-authoritative place-building command sent from the client placement session.
 *
 * Self-contained per the replicated-command rule: all trigger state is captured
 * client-side as fields (definition id, final transform, instigator entity) — no
 * World/PC/GameState members; the world is resolved from WorldContext in Execute.
 *
 * Server-side Execute never trusts the client ghost. It re-runs the full gate
 * sequence: instigator alive -> definition resolve -> BuildRequirements re-check ->
 * resource consumption (re-validates internally) -> spawn (Mass entity with
 * post-place-action overrides, or ActorClass fallback) -> PostPlaced pass.
 * Any failed gate returns false (surfaces via the command confirm rail).
 */
USTRUCT()
struct ARCBUILDER_API FArcPlaceBuildingCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

	/** Primary asset id of the UArcBuildingDefinition to place. */
	UPROPERTY()
	FArcNamedPrimaryAssetId BuildingDefinitionId;

	/** Final (snapped) placement transform computed by the client preview. */
	UPROPERTY()
	FTransform PlacementTransform = FTransform::Identity;

	/** Mass entity of the placing player (resource source + requirement instigator). */
	UPROPERTY()
	FMassEntityHandle InstigatorEntity;

	FArcPlaceBuildingCommand() = default;

	FArcPlaceBuildingCommand(
		FArcNamedPrimaryAssetId InBuildingDefinitionId,
		const FTransform& InPlacementTransform,
		FMassEntityHandle InInstigatorEntity)
		: BuildingDefinitionId(InBuildingDefinitionId)
		, PlacementTransform(InPlacementTransform)
		, InstigatorEntity(InInstigatorEntity)
	{
	}

	/** Client-side sanity check: valid id, set entity, finite non-NaN transform. */
	virtual bool CanSendCommand() const override;

	/** Server-side execution — full authoritative gate sequence, then spawn. */
	virtual bool Execute(UObject* WorldContext) override;

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcPlaceBuildingCommand::StaticStruct();
	}

	virtual ~FArcPlaceBuildingCommand() override = default;
};

template <>
struct TStructOpsTypeTraits<FArcPlaceBuildingCommand>
	: public TStructOpsTypeTraitsBase2<FArcPlaceBuildingCommand>
{
	enum
	{
		WithCopy = true
	};
};
