// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "ArcMassTestCountedArray.generated.h"

USTRUCT()
struct FArcMassTestCountedItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 Id = 0;

	UPROPERTY()
	int32 Value = 0;
};

USTRUCT()
struct FArcMassTestCountedArray : public FIrisFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcMassTestCountedItem> Items;

	static int32 AddCount;
	static int32 ChangeCount;

	static void ResetCounters();

	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize) {}
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
};

UCLASS(NotBlueprintable)
class AArcMassTestCountedProxy : public AActor
{
	GENERATED_BODY()

public:
	AArcMassTestCountedProxy();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	FArcMassTestCountedArray TrackedItems;
};
