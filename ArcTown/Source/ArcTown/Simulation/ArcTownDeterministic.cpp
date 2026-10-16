// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownDeterministic.h"

namespace
{
namespace ArcTownDeterministicInternal
{
	constexpr uint64 FnvOffsetBasis = 14695981039346656037ull;
	constexpr uint64 FnvPrime = 1099511628211ull;
	constexpr uint64 SimulationIdentityKey = 0x51F5D1A9C4B37E62ull;

	uint64 Avalanche(uint64 Value)
	{
		Value ^= Value >> 30;
		Value *= 0xBF58476D1CE4E5B9ull;
		Value ^= Value >> 27;
		Value *= 0x94D049BB133111EBull;
		Value ^= Value >> 31;
		return Value;
	}
}
}

namespace ArcTownDeterministic
{
	FKeyedHash64::FKeyedHash64(const uint64 Key)
		: Hash(ArcTownDeterministicInternal::FnvOffsetBasis)
	{
		AddUInt64(Key);
	}

	void FKeyedHash64::AddUInt8(const uint8 Value)
	{
		Hash ^= Value;
		Hash *= ArcTownDeterministicInternal::FnvPrime;
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
		return ArcTownDeterministicInternal::Avalanche(Hash);
	}

	FGuid MakeSimulationId(const FGuid& TownId, const int32 Seed)
	{
		FKeyedHash64 High(ArcTownDeterministicInternal::SimulationIdentityKey);
		High.AddGuid(TownId);
		High.AddInt32(Seed);
		High.AddUInt8(0);

		FKeyedHash64 Low(ArcTownDeterministicInternal::SimulationIdentityKey);
		Low.AddGuid(TownId);
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

	uint64 MakeKey(
		const FGuid& SimulationId,
		const int32 Seed,
		const EDeterministicDomain Domain,
		const int32 EntityId,
		const int32 Ordinal)
	{
		FKeyedHash64 Hash(static_cast<uint64>(Domain));
		Hash.AddGuid(SimulationId);
		Hash.AddInt32(Seed);
		Hash.AddInt32(EntityId);
		Hash.AddInt32(Ordinal);
		return Hash.Finalize();
	}

	uint64 MakeAnnualKey(
		const FGuid& SimulationId,
		const int32 Seed,
		const EDeterministicDomain Domain,
		const int32 Year,
		const int32 EntityId,
		const int32 Ordinal,
		const int64 QuantizedBranchInput)
	{
		FKeyedHash64 Hash(static_cast<uint64>(Domain));
		Hash.AddGuid(SimulationId);
		Hash.AddInt32(Seed);
		Hash.AddInt32(Year);
		Hash.AddInt32(EntityId);
		Hash.AddInt32(Ordinal);
		Hash.AddInt64(QuantizedBranchInput);
		return Hash.Finalize();
	}

	double KeyToUnitOpen(const uint64 Key)
	{
		constexpr double InverseTwoToThe52 = 1.0 / 4503599627370496.0;
		return (static_cast<double>(Key >> 12) + 0.5) * InverseTwoToThe52;
	}

	bool TryRoundNonNegative(const double Value, const uint64 Key, int32& OutRounded)
	{
		constexpr double MaxRepresentableCount = 2147483647.0;
		if (!FMath::IsFinite(Value) || Value < 0.0 || Value > MaxRepresentableCount)
		{
			return false;
		}

		const double FlooredValue = FMath::FloorToDouble(Value);
		const double Fraction = Value - FlooredValue;
		int32 Rounded = static_cast<int32>(FlooredValue);
		if (Fraction > 0.0 && KeyToUnitOpen(Key) < Fraction)
		{
			++Rounded;
		}

		OutRounded = Rounded;
		return true;
	}
}
