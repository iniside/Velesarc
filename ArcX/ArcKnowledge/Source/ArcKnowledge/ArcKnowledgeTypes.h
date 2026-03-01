// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcKnowledgeTypes.generated.h"

/** Lightweight handle to a knowledge entry in the subsystem. */
USTRUCT(BlueprintType)
struct ARCKNOWLEDGE_API FArcKnowledgeHandle
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

/** Selection mode for knowledge queries. */
UENUM(BlueprintType)
enum class EArcKnowledgeSelectionMode : uint8
{
	HighestScore,
	TopN,
	RandomWeighted
};
