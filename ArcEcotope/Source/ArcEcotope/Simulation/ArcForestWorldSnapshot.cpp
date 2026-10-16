// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcForestWorldSnapshot.h"

#include "Diagnostics/ArcForestDiagnostics.h"
#include "Simulation/ArcEcotopeFrozenSiteSource.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"

namespace ArcForestWorldSnapshotPrivate
{
	constexpr int32 CurrentSchemaVersion = 9;
	constexpr uint64 FnvOffsetBasis = 14695981039346656037ull;
	constexpr uint64 FnvPrime = 1099511628211ull;

	class FCanonicalHash64
	{
	public:
		void AddByte(const uint8 Value)
		{
			Hash ^= Value;
			Hash *= FnvPrime;
		}

		void AddUInt32(const uint32 Value)
		{
			for (int32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
			{
				AddByte(static_cast<uint8>(Value >> (ByteIndex * 8)));
			}
		}

		void AddInt32(const int32 Value)
		{
			AddUInt32(static_cast<uint32>(Value));
		}

		void AddUInt64(const uint64 Value)
		{
			for (int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
			{
				AddByte(static_cast<uint8>(Value >> (ByteIndex * 8)));
			}
		}

		void AddInt64(const int64 Value)
		{
			AddUInt64(static_cast<uint64>(Value));
		}

		void AddFloat(float Value)
		{
			if (Value == 0.0f)
			{
				Value = 0.0f;
			}

			uint32 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			AddUInt32(Bits);
		}

		void AddDouble(double Value)
		{
			if (Value == 0.0)
			{
				Value = 0.0;
			}

			uint64 Bits = 0;
			FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
			AddUInt64(Bits);
		}

		void AddGuid(const FGuid& Value)
		{
			AddUInt32(Value.A);
			AddUInt32(Value.B);
			AddUInt32(Value.C);
			AddUInt32(Value.D);
		}

		void AddVector(const FVector& Value)
		{
			AddDouble(Value.X);
			AddDouble(Value.Y);
			AddDouble(Value.Z);
		}

		void AddQuat(const FQuat& Value)
		{
			AddDouble(Value.X);
			AddDouble(Value.Y);
			AddDouble(Value.Z);
			AddDouble(Value.W);
		}

		void AddTransform(const FTransform& Value)
		{
			AddQuat(Value.GetRotation());
			AddVector(Value.GetTranslation());
			AddVector(Value.GetScale3D());
		}

		uint64 Finalize() const
		{
			return Hash;
		}

	private:
		uint64 Hash = FnvOffsetBasis;
	};

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

	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool IsFiniteTransform(const FTransform& Value)
	{
		const FQuat Rotation = Value.GetRotation();
		return FMath::IsFinite(Rotation.X)
			&& FMath::IsFinite(Rotation.Y)
			&& FMath::IsFinite(Rotation.Z)
			&& FMath::IsFinite(Rotation.W)
			&& Rotation.IsNormalized()
			&& IsFiniteVector(Value.GetTranslation())
			&& IsFiniteVector(Value.GetScale3D());
	}

	template <typename ValueType>
	void NormalizeSignedZero(ValueType& Value)
	{
		if (Value == static_cast<ValueType>(0))
		{
			Value = static_cast<ValueType>(0);
		}
	}

	void NormalizeVector(FVector& Value)
	{
		NormalizeSignedZero(Value.X);
		NormalizeSignedZero(Value.Y);
		NormalizeSignedZero(Value.Z);
	}

	bool ShouldNegateQuaternion(const FQuat& Rotation)
	{
		return Rotation.W < 0.0
			|| (Rotation.W == 0.0 && Rotation.Z < 0.0)
			|| (Rotation.W == 0.0 && Rotation.Z == 0.0 && Rotation.Y < 0.0)
			|| (Rotation.W == 0.0 && Rotation.Z == 0.0 && Rotation.Y == 0.0 && Rotation.X < 0.0);
	}

	void NormalizeTransform(FTransform& Transform)
	{
		FQuat Rotation = Transform.GetRotation();
		if (ShouldNegateQuaternion(Rotation))
		{
			Rotation.X = -Rotation.X;
			Rotation.Y = -Rotation.Y;
			Rotation.Z = -Rotation.Z;
			Rotation.W = -Rotation.W;
		}
		NormalizeSignedZero(Rotation.X);
		NormalizeSignedZero(Rotation.Y);
		NormalizeSignedZero(Rotation.Z);
		NormalizeSignedZero(Rotation.W);

		FVector Translation = Transform.GetTranslation();
		FVector Scale = Transform.GetScale3D();
		NormalizeVector(Translation);
		NormalizeVector(Scale);
		Transform = FTransform(Rotation, Translation, Scale);
	}

	template <typename RecordType, typename IdAccessorType>
	bool HasStrictlyIncreasingIds(const TArray<RecordType>& Records, IdAccessorType IdAccessor)
	{
		for (int32 Index = 1; Index < Records.Num(); ++Index)
		{
			if (IdAccessor(Records[Index - 1]) >= IdAccessor(Records[Index]))
			{
				return false;
			}
		}
		return true;
	}

	template <typename RecordType, typename IdAccessorType>
	TArray<const RecordType*> MakeCanonicalRecordView(
		const TArray<RecordType>& Records,
		IdAccessorType IdAccessor)
	{
		TArray<const RecordType*> Result;
		Result.Reserve(Records.Num());
		for (const RecordType& Record : Records)
		{
			Result.Add(&Record);
		}
		Result.Sort(
			[IdAccessor](const RecordType& Left, const RecordType& Right)
			{
				return IdAccessor(Left) < IdAccessor(Right);
			});
		return Result;
	}

	void SetError(FText& OutError, const FString& Message)
	{
		OutError = FText::FromString(Message);
	}

	struct FWorldChunkRecordSchedule
	{
		TArray<int32> Offsets;
		TArray<int32> RecordIndices;

		TConstArrayView<int32> GetRecords(const int32 ChunkIndex) const
		{
			if (!Offsets.IsValidIndex(ChunkIndex) || !Offsets.IsValidIndex(ChunkIndex + 1))
			{
				return {};
			}
			const int32 Begin = Offsets[ChunkIndex];
			const int32 End = Offsets[ChunkIndex + 1];
			return MakeArrayView(RecordIndices).Slice(Begin, End - Begin);
		}

		int64 GetAllocatedBytes() const
		{
			const uint64 Bytes = static_cast<uint64>(Offsets.GetAllocatedSize())
				+ static_cast<uint64>(RecordIndices.GetAllocatedSize());
			return Bytes <= static_cast<uint64>(MAX_int64) ? static_cast<int64>(Bytes) : MAX_int64;
		}
	};

	bool TryAddInt32ArrayLowerBound(const int32 Num, uint64& InOutBytes)
	{
		if (Num < 0 || static_cast<uint64>(Num) > MAX_uint64 / sizeof(int32))
		{
			return false;
		}
		const uint64 Bytes = static_cast<uint64>(Num) * sizeof(int32);
		if (InOutBytes > MAX_uint64 - Bytes)
		{
			return false;
		}
		InOutBytes += Bytes;
		return true;
	}

	template <typename RecordType, typename ResolveChunkType>
	bool BuildWorldChunkSchedule(
		const TConstArrayView<RecordType> Records,
		const int32 ChunkCount,
		ResolveChunkType&& ResolveChunk,
		const TCHAR* RecordKind,
		FWorldChunkRecordSchedule& OutSchedule,
		int64& InOutPeakScratchBytes,
		FText& OutError)
	{
		OutSchedule = FWorldChunkRecordSchedule();
		TArray<int32> Counts;
		Counts.SetNumZeroed(ChunkCount);
		for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
		{
			int32 ChunkIndex = INDEX_NONE;
			if (!ResolveChunk(Records[RecordIndex], ChunkIndex) || !Counts.IsValidIndex(ChunkIndex)
				|| Counts[ChunkIndex] == MAX_int32)
			{
				SetError(OutError, FString::Printf(
					TEXT("ArcForest %s record %d cannot be assigned to exactly one site chunk."),
					RecordKind, RecordIndex));
				return false;
			}
			++Counts[ChunkIndex];
		}

		OutSchedule.Offsets.SetNumUninitialized(ChunkCount + 1);
		OutSchedule.Offsets[0] = 0;
		int64 RunningCount = 0;
		for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
		{
			RunningCount += Counts[ChunkIndex];
			if (RunningCount > MAX_int32)
			{
				SetError(OutError, FString::Printf(
					TEXT("ArcForest %s compatibility schedule exceeds the supported record count."), RecordKind));
				return false;
			}
			OutSchedule.Offsets[ChunkIndex + 1] = static_cast<int32>(RunningCount);
		}
		if (RunningCount != Records.Num())
		{
			SetError(OutError, FString::Printf(
				TEXT("ArcForest %s compatibility schedule lost records."), RecordKind));
			return false;
		}

		OutSchedule.RecordIndices.SetNumUninitialized(Records.Num());
		TArray<int32> Cursors;
		Cursors.SetNumUninitialized(ChunkCount);
		const uint64 ScratchBytes = static_cast<uint64>(Counts.GetAllocatedSize())
			+ static_cast<uint64>(Cursors.GetAllocatedSize());
		if (ScratchBytes > static_cast<uint64>(MAX_int64))
		{
			SetError(OutError, TEXT("ArcForest compatibility schedule scratch is not representable."));
			return false;
		}
		InOutPeakScratchBytes = FMath::Max(InOutPeakScratchBytes, static_cast<int64>(ScratchBytes));
		for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
		{
			Cursors[ChunkIndex] = OutSchedule.Offsets[ChunkIndex];
		}
		for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
		{
			int32 ChunkIndex = INDEX_NONE;
			if (!ResolveChunk(Records[RecordIndex], ChunkIndex) || !Cursors.IsValidIndex(ChunkIndex)
				|| Cursors[ChunkIndex] >= OutSchedule.Offsets[ChunkIndex + 1])
			{
				SetError(OutError, FString::Printf(
					TEXT("ArcForest %s compatibility schedule changed during stable fill."), RecordKind));
				return false;
			}
			OutSchedule.RecordIndices[Cursors[ChunkIndex]++] = RecordIndex;
		}
		for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
		{
			if (Cursors[ChunkIndex] != OutSchedule.Offsets[ChunkIndex + 1])
			{
				SetError(OutError, FString::Printf(
					TEXT("ArcForest %s compatibility schedule did not assign every record exactly once."), RecordKind));
				return false;
			}
		}
		return true;
	}

	bool TryGetChunkIndex(const FArcEcotopeChunkLayout& Layout, const FArcEcotopeChunkId& Id, int32& OutChunkIndex)
	{
		OutChunkIndex = INDEX_NONE;
		const FIntPoint ChunkGrid = Layout.GetChunkGridDimensions();
		const int64 Index = static_cast<int64>(Id.X) + static_cast<int64>(Id.Y) * ChunkGrid.X;
		if (!Id.IsValid() || Id.X >= ChunkGrid.X || Id.Y >= ChunkGrid.Y
			|| Index < 0 || Index >= Layout.GetChunkCount())
		{
			return false;
		}
		OutChunkIndex = static_cast<int32>(Index);
		return true;
	}
}

bool FArcForestRegionalRecruitmentConfig::IsValid() const
{
	return MaxProposalRounds >= 1
		&& MaxProposalRounds <= 1024;
}

int32 FArcForestWorldSnapshot::GetCurrentSchemaVersion()
{
	return ArcForestWorldSnapshotPrivate::CurrentSchemaVersion;
}

bool FArcForestWorldSnapshot::IsEmpty() const
{
	return !SimulationId.IsValid()
		&& CurrentYear == 0
		&& Seed == 0
		&& SiteSnapshotHash == 0
		&& SettingsHash == 0
		&& SpeciesEcologyHashes.IsEmpty()
		&& StateRevision == 0
		&& Individuals.IsEmpty()
		&& Cohorts.IsEmpty()
		&& Patches.IsEmpty()
		&& Deadwood.IsEmpty()
		&& StateChecksum == 0;
}

EArcForestSimulationStateStatus FArcForestWorldSnapshot::Validate(
	FText& OutError,
	const uint64 ExpectedSettingsHash,
	const TArray<FArcForestSpeciesEcologyHash>* ExpectedSpeciesEcologyHashes) const
{
	using namespace ArcForestWorldSnapshotPrivate;
	ARCFOREST_SCOPED_COUNTER(UE::ArcForest::GetSnapshotValidateCounter(), WorldSnapshot_Validate);

	if (SchemaVersion != CurrentSchemaVersion)
	{
		SetError(
			OutError,
			FString::Printf(
				TEXT("World snapshot schema version %d is unsupported; expected %d. Clear State and bootstrap again."),
				SchemaVersion,
				CurrentSchemaVersion));
		return EArcForestSimulationStateStatus::InvalidVersion;
	}

	if (IsEmpty())
	{
		OutError = FText::GetEmpty();
		return EArcForestSimulationStateStatus::Empty;
	}

	if (!SimulationId.IsValid() || CurrentYear < 0 || SiteSnapshotHash == 0 || SettingsHash == 0)
	{
		SetError(OutError, TEXT("World snapshot metadata is incomplete or out of range."));
		return EArcForestSimulationStateStatus::Invalid;
	}
	if (!RegionalRecruitmentConfig.IsValid())
	{
		SetError(OutError, TEXT("World snapshot regional recruitment lineage is invalid."));
		return EArcForestSimulationStateStatus::Invalid;
	}
	TSet<FGuid> EcologySpeciesIds;
	FGuid PreviousSpeciesId;
	bool bHasPreviousSpecies = false;
	for (int32 EntryIndex = 0; EntryIndex < SpeciesEcologyHashes.Num(); ++EntryIndex)
	{
		const FArcForestSpeciesEcologyHash& Entry = SpeciesEcologyHashes[EntryIndex];
		if (!Entry.SpeciesId.IsValid() || Entry.EcologyHash == 0 || EcologySpeciesIds.Contains(Entry.SpeciesId))
		{
			SetError(
				OutError,
				FString::Printf(
					TEXT("SpeciesEcologyHashes[%d] has an invalid SpeciesId, zero ecology hash, or duplicate SpeciesId."),
					EntryIndex));
			return EArcForestSimulationStateStatus::Invalid;
		}
		if (bHasPreviousSpecies && !GuidLess(PreviousSpeciesId, Entry.SpeciesId))
		{
			SetError(OutError, TEXT("Species ecology hashes are not in canonical SpeciesId order."));
			return EArcForestSimulationStateStatus::Invalid;
		}
		EcologySpeciesIds.Add(Entry.SpeciesId);
		PreviousSpeciesId = Entry.SpeciesId;
		bHasPreviousSpecies = true;
	}

	if (!HasStrictlyIncreasingIds(Individuals, [](const FArcForestIndividualRecord& Record) { return Record.Id; })
		|| !HasStrictlyIncreasingIds(Cohorts, [](const FArcForestCohortRecord& Record) { return Record.Id; })
		|| !HasStrictlyIncreasingIds(Patches, [](const FArcForestPatchRecord& Record) { return Record.Id; })
		|| !HasStrictlyIncreasingIds(Deadwood, [](const FArcForestDeadwoodRecord& Record) { return Record.Id; }))
	{
		SetError(OutError, TEXT("One or more record arrays have zero, duplicate, or non-canonical stable identifiers."));
		return EArcForestSimulationStateStatus::Invalid;
	}

	const auto ValidateSpecies = [&EcologySpeciesIds](const FGuid& SpeciesId)
	{
		return SpeciesId.IsValid() && EcologySpeciesIds.Contains(SpeciesId);
	};

	for (int32 RecordIndex = 0; RecordIndex < Individuals.Num(); ++RecordIndex)
	{
		const FArcForestIndividualRecord& Record = Individuals[RecordIndex];
		if (Record.Id == 0
			|| !ValidateSpecies(Record.SpeciesId)
			|| !IsFiniteVector(Record.LocalPosition)
			|| Record.AgeYears < 0
			|| !FMath::IsFinite(Record.HeightCm) || Record.HeightCm < 30.0f
			|| !FMath::IsFinite(Record.DbhCm) || Record.DbhCm < 0.0f
			|| !FMath::IsFinite(Record.CrownRadiusCm) || Record.CrownRadiusCm < 0.0f
			|| !FMath::IsFinite(Record.BiomassKg) || Record.BiomassKg < 0.0f
			|| (Record.LifeStatus == EArcForestLifeStatus::Alive && Record.BiomassKg <= 0.0f)
			|| !FMath::IsFinite(Record.Vigor) || Record.Vigor < 0.0f || Record.Vigor > 1.0f
			|| static_cast<uint8>(Record.LifeStatus) >= static_cast<uint8>(EArcForestLifeStatus::MAX)
			|| Record.NextShedYear <= 0)
		{
			SetError(OutError, FString::Printf(TEXT("Individuals[%d] contains invalid data."), RecordIndex));
			return EArcForestSimulationStateStatus::Invalid;
		}
	}

	int64 TotalCohortStems = 0;
	for (int32 RecordIndex = 0; RecordIndex < Cohorts.Num(); ++RecordIndex)
	{
		const FArcForestCohortRecord& Record = Cohorts[RecordIndex];
		if (Record.Id == 0
			|| !ValidateSpecies(Record.SpeciesId)
			|| Record.CellIndex < 0
			|| Record.BirthYear > CurrentYear
			|| Record.HeightBin < 0
			|| Record.StemCount <= 0
			|| !FMath::IsFinite(Record.MeanHeightCm) || Record.MeanHeightCm < 30.0f
			|| !FMath::IsFinite(Record.MeanAgeYears) || Record.MeanAgeYears < 0.0f
			|| !FMath::IsFinite(Record.MeanVigor) || Record.MeanVigor < 0.0f || Record.MeanVigor > 1.0f
			|| !FMath::IsFinite(Record.BiomassKg) || Record.BiomassKg < 0.0f)
		{
			SetError(OutError, FString::Printf(TEXT("Cohorts[%d] contains invalid data."), RecordIndex));
			return EArcForestSimulationStateStatus::Invalid;
		}
		if (TotalCohortStems > MAX_int64 - Record.StemCount)
		{
			SetError(OutError, TEXT("Cohort stem counts overflow int64 when accumulated."));
			return EArcForestSimulationStateStatus::Invalid;
		}
		TotalCohortStems += Record.StemCount;
	}

	for (int32 RecordIndex = 0; RecordIndex < Patches.Num(); ++RecordIndex)
	{
		const FArcForestPatchRecord& Record = Patches[RecordIndex];
		if (Record.Id == 0
			|| !ValidateSpecies(Record.SpeciesId)
			|| Record.CellIndex < 0
			|| !FMath::IsFinite(Record.StemOrCoverEquivalent) || Record.StemOrCoverEquivalent <= 0.0f
			|| !FMath::IsFinite(Record.MeanHeightCm) || Record.MeanHeightCm < 30.0f
			|| !FMath::IsFinite(Record.MeanAgeYears) || Record.MeanAgeYears < 0.0f
			|| !FMath::IsFinite(Record.MeanVigor) || Record.MeanVigor < 0.0f || Record.MeanVigor > 1.0f
			|| !FMath::IsFinite(Record.BiomassKg) || Record.BiomassKg < 0.0f)
		{
			SetError(OutError, FString::Printf(TEXT("Patches[%d] contains invalid data."), RecordIndex));
			return EArcForestSimulationStateStatus::Invalid;
		}
	}

	for (int32 RecordIndex = 0; RecordIndex < Deadwood.Num(); ++RecordIndex)
	{
		const FArcForestDeadwoodRecord& Record = Deadwood[RecordIndex];
		if (Record.Id == 0
			|| Record.SourceEntityId == 0
			|| !ValidateSpecies(Record.SpeciesId)
			|| static_cast<uint8>(Record.Type) >= static_cast<uint8>(EArcForestDeadwoodType::MAX)
			|| !IsFiniteTransform(Record.LocalTransform)
			|| !IsFiniteVector(Record.DimensionsCm)
			|| Record.DimensionsCm.X <= 0.0 || Record.DimensionsCm.Y <= 0.0 || Record.DimensionsCm.Z <= 0.0
			|| Record.DeathYear < 0 || Record.DeathYear > CurrentYear
			|| !FMath::IsFinite(Record.InitialBiomassKg) || Record.InitialBiomassKg < 0.0f
			|| Record.RetireYear < Record.DeathYear
			|| Record.FallYear <= Record.DeathYear)
		{
			SetError(OutError, FString::Printf(TEXT("Deadwood[%d] contains invalid data."), RecordIndex));
			return EArcForestSimulationStateStatus::Invalid;
		}
	}

	if (StateChecksum != CalculateCanonicalChecksum())
	{
		SetError(OutError, TEXT("World snapshot checksum does not match its serialized fields."));
		return EArcForestSimulationStateStatus::Invalid;
	}

	if (ExpectedSettingsHash != 0 && SettingsHash != ExpectedSettingsHash)
	{
		SetError(OutError, TEXT("World snapshot settings hash does not match the current volume settings."));
		return EArcForestSimulationStateStatus::Incompatible;
	}

	if (ExpectedSpeciesEcologyHashes != nullptr)
	{
		if (SpeciesEcologyHashes.Num() != ExpectedSpeciesEcologyHashes->Num())
		{
			SetError(OutError, TEXT("World snapshot has missing or extra enabled species ecology entries."));
			return EArcForestSimulationStateStatus::Incompatible;
		}

		for (int32 EntryIndex = 0; EntryIndex < SpeciesEcologyHashes.Num(); ++EntryIndex)
		{
			const FArcForestSpeciesEcologyHash& Actual = SpeciesEcologyHashes[EntryIndex];
			const FArcForestSpeciesEcologyHash& Expected = (*ExpectedSpeciesEcologyHashes)[EntryIndex];
			if (Actual.SpeciesId != Expected.SpeciesId || Actual.EcologyHash != Expected.EcologyHash)
			{
				SetError(
					OutError,
					FString::Printf(
						TEXT("World snapshot species ecology entry %d does not match the current enabled species."),
						EntryIndex));
				return EArcForestSimulationStateStatus::Incompatible;
			}
		}
	}

	OutError = FText::GetEmpty();
	return EArcForestSimulationStateStatus::Valid;
}

bool FArcForestWorldSnapshot::ValidateSiteCompatibility(
	const FArcEcotopeSiteMetadata& SiteMetadata,
	const FArcEcotopeChunkLayout& SiteLayout,
	const FArcEcotopeSiteChunkReader& SiteReader,
	const int64 ScheduleBudgetBytes,
	FArcForestWorldSiteValidationStats& OutStats,
	FText& OutError) const
{
	using namespace ArcForestWorldSnapshotPrivate;
	OutStats = FArcForestWorldSiteValidationStats();
	OutStats.ScheduleBudgetBytes = ScheduleBudgetBytes;
	OutError = FText::GetEmpty();
	const FIntPoint RecordDimensions = SiteMetadata.GetLevelDimensions(SiteMetadata.RecordGridLevel);
	const int64 RecordCellCount = static_cast<int64>(RecordDimensions.X) * RecordDimensions.Y;
	const FIntPoint ChunkGrid = SiteLayout.GetChunkGridDimensions();
	const int32 ChunkCount = SiteLayout.GetChunkCount();
	OutStats.TotalChunkCount = ChunkCount;
	OutStats.IndividualCount = Individuals.Num();
	OutStats.CohortCount = Cohorts.Num();
	OutStats.PatchCount = Patches.Num();
	OutStats.DeadwoodCount = Deadwood.Num();
	if (ScheduleBudgetBytes <= 0 || !SiteReader.IsOpen()
		|| SiteSnapshotHash == 0 || SiteMetadata.SiteSnapshotHash == 0
		|| SiteSnapshotHash != SiteMetadata.SiteSnapshotHash
		|| SiteMetadata.RecordGridLevel > FArcEcotopeSiteMetadata::MaxGridLevel
		|| SiteLayout.GetBaseDimensions() != SiteMetadata.GridDimensions
		|| SiteLayout.GetRecordDimensions() != RecordDimensions
		|| RecordDimensions.X <= 0 || RecordDimensions.Y <= 0
		|| RecordCellCount <= 0 || RecordCellCount > MAX_int32
		|| ChunkGrid.X <= 0 || ChunkGrid.Y <= 0 || ChunkCount <= 0 || ChunkCount >= MAX_int32
		|| static_cast<int64>(ChunkGrid.X) * ChunkGrid.Y != ChunkCount
		|| SiteLayout.GetCoreBaseCellSpan() <= 0 || SiteLayout.GetCoreRecordCellSpan() <= 0
		|| SiteLayout.GetCoreBaseCellSpan() % (1 << SiteMetadata.RecordGridLevel) != 0
		|| SiteLayout.GetCoreRecordCellSpan()
			!= SiteLayout.GetCoreBaseCellSpan() / (1 << SiteMetadata.RecordGridLevel))
	{
		SetError(OutError, TEXT("World snapshot and site reader metadata are incompatible."));
		return false;
	}

	for (int32 RecordIndex = 0; RecordIndex < Cohorts.Num(); ++RecordIndex)
	{
		if (Cohorts[RecordIndex].CellIndex >= RecordCellCount)
		{
			SetError(OutError, FString::Printf(
				TEXT("ArcForest cohort record %d cell index %d exceeds record cell count %lld."),
				RecordIndex, Cohorts[RecordIndex].CellIndex, RecordCellCount));
			return false;
		}
	}
	for (int32 RecordIndex = 0; RecordIndex < Patches.Num(); ++RecordIndex)
	{
		if (Patches[RecordIndex].CellIndex >= RecordCellCount)
		{
			SetError(OutError, FString::Printf(
				TEXT("ArcForest patch record %d cell index %d exceeds record cell count %lld."),
				RecordIndex, Patches[RecordIndex].CellIndex, RecordCellCount));
			return false;
		}
	}

	uint64 ScheduleLowerBound = 0;
	bool bPreflightValid = true;
	for (const int32 RecordCount : {Individuals.Num(), Cohorts.Num(), Patches.Num(), Deadwood.Num()})
	{
		bPreflightValid = bPreflightValid && TryAddInt32ArrayLowerBound(RecordCount, ScheduleLowerBound);
	}
	for (int32 ScheduleIndex = 0; ScheduleIndex < 4; ++ScheduleIndex)
	{
		bPreflightValid = bPreflightValid
			&& TryAddInt32ArrayLowerBound(ChunkCount + 1, ScheduleLowerBound);
	}
	uint64 ScratchLowerBound = 0;
	bPreflightValid = bPreflightValid && TryAddInt32ArrayLowerBound(ChunkCount, ScratchLowerBound)
		&& TryAddInt32ArrayLowerBound(ChunkCount, ScratchLowerBound);
	if (!bPreflightValid || ScheduleLowerBound > static_cast<uint64>(MAX_int64)
		|| ScratchLowerBound > static_cast<uint64>(MAX_int64)
		|| ScheduleLowerBound > MAX_uint64 - ScratchLowerBound
		|| ScheduleLowerBound + ScratchLowerBound > static_cast<uint64>(ScheduleBudgetBytes))
	{
		SetError(OutError, TEXT("ArcForest site compatibility schedule exceeds its bounded memory budget."));
		return false;
	}

	const auto ResolveBaseCellChunk = [&SiteLayout, &SiteMetadata](
		const int32 BaseCellIndex, int32& OutChunkIndex)
	{
		FIntPoint BaseCell;
		FArcEcotopeChunkId ChunkId;
		return SiteMetadata.TryGetBaseCellCoordinates(BaseCellIndex, BaseCell)
			&& SiteLayout.FindChunkForBaseCell(BaseCell, ChunkId)
			&& TryGetChunkIndex(SiteLayout, ChunkId, OutChunkIndex);
	};
	const auto ResolvePositionChunk = [&SiteMetadata, &ResolveBaseCellChunk](
		const FVector& LocalPosition, int32& OutChunkIndex)
	{
		int32 BaseCellIndex = INDEX_NONE;
		return SiteMetadata.TryGetBaseCellIndexForLocalPosition(
				FVector2D(LocalPosition.X, LocalPosition.Y), BaseCellIndex)
			&& ResolveBaseCellChunk(BaseCellIndex, OutChunkIndex);
	};
	const auto ResolveRecordCellChunk = [&SiteLayout, &RecordDimensions](
		const int32 RecordCellIndex, int32& OutChunkIndex)
	{
		if (RecordCellIndex < 0 || RecordCellIndex >= static_cast<int64>(RecordDimensions.X) * RecordDimensions.Y)
		{
			return false;
		}
		FArcEcotopeChunkId ChunkId;
		return SiteLayout.FindChunkForRecordCell(
				FIntPoint(RecordCellIndex % RecordDimensions.X, RecordCellIndex / RecordDimensions.X), ChunkId)
			&& TryGetChunkIndex(SiteLayout, ChunkId, OutChunkIndex);
	};

	FWorldChunkRecordSchedule IndividualSchedule;
	FWorldChunkRecordSchedule CohortSchedule;
	FWorldChunkRecordSchedule PatchSchedule;
	FWorldChunkRecordSchedule DeadwoodSchedule;
	int64 PeakScheduleScratchBytes = 0;
	if (!BuildWorldChunkSchedule(
			MakeArrayView(Individuals), ChunkCount,
			[&ResolvePositionChunk](const FArcForestIndividualRecord& Record, int32& OutChunkIndex)
			{
				return ResolvePositionChunk(Record.LocalPosition, OutChunkIndex);
			},
			TEXT("individual"), IndividualSchedule, PeakScheduleScratchBytes, OutError)
		|| !BuildWorldChunkSchedule(
			MakeArrayView(Cohorts), ChunkCount,
			[&ResolveRecordCellChunk](const FArcForestCohortRecord& Record, int32& OutChunkIndex)
			{
				return ResolveRecordCellChunk(Record.CellIndex, OutChunkIndex);
			},
			TEXT("cohort"), CohortSchedule, PeakScheduleScratchBytes, OutError)
		|| !BuildWorldChunkSchedule(
			MakeArrayView(Patches), ChunkCount,
			[&ResolveRecordCellChunk](const FArcForestPatchRecord& Record, int32& OutChunkIndex)
			{
				return ResolveRecordCellChunk(Record.CellIndex, OutChunkIndex);
			},
			TEXT("patch"), PatchSchedule, PeakScheduleScratchBytes, OutError)
		|| !BuildWorldChunkSchedule(
			MakeArrayView(Deadwood), ChunkCount,
			[&ResolvePositionChunk](const FArcForestDeadwoodRecord& Record, int32& OutChunkIndex)
			{
				return ResolvePositionChunk(Record.LocalTransform.GetTranslation(), OutChunkIndex);
			},
			TEXT("deadwood"), DeadwoodSchedule, PeakScheduleScratchBytes, OutError))
	{
		return false;
	}

	for (const FWorldChunkRecordSchedule* Schedule : {
		&IndividualSchedule, &CohortSchedule, &PatchSchedule, &DeadwoodSchedule})
	{
		const int64 Bytes = Schedule->GetAllocatedBytes();
		if (Bytes < 0 || OutStats.ScheduleBytes > MAX_int64 - Bytes)
		{
			SetError(OutError, TEXT("ArcForest site compatibility schedule memory accounting overflowed."));
			return false;
		}
		OutStats.ScheduleBytes += Bytes;
	}
	if (OutStats.ScheduleBytes > MAX_int64 - PeakScheduleScratchBytes)
	{
		SetError(OutError, TEXT("ArcForest site compatibility peak schedule memory overflowed."));
		return false;
	}
	OutStats.PeakScheduleBytes = OutStats.ScheduleBytes + PeakScheduleScratchBytes;
	if (OutStats.PeakScheduleBytes > ScheduleBudgetBytes)
	{
		SetError(OutError, TEXT("ArcForest site compatibility schedule exceeds its memory budget after allocation."));
		return false;
	}

	const uint32 FieldMask =
		(1u << static_cast<uint32>(EArcEcotopeSiteField::ValidSurface))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::RecordValid));
	for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
	{
		const TConstArrayView<int32> IndividualRecords = IndividualSchedule.GetRecords(ChunkIndex);
		const TConstArrayView<int32> CohortRecords = CohortSchedule.GetRecords(ChunkIndex);
		const TConstArrayView<int32> PatchRecords = PatchSchedule.GetRecords(ChunkIndex);
		const TConstArrayView<int32> DeadwoodRecords = DeadwoodSchedule.GetRecords(ChunkIndex);
		if (IndividualRecords.IsEmpty() && CohortRecords.IsEmpty()
			&& PatchRecords.IsEmpty() && DeadwoodRecords.IsEmpty())
		{
			continue;
		}
		++OutStats.ActiveChunkCount;
		FArcEcotopeChunkRect Chunk;
		FArcEcotopeSiteChunkLease Lease;
		FString ReaderError;
		if (!SiteLayout.GetChunk(ChunkIndex, Chunk)
			|| !SiteReader.AcquireFields(Chunk.Id, 0, FieldMask, Lease, ReaderError))
		{
			++OutStats.FailedAcquireCount;
			SetError(OutError, FString::Printf(
				TEXT("World snapshot site compatibility could not acquire chunk %d: %s"),
				ChunkIndex, *ReaderError));
			return false;
		}
		++OutStats.AcquireCount;
		const FArcEcotopeSiteByteFieldView* ValidSurface =
			Lease.GetView().FindByteField(EArcEcotopeSiteField::ValidSurface);
		const FArcEcotopeSiteByteFieldView* RecordValid =
			Lease.GetView().FindByteField(EArcEcotopeSiteField::RecordValid);
		if (ValidSurface == nullptr || RecordValid == nullptr)
		{
			SetError(OutError, TEXT("World snapshot site compatibility is missing validity fields."));
			return false;
		}

		const auto ValidatePosition = [&SiteMetadata, ValidSurface](const FVector& LocalPosition)
		{
			int32 BaseCellIndex = INDEX_NONE;
			FIntPoint BaseCell;
			if (!SiteMetadata.TryGetBaseCellIndexForLocalPosition(
					FVector2D(LocalPosition.X, LocalPosition.Y), BaseCellIndex)
				|| !SiteMetadata.TryGetBaseCellCoordinates(BaseCellIndex, BaseCell))
			{
				return false;
			}
			const uint8* Value = ValidSurface->TryGet(BaseCell.X, BaseCell.Y);
			return Value != nullptr && *Value != 0;
		};
		for (const int32 RecordIndex : IndividualRecords)
		{
			if (!Individuals.IsValidIndex(RecordIndex) || !ValidatePosition(Individuals[RecordIndex].LocalPosition))
			{
				SetError(OutError, TEXT("An individual is outside a valid baked site cell."));
				return false;
			}
		}
		for (const int32 RecordIndex : DeadwoodRecords)
		{
			if (!Deadwood.IsValidIndex(RecordIndex)
				|| !ValidatePosition(Deadwood[RecordIndex].LocalTransform.GetTranslation()))
			{
				SetError(OutError, TEXT("A deadwood record is outside a valid baked site cell."));
				return false;
			}
		}
		const auto ValidateRecordCell = [&RecordDimensions, RecordValid](const int32 CellIndex)
		{
			if (CellIndex < 0 || CellIndex >= static_cast<int64>(RecordDimensions.X) * RecordDimensions.Y)
			{
				return false;
			}
			const uint8* Value = RecordValid->TryGet(
				CellIndex % RecordDimensions.X, CellIndex / RecordDimensions.X);
			return Value != nullptr && *Value != 0;
		};
		for (const int32 RecordIndex : CohortRecords)
		{
			if (!Cohorts.IsValidIndex(RecordIndex) || !ValidateRecordCell(Cohorts[RecordIndex].CellIndex))
			{
				SetError(OutError, TEXT("A cohort references an invalid baked site cell."));
				return false;
			}
		}
		for (const int32 RecordIndex : PatchRecords)
		{
			if (!Patches.IsValidIndex(RecordIndex) || !ValidateRecordCell(Patches[RecordIndex].CellIndex))
			{
				SetError(OutError, TEXT("A patch references an invalid baked site cell."));
				return false;
			}
		}
	}
	if (OutStats.AcquireCount != OutStats.ActiveChunkCount || OutStats.FailedAcquireCount != 0)
	{
		SetError(OutError, TEXT("World snapshot site compatibility did not acquire each active chunk exactly once."));
		return false;
	}
	OutError = FText::GetEmpty();
	return true;
}

uint64 FArcForestWorldSnapshot::CalculateCanonicalChecksum() const
{
	using namespace ArcForestWorldSnapshotPrivate;
	ARCFOREST_SCOPED_COUNTER(UE::ArcForest::GetSnapshotChecksumCounter(), WorldSnapshot_CalculateCanonicalChecksum);

	FCanonicalHash64 Hash;
	Hash.AddInt32(SchemaVersion);
	Hash.AddGuid(SimulationId);
	Hash.AddInt32(CurrentYear);
	Hash.AddInt32(Seed);
	Hash.AddUInt64(SiteSnapshotHash);
	Hash.AddUInt64(SettingsHash);
	Hash.AddInt32(RegionalRecruitmentConfig.MaxProposalRounds);

	TArray<const FArcForestSpeciesEcologyHash*> OrderedEcologyHashes;
	OrderedEcologyHashes.Reserve(SpeciesEcologyHashes.Num());
	for (const FArcForestSpeciesEcologyHash& Entry : SpeciesEcologyHashes)
	{
		OrderedEcologyHashes.Add(&Entry);
	}
	OrderedEcologyHashes.Sort(
		[](const FArcForestSpeciesEcologyHash& Left, const FArcForestSpeciesEcologyHash& Right)
		{
			return GuidLess(Left.SpeciesId, Right.SpeciesId);
		});
	Hash.AddUInt64(static_cast<uint64>(OrderedEcologyHashes.Num()));
	for (const FArcForestSpeciesEcologyHash* Entry : OrderedEcologyHashes)
	{
		Hash.AddGuid(Entry->SpeciesId);
		Hash.AddUInt64(Entry->EcologyHash);
	}
	Hash.AddUInt64(StateRevision);

	const TArray<const FArcForestIndividualRecord*> OrderedIndividuals =
		MakeCanonicalRecordView(Individuals, [](const FArcForestIndividualRecord& Record) { return Record.Id; });
	Hash.AddUInt64(static_cast<uint64>(OrderedIndividuals.Num()));
	for (const FArcForestIndividualRecord* Record : OrderedIndividuals)
	{
		Hash.AddUInt64(Record->Id);
		Hash.AddGuid(Record->SpeciesId);
		Hash.AddVector(Record->LocalPosition);
		Hash.AddInt32(Record->AgeYears);
		Hash.AddFloat(Record->HeightCm);
		Hash.AddFloat(Record->DbhCm);
		Hash.AddFloat(Record->CrownRadiusCm);
		Hash.AddFloat(Record->BiomassKg);
		Hash.AddFloat(Record->Vigor);
		Hash.AddByte(static_cast<uint8>(Record->LifeStatus));
		Hash.AddInt32(Record->NextShedYear);
	}

	const TArray<const FArcForestCohortRecord*> OrderedCohorts =
		MakeCanonicalRecordView(Cohorts, [](const FArcForestCohortRecord& Record) { return Record.Id; });
	Hash.AddUInt64(static_cast<uint64>(OrderedCohorts.Num()));
	for (const FArcForestCohortRecord* Record : OrderedCohorts)
	{
		Hash.AddUInt64(Record->Id);
		Hash.AddGuid(Record->SpeciesId);
		Hash.AddInt32(Record->CellIndex);
		Hash.AddInt32(Record->BirthYear);
		Hash.AddInt32(Record->HeightBin);
		Hash.AddInt64(Record->StemCount);
		Hash.AddFloat(Record->MeanHeightCm);
		Hash.AddFloat(Record->MeanAgeYears);
		Hash.AddFloat(Record->MeanVigor);
		Hash.AddFloat(Record->BiomassKg);
	}

	const TArray<const FArcForestPatchRecord*> OrderedPatches =
		MakeCanonicalRecordView(Patches, [](const FArcForestPatchRecord& Record) { return Record.Id; });
	Hash.AddUInt64(static_cast<uint64>(OrderedPatches.Num()));
	for (const FArcForestPatchRecord* Record : OrderedPatches)
	{
		Hash.AddUInt64(Record->Id);
		Hash.AddGuid(Record->SpeciesId);
		Hash.AddInt32(Record->CellIndex);
		Hash.AddFloat(Record->StemOrCoverEquivalent);
		Hash.AddFloat(Record->MeanHeightCm);
		Hash.AddFloat(Record->MeanAgeYears);
		Hash.AddFloat(Record->MeanVigor);
		Hash.AddFloat(Record->BiomassKg);
	}

	const TArray<const FArcForestDeadwoodRecord*> OrderedDeadwood =
		MakeCanonicalRecordView(
			Deadwood,
			[](const FArcForestDeadwoodRecord& Record) { return Record.Id; });
	Hash.AddUInt64(static_cast<uint64>(OrderedDeadwood.Num()));
	for (const FArcForestDeadwoodRecord* Record : OrderedDeadwood)
	{
		Hash.AddUInt64(Record->Id);
		Hash.AddUInt64(Record->SourceEntityId);
		Hash.AddGuid(Record->SpeciesId);
		Hash.AddByte(static_cast<uint8>(Record->Type));
		Hash.AddTransform(Record->LocalTransform);
		Hash.AddVector(Record->DimensionsCm);
		Hash.AddInt32(Record->DeathYear);
		Hash.AddFloat(Record->InitialBiomassKg);
		Hash.AddInt32(Record->RetireYear);
		Hash.AddInt32(Record->FallYear);
	}
	return Hash.Finalize();
}

void FArcForestWorldSnapshot::CanonicalizeAndRefreshChecksum()
{
	using namespace ArcForestWorldSnapshotPrivate;
	ARCFOREST_SCOPED_COUNTER(UE::ArcForest::GetSnapshotCanonicalizeCounter(), WorldSnapshot_CanonicalizeAndRefreshChecksum);

	SpeciesEcologyHashes.Sort(
		[](const FArcForestSpeciesEcologyHash& Left, const FArcForestSpeciesEcologyHash& Right)
		{
			return GuidLess(Left.SpeciesId, Right.SpeciesId);
		});
	Individuals.Sort(
		[](const FArcForestIndividualRecord& Left, const FArcForestIndividualRecord& Right)
		{
			return Left.Id < Right.Id;
		});
	Cohorts.Sort(
		[](const FArcForestCohortRecord& Left, const FArcForestCohortRecord& Right)
		{
			return Left.Id < Right.Id;
		});
	Patches.Sort(
		[](const FArcForestPatchRecord& Left, const FArcForestPatchRecord& Right)
		{
			return Left.Id < Right.Id;
		});
	Deadwood.Sort(
		[](const FArcForestDeadwoodRecord& Left, const FArcForestDeadwoodRecord& Right)
		{
			return Left.Id < Right.Id;
		});

	for (FArcForestIndividualRecord& Record : Individuals)
	{
		NormalizeVector(Record.LocalPosition);
		NormalizeSignedZero(Record.HeightCm);
		NormalizeSignedZero(Record.DbhCm);
		NormalizeSignedZero(Record.CrownRadiusCm);
		NormalizeSignedZero(Record.BiomassKg);
		NormalizeSignedZero(Record.Vigor);
	}
	for (FArcForestCohortRecord& Record : Cohorts)
	{
		NormalizeSignedZero(Record.MeanHeightCm);
		NormalizeSignedZero(Record.MeanAgeYears);
		NormalizeSignedZero(Record.MeanVigor);
		NormalizeSignedZero(Record.BiomassKg);
	}
	for (FArcForestPatchRecord& Record : Patches)
	{
		NormalizeSignedZero(Record.StemOrCoverEquivalent);
		NormalizeSignedZero(Record.MeanHeightCm);
		NormalizeSignedZero(Record.MeanAgeYears);
		NormalizeSignedZero(Record.MeanVigor);
		NormalizeSignedZero(Record.BiomassKg);
	}
	for (FArcForestDeadwoodRecord& Record : Deadwood)
	{
		NormalizeTransform(Record.LocalTransform);
		NormalizeVector(Record.DimensionsCm);
		NormalizeSignedZero(Record.InitialBiomassKg);
	}
	StateChecksum = CalculateCanonicalChecksum();
}
