// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeVolume.h"

#include "ArcEcotopeModule.h"
#include "Components/BoxComponent.h"
#include "Determinism/ArcForestDeterministic.h"
#include "Diagnostics/ArcForestDiagnostics.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Instances/ArcEcotopeInstanceComponents.h"
#include "Instances/ArcEcotopeInstancePartitionActor.h"
#include "Serialization/ArcEcotopeCustomVersion.h"
#include "Simulation/ArcEcotopeFrozenSiteSource.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Terrain/ArcEcotopeTerrainWeightStamp.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

namespace ArcEcotopeVolumePrivate
{
	constexpr int64 SiteValidationReaderBudgetBytes = 64ll * 1024ll * 1024ll;
	constexpr int64 SiteCompatibilityScheduleBudgetBytes = 64ll * 1024ll * 1024ll;

	TAutoConsoleVariable<int32> CVarRegionalRecruitmentMaxProposalRounds(
		TEXT("arcecotope.Simulation.RegionalRecruitmentMaxProposalRounds"),
		256,
		TEXT("Maximum rejection-proposal rounds per regional recruitment attempt; clamped to [1,1024]."),
		ECVF_Default);

	bool IsConsoleVariableEnabled(const TCHAR* Name)
	{
		const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name);
		return Variable != nullptr && Variable->GetInt() != 0;
	}

	FArcForestFrozenResponseRange FreezeResponse(const FArcForestResponseRange& Source)
	{
		FArcForestFrozenResponseRange Result;
		Result.Minimum = Source.Minimum;
		Result.Optimum = Source.Optimum;
		Result.Maximum = Source.Maximum;
		return Result;
	}

	bool GuidLess(const FGuid& Left, const FGuid& Right)
	{
		if (Left.A != Right.A)
		{
			return Left.A < Right.A;
		}
		if (Left.B != Right.B)
		{
			return Left.B < Right.B;
		}
		if (Left.C != Right.C)
		{
			return Left.C < Right.C;
		}
		return Left.D < Right.D;
	}

	void AddInstanceVisualHashComponent(UE::ArcForest::FKeyedHash64& Hash, const FGuid& SpeciesId, const uint64 VisualHash)
	{
		Hash.AddGuid(SpeciesId);
		Hash.AddUInt64(VisualHash);
	}

	// Single predicate shared by every settings/ecology/visual hash site and by FreezeBootstrapInput —
	// they must agree on which setups are "in", or the frozen array mismatches the hash the worker
	// re-derives from it and the run aborts.
	bool IsEnabledForSimulation(const FArcEcotopeSpeciesSetup& Setup)
	{
		return Setup.bEnabled;
	}

#if WITH_EDITOR
	bool CanRepairAuthoredSite(const AActor& Actor)
	{
		if (!GIsEditor)
		{
			return false;
		}
		const UPackage* Package = Actor.GetPackage();
		return Package == nullptr || !Package->HasAnyPackageFlags(PKG_PlayInEditor);
	}
#endif

}

AArcEcotopeVolume::AArcEcotopeVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	VolumeBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("VolumeBounds"));
	SetRootComponent(VolumeBounds);
	VolumeBounds->SetBoxExtent(FVector(5000.0, 5000.0, 1000.0));
	VolumeBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VolumeBounds->SetCanEverAffectNavigation(false);

	TerrainWeightStamp = CreateDefaultSubobject<UArcEcotopeTerrainWeightStamp>(TEXT("TerrainWeightStamp"));
	TerrainWeightStamp->SetupAttachment(VolumeBounds);

	SimulationState = CreateDefaultSubobject<UArcForestSimulationState>(TEXT("SimulationState"));
}

void AArcEcotopeVolume::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad) && !VolumeId.IsValid())
	{
		VolumeId = FGuid::NewGuid();
	}
}

void AArcEcotopeVolume::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FArcEcotopeCustomVersion::GUID);
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	const int32 ArchiveCustomVersion = Ar.CustomVer(FArcEcotopeCustomVersion::GUID);
	const bool bLoadingUnsupportedSiteArchive =
		Ar.IsLoading() && ArchiveCustomVersion < FArcEcotopeCustomVersion::ChunkedSiteOnly;
	auto DiscardSiteData = [this]()
	{
		SiteMetadata = FArcEcotopeSiteMetadata();
		SiteChunkManifest.Reset();
		SiteChunkPayloads.Reset();
		FoliageFieldPayload.Reset();
		if (SimulationState != nullptr)
		{
			SimulationState->ResetToEmpty();
		}
		InstanceSnapshotHash = 0;
		InstanceVisualHash = 0;
		InstanceComponentContentHash = 0;
		bInstanceRepresentationRequiresRebuild = false;
	};
	if (Ar.IsLoading() && ArchiveCustomVersion < FArcEcotopeCustomVersion::ChunkedSiteBulkData)
	{
		DiscardSiteData();
		return;
	}
	const bool bSkipSiteBulk = !Ar.IsPersistent() || (!Ar.IsLoading() && !Ar.IsSaving())
		|| Ar.IsTransacting() || Ar.IsCooking() || Ar.IsFilterEditorOnly()
		|| Ar.IsObjectReferenceCollector() || Ar.ShouldSkipBulkData()
		|| Ar.HasAnyPortFlags(PPF_Duplicate | PPF_DuplicateForPIE);
	if (bSkipSiteBulk)
	{
		if (bLoadingUnsupportedSiteArchive)
		{
			DiscardSiteData();
		}
		return;
	}
	constexpr int32 MaximumSerializedSiteChunkCount = 64 * 1024;
	const int32 ExpectedPayloadCount = SiteChunkManifest.Chunks.Num();
	if (Ar.IsSaving())
	{
		const int32 PayloadCount = SiteChunkPayloads.Num();
		if (PayloadCount < 0 || PayloadCount > MaximumSerializedSiteChunkCount
			|| PayloadCount != ExpectedPayloadCount)
		{
			UE_LOGF(LogArcEcotope, Error,"%ls cannot save invalid ArcEcotope site chunk payload count %d (expected %d).",
				*GetPathName(), PayloadCount, ExpectedPayloadCount);
			Ar.SetError();
			return;
		}
		int32 SerializedPayloadCount = PayloadCount;
		Ar << SerializedPayloadCount;
		for (UE::Serialization::FEditorBulkData& Payload : SiteChunkPayloads)
		{
			Payload.Serialize(Ar, this);
		}
		return;
	}
	int32 SerializedPayloadCount = 0;
	Ar << SerializedPayloadCount;
	if (SerializedPayloadCount < 0 || SerializedPayloadCount > MaximumSerializedSiteChunkCount)
	{
		UE_LOGF(LogArcEcotope, Error,"%ls contains an unrepresentable ArcEcotope site chunk payload count %d.",
			*GetPathName(), SerializedPayloadCount);
		DiscardSiteData();
		Ar.SetError();
		return;
	}
	TArray<UE::Serialization::FEditorBulkData> LoadedPayloads;
	LoadedPayloads.SetNum(SerializedPayloadCount);
	for (UE::Serialization::FEditorBulkData& Payload : LoadedPayloads)
	{
		Payload.Serialize(Ar, this);
	}
	if (bLoadingUnsupportedSiteArchive)
	{
		UE_LOGF(LogArcEcotope, Warning,"%ls discarded unsupported ArcEcotope site/state data from custom serialization version %d; Clear State, Bake Site, and Bootstrap and Spin Up are required.",
			*GetPathName(), ArchiveCustomVersion);
		DiscardSiteData();
		return;
	}
	if (SerializedPayloadCount != ExpectedPayloadCount)
	{
		UE_LOGF(LogArcEcotope, Error,"%ls discarded an authoritative ArcEcotope chunked site with %d payloads (expected %d); re-bake is required.",
			*GetPathName(), SerializedPayloadCount, ExpectedPayloadCount);
		DiscardSiteData();
		return;
	}
	SiteChunkPayloads = MoveTemp(LoadedPayloads);
#endif
}

void AArcEcotopeVolume::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITORONLY_DATA
	if (SiteMetadata.SiteSnapshotHash != 0
		&& SiteMetadata.SiteSchemaVersion != FArcEcotopeSiteMetadata::GetCurrentSiteSchemaVersion())
	{
		UE_LOGF(LogArcEcotope, Warning,"%ls holds a chunked site baked at schema version %d and this build bakes version %d. Re-bake the site before simulating.",
			*GetPathName(), SiteMetadata.SiteSchemaVersion, FArcEcotopeSiteMetadata::GetCurrentSiteSchemaVersion());
	}
#endif
	if (!HasAnyFlags(RF_ClassDefaultObject) && !VolumeId.IsValid())
	{
		VolumeId = FGuid::NewGuid();
	}
#if WITH_EDITOR
	FText SiteError;
	if (ArcEcotopeVolumePrivate::CanRepairAuthoredSite(*this) && !ValidateSiteChunkMetadata(SiteError))
	{
		UE_LOGF(LogArcEcotope, Error,"%ls discarded an invalid authoritative ArcEcotope chunked site and simulation state: %ls. Re-bake is required.",
			*GetPathName(), *SiteError.ToString());
		ClearBakedSiteAndSimulationState();
	}
#endif
}

#if WITH_EDITOR
bool AArcEcotopeVolume::ReplaceSiteWithValidatedChunks(
	FArcEcotopeSiteMetadata&& InMetadata,
	FArcEcotopeSiteChunkManifest&& InManifest,
	TArray<FSharedBuffer>&& InPayloads,
	FText& OutError)
{
	check(IsInGameThread());
	FString ValidationError;
	if (InPayloads.Num() != InManifest.Chunks.Num())
	{
		OutError = FText::FromString(TEXT("ArcEcotope chunk publication payload count does not match its manifest."));
		return false;
	}
	TArray<int64> InputPayloadSizes;
	InputPayloadSizes.Reserve(InPayloads.Num());
	for (const FSharedBuffer& Payload : InPayloads)
	{
		if (Payload.GetSize() > MAX_int64)
		{
			OutError = FText::FromString(TEXT("ArcEcotope chunk publication payload size is not representable."));
			return false;
		}
		InputPayloadSizes.Add(static_cast<int64>(Payload.GetSize()));
	}
	if (!FArcEcotopeSiteChunkStorage::ValidateManifestMetadata(
			InMetadata, InManifest, InputPayloadSizes, ValidationError))
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("ArcEcotope chunk publication metadata failed final validation: %s"), *ValidationError));
		return false;
	}
	for (int32 PayloadIndex = 0; PayloadIndex < InPayloads.Num(); ++PayloadIndex)
	{
		if (!FArcEcotopeSiteChunkStorage::ValidatePayloadBytes(
				InManifest.Chunks[PayloadIndex], InPayloads[PayloadIndex], ValidationError))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope chunk publication payload %d is corrupt: %s"),
				PayloadIndex, *ValidationError));
			return false;
		}
	}
	TArray<UE::Serialization::FEditorBulkData> NewPayloads;
	NewPayloads.SetNum(InPayloads.Num());
	for (int32 PayloadIndex = 0; PayloadIndex < InPayloads.Num(); ++PayloadIndex)
	{
		NewPayloads[PayloadIndex].UpdatePayload(InPayloads[PayloadIndex], this);
	}
	for (int32 PayloadIndex = 0; PayloadIndex < NewPayloads.Num(); ++PayloadIndex)
	{
		if (NewPayloads[PayloadIndex].GetPayloadSize() != InputPayloadSizes[PayloadIndex])
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope editor bulk payload %d changed size during staging."), PayloadIndex));
			return false;
		}
	}
	SiteMetadata = MoveTemp(InMetadata);
	SiteChunkManifest = MoveTemp(InManifest);
	SiteChunkPayloads = MoveTemp(NewPayloads);
	FoliageFieldPayload.Reset();
	OutError = FText::GetEmpty();
	return true;
}

void AArcEcotopeVolume::ClearSiteChunkAuthority()
{
	SiteChunkManifest.Reset();
	SiteChunkPayloads.Reset();
	if (SimulationState != nullptr)
	{
		SimulationState->ResetToEmpty();
	}
	InstanceSnapshotHash = 0;
	InstanceVisualHash = 0;
	InstanceComponentContentHash = 0;
	bInstanceRepresentationRequiresRebuild = false;
}

void AArcEcotopeVolume::ClearBakedSiteAndSimulationState()
{
	SiteMetadata = FArcEcotopeSiteMetadata();
	FoliageFieldPayload.Reset();
	ClearSiteChunkAuthority();
}

bool AArcEcotopeVolume::ValidateSiteChunkMetadata(FText& OutError) const
{
	if (SiteMetadata.SiteSnapshotHash == 0 && SiteChunkManifest.IsEmpty() && SiteChunkPayloads.IsEmpty())
	{
		OutError = FText::GetEmpty();
		return true;
	}
	TArray<int64> PayloadSizes;
	PayloadSizes.Reserve(SiteChunkPayloads.Num());
	for (const UE::Serialization::FEditorBulkData& Payload : SiteChunkPayloads)
	{
		PayloadSizes.Add(Payload.GetPayloadSize());
	}
	FString ValidationError;
	if (!FArcEcotopeSiteChunkStorage::ValidateManifestMetadata(
			SiteMetadata, SiteChunkManifest, PayloadSizes, ValidationError))
	{
		OutError = FText::FromString(MoveTemp(ValidationError));
		return false;
	}
	OutError = FText::GetEmpty();
	return true;
}

bool AArcEcotopeVolume::FreezeSiteChunkSource(
	FArcEcotopeFrozenSiteSourcePtr& OutSource,
	FText& OutError) const
{
	check(IsInGameThread());
	OutSource.Reset();
	if (SiteMetadata.SiteSnapshotHash == 0)
	{
		OutError = FText::FromString(TEXT("ArcEcotope has no baked chunked site to freeze."));
		return false;
	}
	FText MetadataError;
	if (!ValidateSiteChunkMetadata(MetadataError))
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("ArcEcotope cannot freeze invalid site chunk metadata: %s"), *MetadataError.ToString()));
		return false;
	}
	TSharedRef<FArcEcotopeFrozenSiteSource, ESPMode::ThreadSafe> Candidate =
		MakeShared<FArcEcotopeFrozenSiteSource, ESPMode::ThreadSafe>();
	Candidate->Metadata = SiteMetadata;
	Candidate->Manifest = SiteChunkManifest;
	FString LayoutError;
	if (!FArcEcotopeChunkLayout::Build(
			Candidate->Metadata, SiteChunkManifest.CoreBaseCellSpan, 0, Candidate->Layout, LayoutError))
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("ArcEcotope could not freeze its site chunk layout: %s"), *LayoutError));
		return false;
	}

	Candidate->Payloads.Reserve(SiteChunkPayloads.Num());
	for (int32 PayloadIndex = 0; PayloadIndex < SiteChunkPayloads.Num(); ++PayloadIndex)
	{
		const FArcEcotopeSiteChunkManifestEntry& Entry = SiteChunkManifest.Chunks[PayloadIndex];
		if (Entry.PayloadSize > MAX_int64)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope frozen site payload %d size is not representable."), PayloadIndex));
			return false;
		}
		FArcEcotopeFrozenSitePayload& Frozen = Candidate->Payloads.AddDefaulted_GetRef();
		Frozen.Data = SiteChunkPayloads[PayloadIndex].CopyTornOff();
		Frozen.ExpectedSize = static_cast<int64>(Entry.PayloadSize);
		Frozen.ExpectedHash = Entry.PayloadHash;
		Frozen.bMemoryOnly = Frozen.Data.IsMemoryOnlyPayload();
		Frozen.Data.UnloadData();
		Frozen.bLoadedAfterFreeze = Frozen.Data.IsPayloadLoaded();
		if (Frozen.Data.GetPayloadSize() != Frozen.ExpectedSize)
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope frozen site payload %d changed size during freeze."), PayloadIndex));
			return false;
		}
	}

	FArcEcotopeSiteChunkReader BoundaryReader;
	FString ReaderError;
	if (!BoundaryReader.OpenFrozen(
			Candidate, ArcEcotopeVolumePrivate::SiteValidationReaderBudgetBytes, ReaderError))
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("ArcEcotope frozen editor bulk source failed boundary validation: %s"), *ReaderError));
		return false;
	}

	OutSource = Candidate;
	OutError = FText::GetEmpty();
	return true;
}
#endif

void AArcEcotopeVolume::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
#if WITH_EDITOR
	// Editor-authoring fixup only: the climate gather walks TActorIterator, which sees loaded levels
	// alone, so a spatially loaded volume would silently drop out of a neighbour's bake.
	const UWorld* World = GetWorld();
	if (World != nullptr && World->WorldType == EWorldType::Editor && World->GetWorldPartition() != nullptr)
	{
		if (GetIsSpatiallyLoaded() && CanChangeIsSpatiallyLoadedFlag())
		{
			Modify();
			SetIsSpatiallyLoaded(false);
			UE_LOGF(
				LogArcEcotope,
				Log,"Forced simulation volume '%ls' to always-loaded on partitioned world '%ls'; its authored climate must stay resident for every bake that reads it.",
				*GetName(),
				*World->GetName());
		}
	}
#endif // WITH_EDITOR
}

int32 AArcEcotopeVolume::GetMaxStateRecords() const
{
	const int64 Total = static_cast<int64>(BudgetSettings.MaxLiveRecords)
		+ static_cast<int64>(BudgetSettings.MaxCoverRecords)
		+ static_cast<int64>(BudgetSettings.MaxDeadwoodRecords)
		+ static_cast<int64>(BudgetSettings.MaxBranchRecords);
	return static_cast<int32>(FMath::Clamp<int64>(Total, MIN_int32, MAX_int32));
}

uint64 AArcEcotopeVolume::ComputeSettingsHash() const
{
	using namespace ArcEcotopeVolumePrivate;

	const UArcEcotopeModel& EcotopeModel = GetEcotopeOrDefault();

	TArray<UE::ArcForest::FSettingsHashSpecies> EnabledSetups;
	for (const FArcEcotopeSpeciesSetup& Setup : EcotopeModel.SpeciesSetups)
	{
		if (!IsEnabledForSimulation(Setup))
		{
			continue;
		}

		UE::ArcForest::FSettingsHashSpecies& Value = EnabledSetups.AddDefaulted_GetRef();
		Value.SpeciesId = Setup.Species != nullptr ? Setup.Species->SpeciesId : FGuid();
		Value.RegionalArrivalPerHectarePerYear = Setup.RegionalArrivalPerHectarePerYear;
	}
	return UE::ArcForest::ComputeSettingsHash(
		Seed,
		CellSizeCm,
		RecordGridLevel,
		BudgetSettings,
		EcotopeModel.SoilPoolSettings,
		EcotopeModel.MaximumCrownOccupancy,
		EcotopeModel.UnderstoryShade,
		EcotopeModel.GroundCoverDensity,
		EcotopeModel.GroundCoverPatchiness,
		EnabledSetups,
		EcotopeModel.DisturbanceSettings);
}

TArray<FArcForestSpeciesEcologyHash> AArcEcotopeVolume::BuildEnabledSpeciesEcologyHashes() const
{
	using namespace ArcEcotopeVolumePrivate;

	TArray<FArcForestSpeciesEcologyHash> Result;
	for (const FArcEcotopeSpeciesSetup& Setup : GetEcotopeOrDefault().SpeciesSetups)
	{
		if (!IsEnabledForSimulation(Setup) || Setup.Species == nullptr)
		{
			continue;
		}

		FArcForestSpeciesEcologyHash& Entry = Result.AddDefaulted_GetRef();
		Entry.SpeciesId = Setup.Species->SpeciesId;
		Entry.EcologyHash = Setup.Species->ComputeEcologyHash();
	}
	Result.Sort(
		[](const FArcForestSpeciesEcologyHash& Left, const FArcForestSpeciesEcologyHash& Right)
		{
			if (Left.SpeciesId != Right.SpeciesId)
			{
				return GuidLess(Left.SpeciesId, Right.SpeciesId);
			}
			return Left.EcologyHash < Right.EcologyHash;
		});
	return Result;
}

uint64 AArcEcotopeVolume::ComputeEnabledSpeciesVisualHash() const
{
	using namespace ArcEcotopeVolumePrivate;

	TArray<TPair<FGuid, uint64>> Entries;
	for (const FArcEcotopeSpeciesSetup& Setup : GetEcotopeOrDefault().SpeciesSetups)
	{
		if (IsEnabledForSimulation(Setup) && Setup.Species != nullptr)
		{
			Entries.Emplace(Setup.Species->SpeciesId, Setup.Species->ComputeVisualHash());
		}
	}
	Entries.Sort([](const TPair<FGuid, uint64>& Left, const TPair<FGuid, uint64>& Right)
	{
		return Left.Key != Right.Key ? GuidLess(Left.Key, Right.Key) : Left.Value < Right.Value;
	});

	UE::ArcForest::FKeyedHash64 Hash(0xB6A74F3982D15CE0ull);
	Hash.AddInt32(Entries.Num());
	for (const TPair<FGuid, uint64>& Entry : Entries)
	{
		AddInstanceVisualHashComponent(Hash, Entry.Key, Entry.Value);
	}
	return Hash.Finalize();
}

void AArcEcotopeVolume::ClearInstanceRepresentation(const bool bRequiresRebuild)
{
	check(IsInGameThread());
	InstanceSnapshotHash = 0;
	InstanceVisualHash = 0;
	InstanceComponentContentHash = 0;
	bInstanceRepresentationRequiresRebuild = bRequiresRebuild;
}

FArcEcotopeSiteMetadata AArcEcotopeVolume::GetActiveSiteMetadata() const
{
	return SiteMetadata;
}

#if WITH_EDITOR
bool AArcEcotopeVolume::FreezeBootstrapInput(
	const EArcForestStartMode StartMode,
	FArcForestBootstrapInput& OutInput,
	FText& OutError) const
{
	using namespace ArcEcotopeVolumePrivate;

	if (!IsInGameThread())
	{
		OutError = FText::FromString(TEXT("ArcForest bootstrap inputs must be frozen on the game thread."));
		return false;
	}
	if (!VolumeId.IsValid())
	{
		OutError = FText::FromString(TEXT("ArcEcotope volume has no valid VolumeId."));
		return false;
	}

	const UArcEcotopeModel& EcotopeModel = GetEcotopeOrDefault();

	FText SoilPoolError;
	if (!EcotopeModel.SoilPoolSettings.IsValid(SoilPoolError))
	{
		OutError = FText::FromString(
			FString::Printf(TEXT("ArcForest bootstrap requires valid Ecotope.SoilPoolSettings: %s"), *SoilPoolError.ToString()));
		return false;
	}

	FText DisturbanceError;
	if (!EcotopeModel.DisturbanceSettings.IsValid(DisturbanceError))
	{
		OutError = FText::FromString(
			FString::Printf(TEXT("ArcForest bootstrap requires valid Ecotope.DisturbanceSettings: %s"), *DisturbanceError.ToString()));
		return false;
	}
	if (!FMath::IsFinite(EcotopeModel.MaximumCrownOccupancy) || EcotopeModel.MaximumCrownOccupancy <= 0.0)
	{
		OutError = FText::FromString(
			TEXT("ArcForest bootstrap requires a finite, strictly positive Ecotope.MaximumCrownOccupancy."));
		return false;
	}

	if (!FMath::IsFinite(EcotopeModel.UnderstoryShade) || EcotopeModel.UnderstoryShade < 0.05 || EcotopeModel.UnderstoryShade > 4.0)
	{
		OutError = FText::FromString(
			TEXT("ArcForest bootstrap requires a finite Ecotope.UnderstoryShade in [0.05, 4.0]."));
		return false;
	}

	if (!FMath::IsFinite(EcotopeModel.GroundCoverDensity) || EcotopeModel.GroundCoverDensity < 0.0 || EcotopeModel.GroundCoverDensity > 4.0)
	{
		OutError = FText::FromString(
			TEXT("ArcForest bootstrap requires a finite Ecotope.GroundCoverDensity in [0.0, 4.0]."));
		return false;
	}

	if (!FMath::IsFinite(EcotopeModel.GroundCoverPatchiness) || EcotopeModel.GroundCoverPatchiness < 0.0 || EcotopeModel.GroundCoverPatchiness > 0.95)
	{
		OutError = FText::FromString(
			TEXT("ArcForest bootstrap requires a finite Ecotope.GroundCoverPatchiness in [0.0, 0.95]."));
		return false;
	}

	if (BudgetSettings.MaxLiveRecords <= 0)
	{
		OutError = FText::FromString(
			TEXT("ArcForest bootstrap requires MaxLiveRecords greater than zero."));
		return false;
	}
	if (BudgetSettings.RecruitmentOversamplingFactor <= 0)
	{
		OutError = FText::FromString(
			TEXT("ArcForest bootstrap requires RecruitmentOversamplingFactor greater than zero."));
		return false;
	}

	if (BudgetSettings.MaxCoverRecords < 0)
	{
		OutError = FText::FromString(
			TEXT("ArcForest bootstrap requires a non-negative MaxCoverRecords."));
		return false;
	}

	if (static_cast<int64>(BudgetSettings.MaxLiveRecords)
			+ static_cast<int64>(BudgetSettings.MaxCoverRecords)
			+ static_cast<int64>(BudgetSettings.MaxDeadwoodRecords)
			+ static_cast<int64>(BudgetSettings.MaxBranchRecords) > MAX_int32)
	{
		OutError = FText::FromString(
			TEXT("ArcForest bootstrap requires MaxLiveRecords plus MaxCoverRecords plus MaxDeadwoodRecords plus MaxBranchRecords to not exceed 2147483647."));
		return false;
	}

	FArcForestBootstrapInput Candidate;
	Candidate.StartMode = StartMode;
	Candidate.VolumeId = VolumeId;
	Candidate.Seed = Seed;
	Candidate.MaxLiveStems = BudgetSettings.MaxLiveStems;
	Candidate.MaxLiveRecords = BudgetSettings.MaxLiveRecords;
	Candidate.RecruitmentOversamplingFactor = BudgetSettings.RecruitmentOversamplingFactor;
	Candidate.MaxCoverRecords = BudgetSettings.MaxCoverRecords;
	Candidate.MaxStateRecords = GetMaxStateRecords();
	Candidate.MaxDeadwoodRecords = BudgetSettings.MaxDeadwoodRecords;
	Candidate.MaxBranchRecords = BudgetSettings.MaxBranchRecords;
	Candidate.bEmitGrowthDiagnostics = bEmitGrowthDiagnostics;
	Candidate.GrowthDiagnosticsIntervalYears = FMath::Max(GrowthDiagnosticsIntervalYears, 1);
	Candidate.SettingsHash = ComputeSettingsHash();
	Candidate.DeadwoodSettings = EcotopeModel.SoilPoolSettings;
	Candidate.DisturbanceSettings = EcotopeModel.DisturbanceSettings;
	Candidate.MaximumCrownOccupancy = EcotopeModel.MaximumCrownOccupancy;
	Candidate.UnderstoryShade = EcotopeModel.UnderstoryShade;
	Candidate.GroundCoverDensity = EcotopeModel.GroundCoverDensity;
	Candidate.GroundCoverPatchiness = EcotopeModel.GroundCoverPatchiness;
	if (!FreezeSiteChunkSource(Candidate.SiteSource, OutError))
	{
		return false;
	}

	if (!Candidate.SiteSource.IsValid())
	{
		OutError = FText::FromString(TEXT("ArcForest bootstrap could not select immutable site storage."));
		return false;
	}
	const FArcEcotopeSiteMetadata& FrozenMetadata = Candidate.SiteSource->Metadata;
	if (FrozenMetadata.RecordGridLevel != RecordGridLevel)
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("The baked site records at grid level %d while the volume authors level %d. Re-apply the volume's RecordGridLevel to fix the disagreement."),
			static_cast<int32>(FrozenMetadata.RecordGridLevel),
			static_cast<int32>(RecordGridLevel)));
		return false;
	}

	Candidate.RegionalRecruitmentConfig.MaxProposalRounds = FMath::Clamp(
		CVarRegionalRecruitmentMaxProposalRounds.GetValueOnGameThread(), 1, 1024);
	if (!Candidate.RegionalRecruitmentConfig.IsValid())
	{
		OutError = FText::FromString(TEXT("ArcForest captured an invalid regional recruitment configuration."));
		return false;
	}

	TSet<FGuid> SeenSpeciesIds;
	for (int32 SetupIndex = 0; SetupIndex < EcotopeModel.SpeciesSetups.Num(); ++SetupIndex)
	{
		const FArcEcotopeSpeciesSetup& Setup = EcotopeModel.SpeciesSetups[SetupIndex];
		if (!IsEnabledForSimulation(Setup))
		{
			continue;
		}
		if (Setup.Species == nullptr || !Setup.Species->SpeciesId.IsValid())
		{
			OutError = FText::FromString(
				FString::Printf(TEXT("Enabled Ecotope.SpeciesSetups[%d] has no species with a valid SpeciesId."), SetupIndex));
			return false;
		}
		if (SeenSpeciesIds.Contains(Setup.Species->SpeciesId))
		{
			OutError = FText::FromString(
				FString::Printf(TEXT("Enabled Ecotope.SpeciesSetups[%d] duplicates a SpeciesId."), SetupIndex));
			return false;
		}
		SeenSpeciesIds.Add(Setup.Species->SpeciesId);

		// Derived here and hashed independently off the asset: the mismatch ValidateSpecies catches is the only guard on the two hash twins.
		ArcForestSpeciesModel::FArcForestDerivedEcology Derived;
		FText DerivationError;
		if (!ArcForestSpeciesModel::DeriveEcology(*Setup.Species, Derived, DerivationError))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("Enabled Ecotope.SpeciesSetups[%d] (%s) cannot solve its ecology: %s"),
				SetupIndex,
				*Setup.Species->GetPathName(),
				*DerivationError.ToString()));
			return false;
		}

		FArcForestFrozenSpeciesSetup& Frozen = Candidate.SpeciesSetups.AddDefaulted_GetRef();
		Frozen.SpeciesId = Setup.Species->SpeciesId;
		Frozen.EcologyHash = Setup.Species->ComputeEcologyHash();
		Frozen.bIndividualizes = Setup.Species->Individualizes();
		Frozen.InitialDensityPerHectare = Setup.InitialDensityPerHectare;
		Frozen.RegionalArrivalPerHectarePerYear = Setup.RegionalArrivalPerHectarePerYear;
		Frozen.TargetStemsPerHectare = Setup.Species->TargetStemsPerHectare;
		Frozen.TreeSizeM = Setup.Species->TreeSizeM;
		Frozen.Slenderness = Setup.Species->Slenderness;
		Frozen.GrowthSpeed = Setup.Species->GrowthSpeed;
		Frozen.Toughness = Setup.Species->Toughness;
		Frozen.ShadeTolerance = Setup.Species->ShadeTolerance;
		Frozen.PreferredTemperature = Setup.Species->PreferredTemperature;
		Frozen.TemperatureTolerance = Setup.Species->TemperatureTolerance;
		Frozen.PreferredHumidity = Setup.Species->PreferredHumidity;
		Frozen.MeanDispersalDistanceCm = Setup.Species->GetMeanDispersalDistanceCm();
		Frozen.ExclusionSensitivity = Setup.Species->ExclusionSensitivity;
		Frozen.RecruitsPerMatureStemPerYear = Setup.Species->GetRecruitsPerMatureStemPerYear();
		Frozen.MobilityMortalitySensitivity = Setup.Species->MobilityMortalitySensitivity;
		Frozen.LateralSpreadRatePerYear = Setup.Species->GetLateralSpreadRatePerYear();
		Frozen.IndividualizationHeightCm = Derived.IndividualizationHeightCm;
		Frozen.MaxHeightCm = Derived.MaxHeightCm;
		Frozen.MaxDbhCm = Derived.MaxDbhCm;
		Frozen.MaxAgeYears = Derived.MaxAgeYears;
		Frozen.MaturityAgeYears = Derived.MaturityAgeYears;
		Frozen.MaxAnnualHeightGrowthCm = Derived.MaxAnnualHeightGrowthCm;
		Frozen.MaxAnnualDbhGrowthCm = Derived.MaxAnnualDbhGrowthCm;
		Frozen.HeightDbhAllometryCoefficient = Derived.HeightDbhAllometryCoefficient;
		Frozen.HeightDbhAllometryExponent = Derived.HeightDbhAllometryExponent;
		Frozen.CrownRadiusHeightRatio = Derived.CrownRadiusHeightRatio;
		Frozen.BiomassAllometryCoefficient = Derived.BiomassAllometryCoefficient;
		Frozen.BiomassAllometryExponent = Derived.BiomassAllometryExponent;
		Frozen.CrownOccupancyWeight = Derived.CrownOccupancyWeight;
		Frozen.BaseAnnualMortalityProbability = Derived.BaseAnnualMortalityProbability;
		Frozen.OldAgeMortalityProbabilityAtMaxAge = Derived.OldAgeMortalityProbabilityAtMaxAge;
		Frozen.StressMortalityMaximum = Derived.StressMortalityMaximum;
		Frozen.StressMortalityVigorThreshold = Derived.StressMortalityVigorThreshold;
		Frozen.StressMortalitySteepness = Derived.StressMortalitySteepness;
		Frozen.StressMortalityCanopyFloor = Derived.StressMortalityCanopyFloor;
		Frozen.VigorSmoothingRetention = Derived.VigorSmoothingRetention;
		Frozen.AnnualPropaguleProduction = Derived.AnnualPropaguleProduction;
		Frozen.LightResponse = FreezeResponse(Derived.LightResponse);
		Frozen.MoistureResponse = FreezeResponse(Derived.MoistureResponse);
		Frozen.TemperatureResponse = FreezeResponse(Derived.TemperatureResponse);
		Frozen.MaxSlopeDeg = Derived.MaxSlopeDeg;
		Frozen.SunLoadMin = Derived.SunLoadMin;
		Frozen.WindExposureMax = Derived.WindExposureMax;
		Frozen.FertilityMin = Derived.FertilityMin;
	}

	if (StartMode == EArcForestStartMode::ContinueSnapshot)
	{
		if (SimulationState == nullptr)
		{
			OutError = FText::FromString(TEXT("ContinueSnapshot requires a simulation state object."));
			return false;
		}
		Candidate.SourceSnapshot = SimulationState->Snapshot;
	}

	OutInput = MoveTemp(Candidate);
	OutError = FText::GetEmpty();
	return true;
}
#endif

#if WITH_EDITOR
bool AArcEcotopeVolume::CommitInstanceRepresentation(
	const uint64 InSnapshotHash,
	const uint64 InVisualHash,
	FText& OutError)
{
	check(IsInGameThread());
	uint64 NewContentHash = 0;
	const EArcEcotopeContentHashResult HashResult = ComputeInstanceComponentContentHash(NewContentHash, OutError);
	if (HashResult == EArcEcotopeContentHashResult::Skipped)
	{
		OutError = FText::FromString(
			TEXT("ArcEcotope cannot record a representation while an instance partition actor it owns is unloaded."));
	}
	if (HashResult != EArcEcotopeContentHashResult::Computed)
	{
		return false;
	}
	InstanceSnapshotHash = InSnapshotHash;
	InstanceVisualHash = InVisualHash;
	InstanceComponentContentHash = NewContentHash;
	bInstanceRepresentationRequiresRebuild = false;
	OutError = FText::GetEmpty();
	return true;
}

EArcEcotopeContentHashResult AArcEcotopeVolume::ComputeInstanceComponentContentHash(
	uint64& OutHash,
	FText& OutError) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcEcotope_ComputeInstanceComponentContentHash);
	OutHash = 0;
	OutError = FText::GetEmpty();
	if (InstancePartitionActors.IsEmpty())
	{
		// The same zero a cleared representation records, so "owns no geometry" and "cleared" agree.
		return EArcEcotopeContentHashResult::Computed;
	}

	TArray<uint64> ActorHashes;
	ActorHashes.Reserve(InstancePartitionActors.Num());
	for (const FArcEcotopeEmittedActor& Record : InstancePartitionActors)
	{
		AActor* RecordedActor = Record.Actor.Get();
		const AArcEcotopeInstancePartitionActor* Actor =
			Cast<AArcEcotopeInstancePartitionActor>(RecordedActor);
		if (Actor == nullptr)
		{
			if (RecordedActor != nullptr)
			{
				OutError = FText::FromString(FString::Printf(
					TEXT("Recorded instance partition actor '%s' is a %s."),
					*Record.Actor.ToString(),
					*RecordedActor->GetClass()->GetName()));
				return EArcEcotopeContentHashResult::Failed;
			}
			// Resolving it through World Partition here would stream a cell in from validation or undo.
			return EArcEcotopeContentHashResult::Skipped;
		}
		uint64 ActorHash = 0;
		if (!UE::ArcEcotope::TryComputeContentHash(
			Actor->StaticMeshInstances, Actor->SkinnedMeshInstances, ActorHash, OutError))
		{
			return EArcEcotopeContentHashResult::Failed;
		}
		ActorHashes.Add(ActorHash);
	}
	// Sorted so the fold does not depend on the order the bake happened to record the actors in.
	ActorHashes.Sort();

	UE::ArcForest::FKeyedHash64 Hash(0x4F1B39C7D2A56E80ull);
	Hash.AddInt32(ActorHashes.Num());
	for (const uint64 ActorHash : ActorHashes)
	{
		Hash.AddUInt64(ActorHash);
	}
	OutHash = Hash.Finalize();
	return EArcEcotopeContentHashResult::Computed;
}

void AArcEcotopeVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const bool bTouchesRecordGridLevel =
		PropertyChangedEvent.MemberProperty == nullptr
		|| PropertyChangedEvent.GetMemberPropertyName()
		== GET_MEMBER_NAME_CHECKED(AArcEcotopeVolume, RecordGridLevel);
	const bool bSiteIsCurrent =
		SiteMetadata.SiteSnapshotHash != 0
		&& SiteMetadata.SiteSchemaVersion == FArcEcotopeSiteMetadata::GetCurrentSiteSchemaVersion();
	if (bTouchesRecordGridLevel && bSiteIsCurrent
		&& RecordGridLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
		&& SiteMetadata.RecordGridLevel != RecordGridLevel)
	{
		ClearBakedSiteAndSimulationState();
	}
	const bool bTouchesChunkSpan =
		PropertyChangedEvent.MemberProperty == nullptr
		|| PropertyChangedEvent.GetMemberPropertyName()
		== GET_MEMBER_NAME_CHECKED(AArcEcotopeVolume, ChunkBaseCellSpan);
	if (bTouchesChunkSpan && !SiteChunkManifest.IsEmpty()
		&& SiteChunkManifest.CoreBaseCellSpan != ChunkBaseCellSpan)
	{
		ClearBakedSiteAndSimulationState();
	}
}

void AArcEcotopeVolume::PostDuplicate(const bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (bDuplicateForPIE)
	{
		// PIE duplication deliberately skips the editor-only bulk payload stream, so the copy keeps the site
		// scalars and the foliage field the runtime reads but holds no chunk authority.
		EmittedEntityActors.Empty();
		InstancePartitionActors.Empty();
		ClearSiteChunkAuthority();
	}
	else if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		VolumeId = FGuid::NewGuid();
		// The copied records name the source volume's actors, which the new identity does not own.
		EmittedEntityActors.Empty();
		InstancePartitionActors.Empty();
		ClearBakedSiteAndSimulationState();
	}
}

void AArcEcotopeVolume::PostEditImport()
{
	Super::PostEditImport();

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		VolumeId = FGuid::NewGuid();
		EmittedEntityActors.Empty();
		InstancePartitionActors.Empty();
		// Text import cannot carry the manually serialized non-UPROPERTY payloads.
		ClearBakedSiteAndSimulationState();
	}
}

void AArcEcotopeVolume::PostEditUndo()
{
	Super::PostEditUndo();
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	FText SiteError;
	if (!ValidateSiteChunkMetadata(SiteError))
	{
		ClearBakedSiteAndSimulationState();
	}

	bool bDerivedDataMismatch = SimulationState == nullptr;
	if (SimulationState != nullptr)
	{
		const bool bAuthoritativeStateIsEmpty =
			SimulationState->Status == EArcForestSimulationStateStatus::Empty
			|| SimulationState->Snapshot.IsEmpty();
		if (bAuthoritativeStateIsEmpty)
		{
			bDerivedDataMismatch =
				!InstancePartitionActors.IsEmpty()
				|| !EmittedEntityActors.IsEmpty()
				|| InstanceSnapshotHash != 0
				|| InstanceVisualHash != 0
				|| InstanceComponentContentHash != 0
				|| bInstanceRepresentationRequiresRebuild;
		}
		else
		{
			bDerivedDataMismatch =
				InstanceSnapshotHash != SimulationState->Snapshot.StateChecksum
				|| InstanceVisualHash != ComputeEnabledSpeciesVisualHash();
			uint64 ActualContentHash = 0;
			FText ContentError;
			const EArcEcotopeContentHashResult HashResult =
				ComputeInstanceComponentContentHash(ActualContentHash, ContentError);
			bDerivedDataMismatch |=
				HashResult == EArcEcotopeContentHashResult::Failed
				|| (HashResult == EArcEcotopeContentHashResult::Computed
					&& ActualContentHash != InstanceComponentContentHash);
		}
	}

	if (bDerivedDataMismatch)
	{
		// The restored records and the geometry standing on the partition actors can only be reconciled by
		// a rebuild; destroying actors in other packages from an undo pass is not itself undoable.
		const bool bRequiresRebuild =
			SimulationState != nullptr && !SimulationState->Snapshot.IsEmpty();
		ClearInstanceRepresentation(bRequiresRebuild);
	}
}

EDataValidationResult AArcEcotopeVolume::IsDataValid(FDataValidationContext& Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcEcotope_VolumeIsDataValid);
	UE::ArcForest::FDiagnosticScopeReport Report(TEXT("ArcEcotope Volume::IsDataValid"));
	Report.AddDetail(FString::Printf(
		TEXT("owned actors: instance=%d entity=%d"),
		InstancePartitionActors.Num(),
		EmittedEntityActors.Num()));
	EDataValidationResult Result = Super::IsDataValid(Context);

	const TFunction<void(const FString&)> AddError = [&Context, &Result](const FString& Message) -> void
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	};
	const TFunction<void(const FString&)> AddWarning = [&Context](const FString& Message) -> void
	{
		Context.AddWarning(FText::FromString(Message));
	};

	if (!VolumeId.IsValid())
	{
		AddError(TEXT("VolumeId must be a valid GUID."));
	}

	if (!FMath::IsFinite(CellSizeCm) || CellSizeCm <= 0.0f)
	{
		AddError(TEXT("CellSizeCm must be finite and greater than zero."));
	}
	if (YearsPerRun <= 0)
	{
		AddError(TEXT("YearsPerRun must be greater than zero."));
	}
	if (SpinUpYears < 0)
	{
		AddError(TEXT("SpinUpYears cannot be negative."));
	}
	if (BudgetSettings.MaxLiveStems <= 0)
	{
		AddError(TEXT("MaxLiveStems must be greater than zero."));
	}
	if (BudgetSettings.MaxLiveRecords <= 0)
	{
		AddError(TEXT("MaxLiveRecords must be greater than zero."));
	}
	if (BudgetSettings.RecruitmentOversamplingFactor <= 0)
	{
		AddError(TEXT("RecruitmentOversamplingFactor must be greater than zero."));
	}
	if (BudgetSettings.MaxCoverRecords < 0)
	{
		AddError(TEXT("MaxCoverRecords cannot be negative."));
	}
	if (BudgetSettings.MaxDeadwoodRecords < 0)
	{
		AddError(TEXT("MaxDeadwoodRecords cannot be negative."));
	}
	else if (BudgetSettings.MaxBranchRecords < 0)
	{
		AddError(TEXT("MaxBranchRecords cannot be negative."));
	}
	else if (static_cast<int64>(BudgetSettings.MaxLiveRecords)
			+ static_cast<int64>(BudgetSettings.MaxCoverRecords)
			+ static_cast<int64>(BudgetSettings.MaxDeadwoodRecords)
			+ static_cast<int64>(BudgetSettings.MaxBranchRecords) > MAX_int32)
	{
		AddError(TEXT("MaxLiveRecords plus MaxCoverRecords plus MaxDeadwoodRecords plus MaxBranchRecords must not exceed 2147483647."));
	}

	if (!FMath::IsFinite(MaxInvalidFraction) || MaxInvalidFraction < 0.0f || MaxInvalidFraction > 1.0f)
	{
		AddError(TEXT("MaxInvalidFraction must be finite and inside [0, 1]."));
	}

	if (RecordGridLevel > FArcEcotopeSiteMetadata::MaxGridLevel)
	{
		AddError(FString::Printf(
			TEXT("RecordGridLevel must not exceed the supported maximum grid level of %d."),
			static_cast<int32>(FArcEcotopeSiteMetadata::MaxGridLevel)));
	}
	if (ChunkBaseCellSpan <= 0)
	{
		AddError(TEXT("ChunkBaseCellSpan must be greater than zero."));
	}
	else if (RecordGridLevel <= FArcEcotopeSiteMetadata::MaxGridLevel)
	{
		const int32 RecordGridDivisor = static_cast<int32>(1) << RecordGridLevel;
		if (ChunkBaseCellSpan % RecordGridDivisor != 0)
		{
			AddError(FString::Printf(
				TEXT("ChunkBaseCellSpan must be divisible by the record-grid divisor %d."),
				RecordGridDivisor));
		}
	}
	FText SiteFieldSettingsError;
	if (!SiteFieldSettings.IsValid(SiteFieldSettingsError))
	{
		AddError(FString::Printf(TEXT("SiteFieldSettings is invalid: %s"), *SiteFieldSettingsError.ToString()));
	}

	FText TerrainDataSettingsError;
	if (!TerrainDataSettings.IsValid(TerrainDataSettingsError))
	{
		AddError(FString::Printf(TEXT("TerrainDataSettings is invalid: %s"), *TerrainDataSettingsError.ToString()));
	}

	if (Ecotope == nullptr)
	{
		AddError(TEXT("Ecotope must be set."));
	}

	if (!FMath::IsFinite(EdgeGradientBandCm) || EdgeGradientBandCm < 0.0)
	{
		AddError(TEXT("EdgeGradientBandCm must be finite and cannot be negative."));
	}

	if (!FMath::IsFinite(EdgeGradientSharpness) || EdgeGradientSharpness < 0.0)
	{
		AddError(TEXT("EdgeGradientSharpness must be finite and cannot be negative."));
	}

	const FVector ActorScale = GetActorScale3D();
	if (!FMath::IsFinite(ActorScale.X)
		|| !FMath::IsFinite(ActorScale.Y)
		|| !FMath::IsFinite(ActorScale.Z)
		|| ActorScale.X <= 0.0
		|| ActorScale.Y <= 0.0
		|| ActorScale.Z <= 0.0
		|| !FMath::IsNearlyEqual(ActorScale.X, ActorScale.Y)
		|| !FMath::IsNearlyEqual(ActorScale.X, ActorScale.Z))
	{
		AddError(TEXT("ArcEcotope volumes require finite, positive, uniform actor scale."));
	}

	if (VolumeBounds == nullptr)
	{
		AddError(TEXT("VolumeBounds component is missing."));
	}
	else
	{
		const FVector Extent = VolumeBounds->GetUnscaledBoxExtent();
		if (!FMath::IsFinite(Extent.X)
			|| !FMath::IsFinite(Extent.Y)
			|| !FMath::IsFinite(Extent.Z)
			|| Extent.X <= 0.0
			|| Extent.Y <= 0.0
			|| Extent.Z <= 0.0)
		{
			AddError(TEXT("VolumeBounds must have finite, positive extents."));
		}
	}

	if (SimulationState == nullptr)
	{
		AddError(TEXT("SimulationState subobject is missing."));
	}
	else if (SimulationState->GetOuter() != this)
	{
		AddError(TEXT("SimulationState must be owned by this simulation volume."));
	}

	FArcEcotopeSiteMetadata ValidationSiteMetadata;
	FArcEcotopeChunkLayout ValidationSiteLayout;
	FArcEcotopeSiteChunkReader ValidationSiteReader;
	bool bValidationSiteAvailable = false;
	const double SiteValidationStartSeconds = FPlatformTime::Seconds();
	const bool bHasAnySiteData = SiteMetadata.SiteSnapshotHash != 0
		|| !SiteChunkManifest.IsEmpty() || !SiteChunkPayloads.IsEmpty();
	if (bHasAnySiteData)
	{
		FArcEcotopeFrozenSiteSourcePtr Source;
		FText FreezeError;
		if (!FreezeSiteChunkSource(Source, FreezeError))
		{
			AddError(FString::Printf(TEXT("Site chunk source is invalid: %s"), *FreezeError.ToString()));
		}
		else
		{
			FArcEcotopeSiteBulkValidationResult SourceResult;
			FArcEcotopeSiteBulkReaderStats ReaderStats;
			FString SourceError;
			if (!FArcEcotopeSiteChunkReader::ValidateEditorBulkSource(
					Source, ArcEcotopeVolumePrivate::SiteValidationReaderBudgetBytes,
					ValidationSiteReader, SourceResult, ReaderStats, SourceError))
			{
				AddError(FString::Printf(TEXT("Site chunk source is invalid: %s"), *SourceError));
			}
			else
			{
				ValidationSiteMetadata = Source->Metadata;
				ValidationSiteLayout = Source->Layout;
				bValidationSiteAvailable = true;
				UE_LOGF(LogArcEcotope, Log,"ArcEcotope site source validation | hash=%llu logical=%llu values=%lld acquires=%lld validationScratchPeak=%.2fMiB readerPeak=%.2fMiB readerBudget=%.2fMiB status=PASS",
					SourceResult.CanonicalHash, SourceResult.LogicalFieldHash,
					SourceResult.LogicalValueCount, SourceResult.AcquireCount,
					static_cast<double>(SourceResult.PeakValidationScratchBytes) / (1024.0 * 1024.0),
					static_cast<double>(ReaderStats.PeakTotalBytes) / (1024.0 * 1024.0),
					static_cast<double>(ReaderStats.BudgetBytes) / (1024.0 * 1024.0));
			}
		}
	}
	if (bValidationSiteAvailable && ValidationSiteMetadata.RecordGridLevel != RecordGridLevel)
	{
		AddError(FString::Printf(
			TEXT("The baked site records at grid level %d while RecordGridLevel is %d. Re-apply the volume's RecordGridLevel to fix the disagreement."),
			static_cast<int32>(ValidationSiteMetadata.RecordGridLevel),
			static_cast<int32>(RecordGridLevel)));
		bValidationSiteAvailable = false;
	}
	Report.GetPhases().AddSample(
		TEXT("IsDataValid_ValidateSiteStorage"),
		UE::ArcForest::GetElapsedMilliseconds(SiteValidationStartSeconds));

	if (SimulationState != nullptr && SimulationState->GetOuter() == this)
	{
		const uint64 ExpectedSettingsHash = ComputeSettingsHash();
		const TArray<FArcForestSpeciesEcologyHash> ExpectedSpeciesEcologyHashes =
			BuildEnabledSpeciesEcologyHashes();
		const FGuid ExpectedSimulationId =
			VolumeId.IsValid() ? UE::ArcForest::MakeSimulationId(VolumeId, Seed) : FGuid();

		FName WorldPackageName = NAME_None;
		if (const UWorld* World = GetWorld())
		{
			WorldPackageName = World->GetPackage()->GetFName();
		}

		FText StateError;
		const double EvaluateStatusStartSeconds = FPlatformTime::Seconds();
		EArcForestSimulationStateStatus StateStatus =
			SimulationState->EvaluateStatus(
				StateError,
				WorldPackageName,
				ExpectedSettingsHash,
				&ExpectedSpeciesEcologyHashes,
				ExpectedSimulationId);
		Report.GetPhases().AddSample(
			TEXT("IsDataValid_EvaluateStatus"),
			UE::ArcForest::GetElapsedMilliseconds(EvaluateStatusStartSeconds));
		bool bStateErrorAlreadyReported = false;
		if (StateStatus == EArcForestSimulationStateStatus::Valid)
		{
			if (!bValidationSiteAvailable)
			{
				AddError(TEXT("SimulationState is incompatible: a nonempty state requires a valid baked site source."));
				StateStatus = EArcForestSimulationStateStatus::Incompatible;
				bStateErrorAlreadyReported = true;
			}
			else
			{
				const double CompatibilityStartSeconds = FPlatformTime::Seconds();
				FArcForestWorldSiteValidationStats CompatibilityStats;
				FText CompatibilityError;
				if (!SimulationState->Snapshot.ValidateSiteCompatibility(
						ValidationSiteMetadata,
						ValidationSiteLayout,
						ValidationSiteReader,
						ArcEcotopeVolumePrivate::SiteCompatibilityScheduleBudgetBytes,
						CompatibilityStats,
						CompatibilityError))
				{
					AddError(FString::Printf(
						TEXT("SimulationState is incompatible with the baked site: %s"),
						*CompatibilityError.ToString()));
					StateStatus = EArcForestSimulationStateStatus::Incompatible;
					bStateErrorAlreadyReported = true;
				}
				else
				{
					UE_LOGF(LogArcEcotope, Log,"ArcEcotope world/site compatibility | chunks=%d/%d records=%lld/%lld/%lld/%lld acquires=%lld failed=%lld schedule=%.2fMiB peakSchedule=%.2fMiB budget=%.2fMiB status=PASS",
						CompatibilityStats.ActiveChunkCount,
						CompatibilityStats.TotalChunkCount,
						CompatibilityStats.IndividualCount,
						CompatibilityStats.CohortCount,
						CompatibilityStats.PatchCount,
						CompatibilityStats.DeadwoodCount,
						CompatibilityStats.AcquireCount,
						CompatibilityStats.FailedAcquireCount,
						static_cast<double>(CompatibilityStats.ScheduleBytes) / (1024.0 * 1024.0),
						static_cast<double>(CompatibilityStats.PeakScheduleBytes) / (1024.0 * 1024.0),
						static_cast<double>(CompatibilityStats.ScheduleBudgetBytes) / (1024.0 * 1024.0));
				}
				Report.GetPhases().AddSample(
					TEXT("IsDataValid_ValidateSiteCompatibility"),
					UE::ArcForest::GetElapsedMilliseconds(CompatibilityStartSeconds));
			}
		}
		if (StateStatus == EArcForestSimulationStateStatus::Invalid
			|| StateStatus == EArcForestSimulationStateStatus::InvalidVersion)
		{
			AddError(FString::Printf(TEXT("SimulationState is invalid: %s"), *StateError.ToString()));
		}
		else if (StateStatus == EArcForestSimulationStateStatus::Incompatible
			&& !bStateErrorAlreadyReported)
		{
			AddError(FString::Printf(TEXT("SimulationState is incompatible: %s"), *StateError.ToString()));
		}

		const bool bOwnsEmittedActors =
			!InstancePartitionActors.IsEmpty() || !EmittedEntityActors.IsEmpty();
		const TFunction<void()> ValidateContentHash = [this, &AddError]() -> void
		{
			uint64 ActualContentHash = 0;
			FText ContentError;
			const EArcEcotopeContentHashResult HashResult =
				ComputeInstanceComponentContentHash(ActualContentHash, ContentError);
			if (HashResult == EArcEcotopeContentHashResult::Failed)
			{
				AddError(FString::Printf(
					TEXT("Saved ArcEcotope instance content cannot be hashed: %s"),
					*ContentError.ToString()));
			}
			else if (HashResult == EArcEcotopeContentHashResult::Computed
				&& ActualContentHash != InstanceComponentContentHash)
			{
				AddError(TEXT("Saved ArcEcotope instance contents do not match InstanceComponentContentHash."));
			}
		};

		if (StateStatus == EArcForestSimulationStateStatus::Valid)
		{
			if (bInstanceRepresentationRequiresRebuild)
			{
				if (InstanceSnapshotHash != 0
					|| InstanceVisualHash != 0
					|| InstanceComponentContentHash != 0)
				{
					AddError(TEXT("ArcForest rebuild-required state must not retain partial representation hashes."));
				}
				AddWarning(TEXT("ArcForest authoritative snapshot is valid, but Rebuild Instances is required before it has a complete saved runtime representation."));
			}
			else
			{
				if (InstanceSnapshotHash != SimulationState->Snapshot.StateChecksum)
				{
					AddError(TEXT("Saved ArcEcotope instances do not match the simulation snapshot checksum."));
				}
				if (InstanceVisualHash != ComputeEnabledSpeciesVisualHash())
				{
					AddError(TEXT("Saved ArcEcotope instances do not match the enabled species visual configuration."));
				}
				ValidateContentHash();
			}
		}
		else if (StateStatus == EArcForestSimulationStateStatus::Empty
			&& (bOwnsEmittedActors
				|| InstanceSnapshotHash != 0
				|| InstanceVisualHash != 0
				|| InstanceComponentContentHash != 0
				|| bInstanceRepresentationRequiresRebuild))
		{
			AddError(TEXT("An empty ArcForest state cannot retain emitted partition actors or representation hashes."));
		}
		else if (StateStatus != EArcForestSimulationStateStatus::Valid
			&& (bOwnsEmittedActors || InstanceComponentContentHash != 0))
		{
			ValidateContentHash();
		}

		for (const FArcEcotopeEmittedActor& Record : InstancePartitionActors)
		{
			const AArcEcotopeInstancePartitionActor* Actor =
				Cast<AArcEcotopeInstancePartitionActor>(Record.Actor.Get());
			if (Actor != nullptr && Actor->OwnerGuid != VolumeId)
			{
				AddError(FString::Printf(
					TEXT("Instance partition actor '%s' is recorded by this volume but names another owner."),
					*Actor->GetName()));
			}
		}
	}

	return Result;
}
#endif
