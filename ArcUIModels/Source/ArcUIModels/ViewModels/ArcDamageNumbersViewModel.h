// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "ArcDamageNumbersViewModel.generated.h"

USTRUCT(BlueprintType)
struct ARCUIMODELS_API FArcDamageNumberEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 Id = 0;

	UPROPERTY(BlueprintReadOnly)
	FString Kind;

	UPROPERTY(BlueprintReadOnly)
	int32 Value = 0;

	UPROPERTY(BlueprintReadOnly)
	float WorldX = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float WorldY = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float WorldZ = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float OffsetX = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float OffsetY = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float Drift = 0.f;

	bool operator==(const FArcDamageNumberEntry& Other) const
	{
		return Id == Other.Id
			&& Kind == Other.Kind
			&& Value == Other.Value
			&& WorldX == Other.WorldX
			&& WorldY == Other.WorldY
			&& WorldZ == Other.WorldZ
			&& OffsetX == Other.OffsetX
			&& OffsetY == Other.OffsetY
			&& Drift == Other.Drift;
	}
};

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcDamageNumbersViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Getter)
	TArray<FArcDamageNumberEntry> Entries;

	void SetEntries(const TArray<FArcDamageNumberEntry>& InEntries);
	const TArray<FArcDamageNumberEntry>& GetEntries() const { return Entries; }
};
