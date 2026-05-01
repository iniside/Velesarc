// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcActiveEffectHandle.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcActiveEffectHandle
{
	GENERATED_BODY()

	FArcActiveEffectHandle() = default;

	static FArcActiveEffectHandle Generate();
	bool IsValid() const { return Id != 0; }
	explicit operator bool() const { return IsValid(); }
	void Reset() { Id = 0; }

	bool operator==(FArcActiveEffectHandle Other) const { return Id == Other.Id; }
	bool operator!=(FArcActiveEffectHandle Other) const { return Id != Other.Id; }
	friend uint32 GetTypeHash(FArcActiveEffectHandle H) { return ::GetTypeHash(H.Id); }

private:
	UPROPERTY()
	uint32 Id = 0;
};
