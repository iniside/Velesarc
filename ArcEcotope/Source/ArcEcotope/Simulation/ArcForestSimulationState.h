// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ArcEcotopeCustomVersion.h"
#include "Simulation/ArcForestWorldSnapshot.h"
#include "UObject/Object.h"

#include "ArcForestSimulationState.generated.h"

UCLASS(BlueprintType, DefaultToInstanced, EditInlineNew)
class ARCECOTOPE_API UArcForestSimulationState : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Custom version of the archive this object originated from. This remains
	 * serialized so unsupported future data cannot become usable after a
	 * current-version save or duplication.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Serialization")
	int32 SourceCustomVersion = FArcEcotopeCustomVersion::LatestVersion;

	/** Package name of the world in which this state was produced. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Association")
	FName AssociatedWorldPackageName;

	/** Site hash captured separately so an association cannot silently drift from the snapshot metadata. */
	UPROPERTY(VisibleAnywhere, Category = "Association")
	uint64 AssociatedSiteSnapshotHash = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	FArcForestWorldSnapshot Snapshot;

	/** Derived on load or explicit refresh; it is never authoritative serialized data. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "State")
	EArcForestSimulationStateStatus Status = EArcForestSimulationStateStatus::Empty;

	/**
	 * Bumped whenever Snapshot is replaced. It is the only signal a live consumer has that the stand
	 * under it changed, and it is transient because a fresh load rebuilds those consumers anyway.
	 */
	UPROPERTY(Transient, DuplicateTransient)
	uint64 SnapshotGeneration = 0;

	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;

	EArcForestSimulationStateStatus EvaluateStatus(
		FText& OutError,
		FName ExpectedWorldPackageName = NAME_None,
		uint64 ExpectedSettingsHash = 0,
		const TArray<FArcForestSpeciesEcologyHash>* ExpectedSpeciesEcologyHashes = nullptr,
		FGuid ExpectedSimulationId = FGuid()) const;

	void RefreshStatus(
		FName ExpectedWorldPackageName = NAME_None,
		uint64 ExpectedSettingsHash = 0,
		const TArray<FArcForestSpeciesEcologyHash>* ExpectedSpeciesEcologyHashes = nullptr,
		FGuid ExpectedSimulationId = FGuid());

	/** Clears copied lineage while preserving the owning volume's baked site and authoring configuration. */
	void ResetToEmpty();

	/**
	 * Replaces the complete authoritative snapshot and its serialized association.
	 * Editor orchestration owns transactions and stale-input checks around this pure state apply.
	 */
	void ApplyCommittedSnapshot(FArcForestWorldSnapshot&& InSnapshot, FName InAssociatedWorldPackageName);

private:
	bool bLoadedUnsupportedVersion = false;
};
