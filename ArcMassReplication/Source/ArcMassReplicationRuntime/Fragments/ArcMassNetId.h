// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassNetId.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSREPLICATIONRUNTIME_API FArcMassNetId
{
	GENERATED_BODY()

	FArcMassNetId() = default;
	explicit FArcMassNetId(uint32 InValue) : Value(InValue) {}

	bool IsValid() const { return Value != 0; }
	uint32 GetValue() const { return Value; }

	bool operator==(const FArcMassNetId& Other) const { return Value == Other.Value; }
	bool operator!=(const FArcMassNetId& Other) const { return Value != Other.Value; }

	friend uint32 GetTypeHash(const FArcMassNetId& Id) { return ::GetTypeHash(Id.Value); }

	FString ToString() const { return FString::Printf(TEXT("NetId[%u]"), Value); }

	friend FArchive& operator<<(FArchive& Ar, FArcMassNetId& Id)
	{
		Ar << Id.Value;
		return Ar;
	}

private:
	UPROPERTY(EditAnywhere)
	uint32 Value = 0;
};
