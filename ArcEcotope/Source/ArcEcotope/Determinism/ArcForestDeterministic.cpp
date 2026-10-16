// Copyright Lukasz Baran. All Rights Reserved.

#include "Determinism/ArcForestDeterministic.h"

#include "Simulation/ArcEcotopeBudgetSettings.h"
#include "Simulation/ArcForestBootstrap.h"
#include "Simulation/ArcForestDisturbanceSettings.h"
#include "Simulation/ArcEcotopeDeadwoodSettings.h"

namespace UE::ArcForest
{
	namespace Private
	{
		constexpr uint64 FnvOffsetBasis = 14695981039346656037ull;
		constexpr uint64 FnvPrime = 1099511628211ull;

		uint64 Avalanche(uint64 Value)
		{
			Value ^= Value >> 30;
			Value *= 0xBF58476D1CE4E5B9ull;
			Value ^= Value >> 27;
			Value *= 0x94D049BB133111EBull;
			Value ^= Value >> 31;
			return Value;
		}

		class FCanonicalHash64
		{
		public:
			void AddUInt8(const uint8 Value)
			{
				Hash ^= Value;
				Hash *= FnvPrime;
			}

			void AddUInt32(const uint32 Value)
			{
				AddUInt8(static_cast<uint8>(Value));
				AddUInt8(static_cast<uint8>(Value >> 8));
				AddUInt8(static_cast<uint8>(Value >> 16));
				AddUInt8(static_cast<uint8>(Value >> 24));
			}

			void AddInt32(const int32 Value)
			{
				AddUInt32(static_cast<uint32>(Value));
			}

			void AddUInt64(const uint64 Value)
			{
				AddUInt32(static_cast<uint32>(Value));
				AddUInt32(static_cast<uint32>(Value >> 32));
			}

			void AddFloat(const float Value)
			{
				AddUInt32(Value == 0.0f ? 0u : FPlatformMath::AsUInt(Value));
			}

			void AddDouble(const double Value)
			{
				AddUInt64(Value == 0.0 ? 0ull : FPlatformMath::AsUInt(Value));
			}

			void AddGuid(const FGuid& Value)
			{
				AddUInt32(Value.A);
				AddUInt32(Value.B);
				AddUInt32(Value.C);
				AddUInt32(Value.D);
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

		uint32 CanonicalFloatBits(const float Value)
		{
			return Value == 0.0f ? 0u : FPlatformMath::AsUInt(Value);
		}

		void AddResponse(FCanonicalHash64& Hash, const FArcForestFrozenResponseRange& Response)
		{
			Hash.AddFloat(Response.Minimum);
			Hash.AddFloat(Response.Optimum);
			Hash.AddFloat(Response.Maximum);
		}
	}

	FKeyedHash64::FKeyedHash64(const uint64 Key)
		: Hash(Private::FnvOffsetBasis)
	{
		AddUInt64(Key);
	}

	void FKeyedHash64::AddUInt8(const uint8 Value)
	{
		Hash ^= Value;
		Hash *= Private::FnvPrime;
	}

	void FKeyedHash64::AddUInt32(const uint32 Value)
	{
		for (int32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
		{
			AddUInt8(static_cast<uint8>(Value >> (ByteIndex * 8)));
		}
	}

	void FKeyedHash64::AddInt32(const int32 Value)
	{
		AddUInt32(static_cast<uint32>(Value));
	}

	void FKeyedHash64::AddUInt64(const uint64 Value)
	{
		for (int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
		{
			AddUInt8(static_cast<uint8>(Value >> (ByteIndex * 8)));
		}
	}

	void FKeyedHash64::AddInt64(const int64 Value)
	{
		AddUInt64(static_cast<uint64>(Value));
	}

	void FKeyedHash64::AddGuid(const FGuid& Value)
	{
		AddUInt32(Value.A);
		AddUInt32(Value.B);
		AddUInt32(Value.C);
		AddUInt32(Value.D);
	}

	uint64 FKeyedHash64::Finalize() const
	{
		return Private::Avalanche(Hash);
	}

	uint64 MakeKey(
		const FGuid& SimulationId,
		const int32 Seed,
		const EDeterministicDomain Domain,
		const FGuid& SpeciesId,
		const int32 CellIndex,
		const int64 FieldOrdinal)
	{
		FKeyedHash64 Hash(static_cast<uint64>(Domain));
		Hash.AddGuid(SimulationId);
		Hash.AddInt32(Seed);
		Hash.AddGuid(SpeciesId);
		Hash.AddInt32(CellIndex);
		Hash.AddInt64(FieldOrdinal);
		return Hash.Finalize();
	}

	uint64 MakeAnnualKey(
		const FGuid& SimulationId,
		const int32 Seed,
		const int32 Year,
		const EDeterministicDomain Domain,
		const FGuid& SpeciesId,
		const int32 CellIndex,
		const uint64 EntityId,
		const int32 DecisionOrdinal,
		const int64 QuantizedBranchInput)
	{
		FKeyedHash64 Hash(static_cast<uint64>(Domain));
		Hash.AddGuid(SimulationId);
		Hash.AddInt32(Seed);
		Hash.AddInt32(Year);
		Hash.AddGuid(SpeciesId);
		Hash.AddInt32(CellIndex);
		Hash.AddUInt64(EntityId);
		Hash.AddInt32(DecisionOrdinal);
		Hash.AddInt64(QuantizedBranchInput);
		return Hash.Finalize();
	}

	double KeyToUnitOpen(const uint64 Key)
	{
		constexpr double InverseTwoToThe53 = 1.0 / 9007199254740992.0;
		return (static_cast<double>(Key >> 11) + 0.5) * InverseTwoToThe53;
	}

	bool TryRoundNonNegative(const double Value, const uint64 Key, int64& OutRounded)
	{
		// Above 2^53-1 a double cannot represent every integer, so accepting the
		// value would make the exact admission count dependent on lost low bits.
		constexpr double MaxExactlyRepresentableInteger = 9007199254740991.0;
		if (!FMath::IsFinite(Value) || Value < 0.0 || Value > MaxExactlyRepresentableInteger)
		{
			return false;
		}

		const double FlooredValue = FMath::FloorToDouble(Value);
		const double Fraction = Value - FlooredValue;
		int64 Rounded = static_cast<int64>(FlooredValue);
		if (Fraction > 0.0 && KeyToUnitOpen(Key) < Fraction)
		{
			++Rounded;
		}

		OutRounded = Rounded;
		return true;
	}

	FGuid MakeSimulationId(const FGuid& VolumeId, const int32 Seed)
	{
		FKeyedHash64 High(static_cast<uint64>(EDeterministicDomain::SimulationIdentity));
		High.AddGuid(VolumeId);
		High.AddInt32(Seed);
		High.AddUInt8(0);

		FKeyedHash64 Low(static_cast<uint64>(EDeterministicDomain::SimulationIdentity));
		Low.AddGuid(VolumeId);
		Low.AddInt32(Seed);
		Low.AddUInt8(1);

		const uint64 HighValue = High.Finalize();
		const uint64 LowValue = Low.Finalize();
		FGuid Result(
			static_cast<uint32>(HighValue),
			static_cast<uint32>(HighValue >> 32),
			static_cast<uint32>(LowValue),
			static_cast<uint32>(LowValue >> 32));
		if (!Result.IsValid())
		{
			Result.D = 1;
		}
		return Result;
	}

	uint64 ComputeSettingsHash(
		const int32 Seed,
		const float CellSizeCm,
		const uint8 RecordGridLevel,
		const FArcEcotopeBudgetSettings& BudgetSettings,
		const FArcEcotopeDeadwoodSettings& DeadwoodSettings,
		const double MaximumCrownOccupancy,
		const double UnderstoryShade,
		const double GroundCoverDensity,
		const double GroundCoverPatchiness,
		const TConstArrayView<FSettingsHashSpecies> Species,
		const FArcForestDisturbanceSettings& DisturbanceSettings)
	{
		TArray<FSettingsHashSpecies> CanonicalSpecies;
		CanonicalSpecies.Append(Species.GetData(), Species.Num());
		CanonicalSpecies.Sort(
			[](const FSettingsHashSpecies& Left, const FSettingsHashSpecies& Right)
			{
				if (Left.SpeciesId != Right.SpeciesId)
				{
					return Private::GuidLess(Left.SpeciesId, Right.SpeciesId);
				}
				return Private::CanonicalFloatBits(Left.RegionalArrivalPerHectarePerYear)
					< Private::CanonicalFloatBits(Right.RegionalArrivalPerHectarePerYear);
			});

		Private::FCanonicalHash64 Hash;
		Hash.AddUInt32(SimulationRuleVersion);
		Hash.AddInt32(Seed);
		Hash.AddFloat(CellSizeCm);
		Hash.AddUInt8(RecordGridLevel);
		Hash.AddInt32(BudgetSettings.MaxLiveStems);
		Hash.AddInt32(BudgetSettings.MaxLiveRecords);
		Hash.AddInt32(BudgetSettings.RecruitmentOversamplingFactor);
		Hash.AddInt32(BudgetSettings.MaxCoverRecords);
		Hash.AddInt32(BudgetSettings.MaxDeadwoodRecords);
		Hash.AddInt32(BudgetSettings.MaxBranchRecords);
		Hash.AddFloat(DeadwoodSettings.DeadwoodDecayPerYear);
		Hash.AddFloat(DeadwoodSettings.MinimumTrackedDeadwoodKg);
		Hash.AddFloat(DeadwoodSettings.UprootFractionOfWindKills);
		Hash.AddFloat(DeadwoodSettings.CoarseWoodyDebrisMinimumDbhCm);
		Hash.AddFloat(DeadwoodSettings.SnagFallProbabilityPerYear);
		Hash.AddFloat(DeadwoodSettings.BranchShedProbabilityPerYear);
		Hash.AddDouble(MaximumCrownOccupancy);
		Hash.AddDouble(UnderstoryShade);
		Hash.AddDouble(GroundCoverDensity);
		Hash.AddDouble(GroundCoverPatchiness);
		Hash.AddUInt64(static_cast<uint64>(CanonicalSpecies.Num()));
		for (const FSettingsHashSpecies& Entry : CanonicalSpecies)
		{
			Hash.AddGuid(Entry.SpeciesId);
			Hash.AddFloat(Entry.RegionalArrivalPerHectarePerYear);
		}
		Hash.AddFloat(DisturbanceSettings.FireReturnIntervalYears);
		Hash.AddFloat(DisturbanceSettings.WindReturnIntervalYears);
		Hash.AddFloat(DisturbanceSettings.MinEventRadiusCm);
		Hash.AddFloat(DisturbanceSettings.MaxEventRadiusCm);
		Hash.AddFloat(DisturbanceSettings.WindHalfSeverityHeightCm);
		Hash.AddFloat(DisturbanceSettings.FireHalfSeverityHeightCm);
		Hash.AddFloat(DisturbanceSettings.FireCombustedFraction);
		Hash.AddFloat(DisturbanceSettings.DeadwoodConsumedFraction);
		return Hash.Finalize();
	}

	uint64 ComputeFrozenSpeciesEcologyHash(const FArcForestFrozenSpeciesSetup& Species)
	{
		Private::FCanonicalHash64 Hash;
		Hash.AddGuid(Species.SpeciesId);
		Hash.AddUInt8(Species.bIndividualizes ? 1 : 0);
		// Field-for-field twin of UArcEcotopeSpeciesModel::ComputeEcologyHash; only a runtime cross-check keeps the two in step.
		Hash.AddInt32(Species.TargetStemsPerHectare);
		Hash.AddFloat(Species.TreeSizeM);
		Hash.AddFloat(Species.Slenderness);
		Hash.AddFloat(Species.GrowthSpeed);
		Hash.AddFloat(Species.Toughness);
		Hash.AddFloat(Species.ShadeTolerance);
		Hash.AddFloat(Species.PreferredTemperature);
		Hash.AddFloat(Species.TemperatureTolerance);
		Hash.AddFloat(Species.PreferredHumidity);
		Hash.AddFloat(Species.MeanDispersalDistanceCm);
		Hash.AddFloat(Species.ExclusionSensitivity);
		Hash.AddFloat(Species.RecruitsPerMatureStemPerYear);
		Hash.AddFloat(Species.MobilityMortalitySensitivity);
		Hash.AddFloat(Species.LateralSpreadRatePerYear);
		Hash.AddFloat(Species.IndividualizationHeightCm);
		Hash.AddFloat(Species.MaxHeightCm);
		Hash.AddFloat(Species.MaxDbhCm);
		Hash.AddInt32(Species.MaxAgeYears);
		Hash.AddInt32(Species.MaturityAgeYears);
		Hash.AddFloat(Species.MaxAnnualHeightGrowthCm);
		Hash.AddFloat(Species.MaxAnnualDbhGrowthCm);
		Hash.AddFloat(Species.HeightDbhAllometryCoefficient);
		Hash.AddFloat(Species.HeightDbhAllometryExponent);
		Hash.AddFloat(Species.CrownRadiusHeightRatio);
		Hash.AddFloat(Species.BiomassAllometryCoefficient);
		Hash.AddFloat(Species.BiomassAllometryExponent);
		Hash.AddFloat(Species.CrownOccupancyWeight);
		Hash.AddFloat(Species.BaseAnnualMortalityProbability);
		Hash.AddDouble(Species.OldAgeMortalityProbabilityAtMaxAge);
		Hash.AddDouble(Species.StressMortalityMaximum);
		Hash.AddDouble(Species.StressMortalityVigorThreshold);
		Hash.AddDouble(Species.StressMortalitySteepness);
		Hash.AddDouble(Species.StressMortalityCanopyFloor);
		Hash.AddDouble(Species.VigorSmoothingRetention);
		Hash.AddFloat(Species.AnnualPropaguleProduction);
		Private::AddResponse(Hash, Species.LightResponse);
		Private::AddResponse(Hash, Species.MoistureResponse);
		Private::AddResponse(Hash, Species.TemperatureResponse);
		Hash.AddFloat(Species.MaxSlopeDeg);
		Hash.AddFloat(Species.SunLoadMin);
		Hash.AddFloat(Species.WindExposureMax);
		Hash.AddFloat(Species.FertilityMin);
		return Hash.Finalize();
	}
}
