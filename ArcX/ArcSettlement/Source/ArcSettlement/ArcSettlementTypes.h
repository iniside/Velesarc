// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcSettlementTypes.generated.h"

/** Lightweight handle to a knowledge entry in the subsystem. */
USTRUCT(BlueprintType)
struct ARCSETTLEMENT_API FArcKnowledgeHandle
{
	GENERATED_BODY()

	FArcKnowledgeHandle() = default;
	explicit FArcKnowledgeHandle(uint32 InValue) : Value(InValue) {}

	bool IsValid() const { return Value != 0; }
	uint32 GetValue() const { return Value; }

	bool operator==(const FArcKnowledgeHandle& Other) const { return Value == Other.Value; }
	bool operator!=(const FArcKnowledgeHandle& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FArcKnowledgeHandle& Handle) { return ::GetTypeHash(Handle.Value); }

	static FArcKnowledgeHandle Make()
	{
		static uint32 Counter = 0;
		return FArcKnowledgeHandle(++Counter);
	}

private:
	UPROPERTY()
	uint32 Value = 0;
};

/** Lightweight handle to a settlement. */
USTRUCT(BlueprintType)
struct ARCSETTLEMENT_API FArcSettlementHandle
{
	GENERATED_BODY()

	FArcSettlementHandle() = default;
	explicit FArcSettlementHandle(uint32 InValue) : Value(InValue) {}

	bool IsValid() const { return Value != 0; }
	uint32 GetValue() const { return Value; }

	bool operator==(const FArcSettlementHandle& Other) const { return Value == Other.Value; }
	bool operator!=(const FArcSettlementHandle& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FArcSettlementHandle& Handle) { return ::GetTypeHash(Handle.Value); }

	static FArcSettlementHandle Make()
	{
		static uint32 Counter = 0;
		return FArcSettlementHandle(++Counter);
	}

private:
	UPROPERTY()
	uint32 Value = 0;
};

/** Lightweight handle to a region. */
USTRUCT(BlueprintType)
struct ARCSETTLEMENT_API FArcRegionHandle
{
	GENERATED_BODY()

	FArcRegionHandle() = default;
	explicit FArcRegionHandle(uint32 InValue) : Value(InValue) {}

	bool IsValid() const { return Value != 0; }
	uint32 GetValue() const { return Value; }

	bool operator==(const FArcRegionHandle& Other) const { return Value == Other.Value; }
	bool operator!=(const FArcRegionHandle& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FArcRegionHandle& Handle) { return ::GetTypeHash(Handle.Value); }

	static FArcRegionHandle Make()
	{
		static uint32 Counter = 0;
		return FArcRegionHandle(++Counter);
	}

private:
	UPROPERTY()
	uint32 Value = 0;
};

/** Lightweight faction handle (stub — no diplomacy logic yet). */
USTRUCT(BlueprintType)
struct ARCSETTLEMENT_API FArcFactionHandle
{
	GENERATED_BODY()

	FArcFactionHandle() = default;
	explicit FArcFactionHandle(uint32 InValue) : Value(InValue) {}

	bool IsValid() const { return Value != 0; }
	uint32 GetValue() const { return Value; }

	bool operator==(const FArcFactionHandle& Other) const { return Value == Other.Value; }
	bool operator!=(const FArcFactionHandle& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FArcFactionHandle& Handle) { return ::GetTypeHash(Handle.Value); }

private:
	UPROPERTY()
	uint32 Value = 0;
};

/** Selection mode for knowledge queries. */
UENUM(BlueprintType)
enum class EArcKnowledgeSelectionMode : uint8
{
	HighestScore,
	TopN,
	RandomWeighted
};
