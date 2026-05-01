// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "Mass/EntityHandle.h"
#include "GameFramework/Actor.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Replication/ArcIrisReplicatedArray.h"
#include "ArcMassTestItemFragment.generated.h"

USTRUCT()
struct FArcMassTestReplicatedItem : public FArcIrisReplicatedArrayItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ItemId = 0;

	UPROPERTY()
	int32 Value = 0;

	void PreReplicatedRemove(struct FArcMassTestReplicatedItemArray& Array);
	void PostReplicatedAdd(struct FArcMassTestReplicatedItemArray& Array);
	void PostReplicatedChange(struct FArcMassTestReplicatedItemArray& Array);
};

USTRUCT()
struct FArcMassTestReplicatedItemArray : public FArcIrisReplicatedArray
{
	GENERATED_BODY()

	FArcMassTestReplicatedItemArray() { InitCallbacks<FArcMassTestReplicatedItemArray, FArcMassTestReplicatedItem>(); }

	UPROPERTY()
	TArray<FArcMassTestReplicatedItem> Items;

	int32 AddCallbackCount = 0;
	int32 ChangeCallbackCount = 0;
	int32 RemoveCallbackCount = 0;

	int32 AddItem(FArcMassTestReplicatedItem&& Item) { return FArcIrisReplicatedArray::AddItem(Items, MoveTemp(Item)); }
	void RemoveItemAt(int32 Index) { FArcIrisReplicatedArray::RemoveItemAt(Items, Index); }
	void MarkItemDirty(FArcMassTestReplicatedItem& Item) { FArcIrisReplicatedArray::MarkItemDirty(Items, Item); }
	void MarkArrayDirty() { FArcIrisReplicatedArray::MarkAllDirty(Items); }
	void ClearDirtyState() { FArcIrisReplicatedArray::ClearDirtyState(Items.Num()); }
};

USTRUCT()
struct FArcMassTestItemFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassTestReplicatedItemArray ReplicatedItems;
};

template<>
struct TMassFragmentTraits<FArcMassTestItemFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

// --- Variant 2: item with nested struct ---

USTRUCT()
struct FArcMassTestNestedData
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Position = FVector::ZeroVector;

	UPROPERTY()
	float Health = 0.f;

	UPROPERTY()
	FName Tag;
};

USTRUCT()
struct FArcMassTestNestedItem : public FArcIrisReplicatedArrayItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ItemId = 0;

	UPROPERTY()
	FArcMassTestNestedData NestedData;

	void PreReplicatedRemove(const struct FArcMassTestNestedItemArray& Array) {}
	void PostReplicatedAdd(const struct FArcMassTestNestedItemArray& Array) {}
	void PostReplicatedChange(const struct FArcMassTestNestedItemArray& Array) {}
};

USTRUCT()
struct FArcMassTestNestedItemArray : public FArcIrisReplicatedArray
{
	GENERATED_BODY()

	FArcMassTestNestedItemArray() { InitCallbacks<FArcMassTestNestedItemArray, FArcMassTestNestedItem>(); }

	UPROPERTY()
	TArray<FArcMassTestNestedItem> Items;

	int32 AddItem(FArcMassTestNestedItem&& Item) { return FArcIrisReplicatedArray::AddItem(Items, MoveTemp(Item)); }
	void RemoveItemAt(int32 Index) { FArcIrisReplicatedArray::RemoveItemAt(Items, Index); }
};

USTRUCT()
struct FArcMassTestNestedItemFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassTestNestedItemArray ReplicatedItems;
};

template<>
struct TMassFragmentTraits<FArcMassTestNestedItemFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

// --- Variant 3: item with FMassEntityHandle (custom Iris net serializer) ---

USTRUCT()
struct FArcMassTestEntityHandleItem : public FArcIrisReplicatedArrayItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ItemId = 0;

	UPROPERTY()
	FMassEntityHandle EntityHandle;

	UPROPERTY()
	int32 Priority = 0;

	void PreReplicatedRemove(const struct FArcMassTestEntityHandleItemArray& Array) {}
	void PostReplicatedAdd(const struct FArcMassTestEntityHandleItemArray& Array) {}
	void PostReplicatedChange(const struct FArcMassTestEntityHandleItemArray& Array) {}
};

USTRUCT()
struct FArcMassTestEntityHandleItemArray : public FArcIrisReplicatedArray
{
	GENERATED_BODY()

	FArcMassTestEntityHandleItemArray() { InitCallbacks<FArcMassTestEntityHandleItemArray, FArcMassTestEntityHandleItem>(); }

	UPROPERTY()
	TArray<FArcMassTestEntityHandleItem> Items;

	int32 AddItem(FArcMassTestEntityHandleItem&& Item) { return FArcIrisReplicatedArray::AddItem(Items, MoveTemp(Item)); }
	void RemoveItemAt(int32 Index) { FArcIrisReplicatedArray::RemoveItemAt(Items, Index); }
};

USTRUCT()
struct FArcMassTestEntityHandleItemFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassTestEntityHandleItemArray ReplicatedItems;
};

template<>
struct TMassFragmentTraits<FArcMassTestEntityHandleItemFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

// --- Actor with direct UPROPERTY replicated array (not nested in fragment) ---

UCLASS(NotBlueprintable)
class AArcMassTestDirectArrayActor : public AActor
{
	GENERATED_BODY()

public:
	AArcMassTestDirectArrayActor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	FArcMassTestReplicatedItemArray ReplicatedItems;
};
