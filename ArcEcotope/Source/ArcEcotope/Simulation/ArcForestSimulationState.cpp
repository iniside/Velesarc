// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcForestSimulationState.h"

#include "Serialization/ArcEcotopeCustomVersion.h"

void UArcForestSimulationState::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FArcEcotopeCustomVersion::GUID);
	const int32 ArchiveCustomVersion = Ar.CustomVer(FArcEcotopeCustomVersion::GUID);
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		// A supported version is retaken from the archive, because a state saved at the then-latest
		// version elides the property against a CDO default that has since moved. An unsupported one
		// is never retaken: duplication and transaction archives report the current registry.
		if (SourceCustomVersion >= FArcEcotopeCustomVersion::ChunkedSiteOnly
			&& SourceCustomVersion <= FArcEcotopeCustomVersion::LatestVersion)
		{
			SourceCustomVersion = ArchiveCustomVersion;
		}

		bLoadedUnsupportedVersion =
			ArchiveCustomVersion < FArcEcotopeCustomVersion::ChunkedSiteOnly
			|| ArchiveCustomVersion > FArcEcotopeCustomVersion::LatestVersion
			|| SourceCustomVersion < FArcEcotopeCustomVersion::ChunkedSiteOnly
			|| SourceCustomVersion > FArcEcotopeCustomVersion::LatestVersion;
		RefreshStatus();
	}
}

void UArcForestSimulationState::PostLoad()
{
	Super::PostLoad();

	++SnapshotGeneration;
}

EArcForestSimulationStateStatus UArcForestSimulationState::EvaluateStatus(
	FText& OutError,
	const FName ExpectedWorldPackageName,
	const uint64 ExpectedSettingsHash,
	const TArray<FArcForestSpeciesEcologyHash>* ExpectedSpeciesEcologyHashes,
	const FGuid ExpectedSimulationId) const
{
	if (bLoadedUnsupportedVersion
		|| SourceCustomVersion < FArcEcotopeCustomVersion::ChunkedSiteOnly
		|| SourceCustomVersion > FArcEcotopeCustomVersion::LatestVersion)
	{
		OutError = FText::FromString(
			FString::Printf(
				TEXT("ArcForest state originated from unsupported custom serialization version %d. Clear State and bootstrap again."),
				SourceCustomVersion));
		return EArcForestSimulationStateStatus::InvalidVersion;
	}

	const EArcForestSimulationStateStatus SnapshotStatus =
		Snapshot.Validate(OutError, ExpectedSettingsHash, ExpectedSpeciesEcologyHashes);
	if (SnapshotStatus != EArcForestSimulationStateStatus::Valid)
	{
		return SnapshotStatus;
	}

	if (AssociatedWorldPackageName.IsNone()
		|| AssociatedSiteSnapshotHash == 0
		|| AssociatedSiteSnapshotHash != Snapshot.SiteSnapshotHash)
	{
		OutError = FText::FromString(TEXT("ArcForest state has an invalid world or site association."));
		return EArcForestSimulationStateStatus::Invalid;
	}

	if (!ExpectedWorldPackageName.IsNone() && AssociatedWorldPackageName != ExpectedWorldPackageName)
	{
		OutError = FText::FromString(TEXT("ArcForest state belongs to a different world."));
		return EArcForestSimulationStateStatus::Incompatible;
	}

	if (ExpectedSimulationId.IsValid() && Snapshot.SimulationId != ExpectedSimulationId)
	{
		OutError = FText::FromString(TEXT("ArcForest state belongs to a different simulation volume lineage."));
		return EArcForestSimulationStateStatus::Incompatible;
	}

	OutError = FText::GetEmpty();
	return EArcForestSimulationStateStatus::Valid;
}

void UArcForestSimulationState::RefreshStatus(
	const FName ExpectedWorldPackageName,
	const uint64 ExpectedSettingsHash,
	const TArray<FArcForestSpeciesEcologyHash>* ExpectedSpeciesEcologyHashes,
	const FGuid ExpectedSimulationId)
{
	FText UnusedError;
	Status = EvaluateStatus(
		UnusedError,
		ExpectedWorldPackageName,
		ExpectedSettingsHash,
		ExpectedSpeciesEcologyHashes,
		ExpectedSimulationId);
}

void UArcForestSimulationState::ResetToEmpty()
{
	SourceCustomVersion = FArcEcotopeCustomVersion::LatestVersion;
	AssociatedWorldPackageName = NAME_None;
	AssociatedSiteSnapshotHash = 0;
	Snapshot = FArcForestWorldSnapshot();
	Status = EArcForestSimulationStateStatus::Empty;
	bLoadedUnsupportedVersion = false;
	++SnapshotGeneration;
}

void UArcForestSimulationState::ApplyCommittedSnapshot(
	FArcForestWorldSnapshot&& InSnapshot,
	const FName InAssociatedWorldPackageName)
{
	SourceCustomVersion = FArcEcotopeCustomVersion::LatestVersion;
	AssociatedWorldPackageName = InAssociatedWorldPackageName;
	AssociatedSiteSnapshotHash = InSnapshot.SiteSnapshotHash;
	Snapshot = MoveTemp(InSnapshot);
	bLoadedUnsupportedVersion = false;
	++SnapshotGeneration;
	RefreshStatus();
}
