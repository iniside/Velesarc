// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityHandle.h"
#include "GameFramework/Actor.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Replication/ArcIrisReplicatedArray.h"
#include "Replication/ArcIrisReplicatedMap.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMassTestItemFragment.generated.h"

USTRUCT(meta = (Hidden))
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

USTRUCT(meta = (Hidden))
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
	void ClearDirtyState() { FArcIrisReplicatedArray::ClearDirtyState(); }
};

USTRUCT(meta = (Hidden))
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

USTRUCT(meta = (Hidden))
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

USTRUCT(meta = (Hidden))
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

USTRUCT(meta = (Hidden))
struct FArcMassTestNestedItemArray : public FArcIrisReplicatedArray
{
	GENERATED_BODY()

	FArcMassTestNestedItemArray() { InitCallbacks<FArcMassTestNestedItemArray, FArcMassTestNestedItem>(); }

	UPROPERTY()
	TArray<FArcMassTestNestedItem> Items;

	int32 AddItem(FArcMassTestNestedItem&& Item) { return FArcIrisReplicatedArray::AddItem(Items, MoveTemp(Item)); }
	void RemoveItemAt(int32 Index) { FArcIrisReplicatedArray::RemoveItemAt(Items, Index); }
};

USTRUCT(meta = (Hidden))
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

USTRUCT(meta = (Hidden))
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

USTRUCT(meta = (Hidden))
struct FArcMassTestEntityHandleItemArray : public FArcIrisReplicatedArray
{
	GENERATED_BODY()

	FArcMassTestEntityHandleItemArray() { InitCallbacks<FArcMassTestEntityHandleItemArray, FArcMassTestEntityHandleItem>(); }

	UPROPERTY()
	TArray<FArcMassTestEntityHandleItem> Items;

	int32 AddItem(FArcMassTestEntityHandleItem&& Item) { return FArcIrisReplicatedArray::AddItem(Items, MoveTemp(Item)); }
	void RemoveItemAt(int32 Index) { FArcIrisReplicatedArray::RemoveItemAt(Items, Index); }
};

USTRUCT(meta = (Hidden))
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

// --- Variant 4: item with FInstancedStruct field (probes Iris polymorphic serialization) ---

USTRUCT(meta = (Hidden))
struct FArcMassTestInstancedStructItem : public FArcIrisReplicatedArrayItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ItemId = 0;

	UPROPERTY()
	FInstancedStruct Payload;

	void PreReplicatedRemove(const struct FArcMassTestInstancedStructItemArray& Array) {}
	void PostReplicatedAdd(const struct FArcMassTestInstancedStructItemArray& Array) {}
	void PostReplicatedChange(const struct FArcMassTestInstancedStructItemArray& Array) {}
};

USTRUCT(meta = (Hidden))
struct FArcMassTestInstancedStructItemArray : public FArcIrisReplicatedArray
{
	GENERATED_BODY()

	FArcMassTestInstancedStructItemArray() { InitCallbacks<FArcMassTestInstancedStructItemArray, FArcMassTestInstancedStructItem>(); }

	UPROPERTY()
	TArray<FArcMassTestInstancedStructItem> Items;

	int32 AddItem(FArcMassTestInstancedStructItem&& Item) { return FArcIrisReplicatedArray::AddItem(Items, MoveTemp(Item)); }
	void RemoveItemAt(int32 Index) { FArcIrisReplicatedArray::RemoveItemAt(Items, Index); }
};

USTRUCT(meta = (Hidden))
struct FArcMassTestInstancedStructItemFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassTestInstancedStructItemArray ReplicatedItems;
};

template<>
struct TMassFragmentTraits<FArcMassTestInstancedStructItemFragment> final
{
	enum { AuthorAcceptsItsNotTriviallyCopyable = true };
};

// ---------------------------------------------------------------------------
// Map types — three concrete FArcIrisReplicatedMap subclasses for direct-actor tests.
// Each has a companion entry struct with exactly two UPROPERTY members: Key and Value.
// These are plain USTRUCTs (not FMassFragments) — the tests use direct actor UPROPERTYs,
// not Mass fragments.
// ---------------------------------------------------------------------------

// --- Map 1: TMap<int32, FArcMassTestReplicatedItem> ---

USTRUCT(meta = (Hidden))
struct FArcMassTestIntStructMapEntry
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Key = 0;

	UPROPERTY()
	FArcMassTestReplicatedItem Value;
};

USTRUCT(meta = (Hidden))
struct FArcMassTestReplicatedIntStructMap : public FArcIrisReplicatedMap
{
	GENERATED_BODY()

	// NotReplicated suppresses the UHT "Maps are not supported for Replication" error that
	// fires when UHT traces replicated paths through this struct (it is a Replicated UPROPERTY
	// on the actor). The Iris serializer locates this field via TFieldIterator<FMapProperty>
	// which finds NotReplicated properties; replication itself is driven by the custom Iris
	// FArcIrisReplicatedMapNetSerializer, not by UHT-generated code.
	UPROPERTY(NotReplicated)
	TMap<int32, FArcMassTestReplicatedItem> Entries;

	virtual const UScriptStruct* GetEntryStruct() const override
	{
		return FArcMassTestIntStructMapEntry::StaticStruct();
	}
};

// --- Map 2: TMap<FGameplayTag, float> ---

USTRUCT(meta = (Hidden))
struct FArcMassTestTagFloatMapEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Key;

	UPROPERTY()
	float Value = 0.f;
};

USTRUCT(meta = (Hidden))
struct FArcMassTestReplicatedTagFloatMap : public FArcIrisReplicatedMap
{
	GENERATED_BODY()

	// NotReplicated — same rationale as FArcMassTestReplicatedIntStructMap::Entries above.
	UPROPERTY(NotReplicated)
	TMap<FGameplayTag, float> Entries;

	virtual const UScriptStruct* GetEntryStruct() const override
	{
		return FArcMassTestTagFloatMapEntry::StaticStruct();
	}
};

// --- Map 3: TMap<FGameplayTag, FInstancedStruct>
// Note: FArcItemId was considered as key but lives in ArcMassItemsRuntime — a heavy dep
// not already in ArcMassReplicationTest.Build.cs. FGameplayTag is used instead to avoid
// adding a new module dependency. This still exercises FInstancedStruct (dynamic state +
// CollectNetReferences) as the value.

USTRUCT(meta = (Hidden))
struct FArcMassTestTagInstancedStructMapEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Key;

	UPROPERTY()
	FInstancedStruct Value;
};

USTRUCT(meta = (Hidden))
struct FArcMassTestReplicatedTagInstancedStructMap : public FArcIrisReplicatedMap
{
	GENERATED_BODY()

	// NotReplicated — same rationale as FArcMassTestReplicatedIntStructMap::Entries above.
	UPROPERTY(NotReplicated)
	TMap<FGameplayTag, FInstancedStruct> Entries;

	virtual const UScriptStruct* GetEntryStruct() const override
	{
		return FArcMassTestTagInstancedStructMapEntry::StaticStruct();
	}
};

// --- Direct-property test actor holding all three map types ---

UCLASS(NotBlueprintable, Hidden, NotPlaceable)
class AArcMassTestDirectMapActor : public AActor
{
	GENERATED_BODY()

public:
	AArcMassTestDirectMapActor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	FArcMassTestReplicatedIntStructMap IntStructMap;

	UPROPERTY(Replicated)
	FArcMassTestReplicatedTagFloatMap TagFloatMap;

	UPROPERTY(Replicated)
	FArcMassTestReplicatedTagInstancedStructMap TagInstancedStructMap;
};

// --- Actor with direct UPROPERTY replicated array (not nested in fragment) ---

UCLASS(NotBlueprintable, Hidden, NotPlaceable)
class AArcMassTestDirectArrayActor : public AActor
{
	GENERATED_BODY()

public:
	AArcMassTestDirectArrayActor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	FArcMassTestReplicatedItemArray ReplicatedItems;
};

// --- Wrapper around FArcMassTestReplicatedItemArray (probes nested-struct replication) ---

USTRUCT(meta = (Hidden))
struct FArcMassTestReplicatedItemArrayWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassTestReplicatedItemArray Inner;
};

// Actor with the wrapper as a direct replicated UPROPERTY.
UCLASS(NotBlueprintable, Hidden, NotPlaceable)
class AArcMassTestWrappedArrayActor : public AActor
{
	GENERATED_BODY()

public:
	AArcMassTestWrappedArrayActor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	FArcMassTestReplicatedItemArrayWrapper Wrapper;
};

// Actor that holds the wrapper inside a replicated TArray<FInstancedStruct>.
UCLASS(NotBlueprintable, Hidden, NotPlaceable)
class AArcMassTestInstancedStructWrappedArrayActor : public AActor
{
	GENERATED_BODY()

public:
	AArcMassTestInstancedStructWrappedArrayActor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	TArray<FInstancedStruct> Slots;
};

// --- Real engine FFastArraySerializer wrapped in a struct (probes whether nested FastArrays fire callbacks) ---

USTRUCT(meta = (Hidden))
struct FArcMassTestEngineFastItem : public FFastArraySerializerItem
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ItemId = 0;

	UPROPERTY()
	int32 Value = 0;

	void PreReplicatedRemove(const struct FArcMassTestEngineFastArray& Array);
	void PostReplicatedAdd(const struct FArcMassTestEngineFastArray& Array);
	void PostReplicatedChange(const struct FArcMassTestEngineFastArray& Array);
};

USTRUCT(meta = (Hidden))
struct FArcMassTestEngineFastArray : public FFastArraySerializer
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcMassTestEngineFastItem> Items;

	int32 AddCallbackCount = 0;
	int32 ChangeCallbackCount = 0;
	int32 RemoveCallbackCount = 0;

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FArcMassTestEngineFastItem, FArcMassTestEngineFastArray>(Items, DeltaParms, *this);
	}
};

template<>
struct TStructOpsTypeTraits<FArcMassTestEngineFastArray> : public TStructOpsTypeTraitsBase2<FArcMassTestEngineFastArray>
{
	enum { WithNetDeltaSerializer = true };
};

USTRUCT(meta = (Hidden))
struct FArcMassTestEngineFastArrayWrapper
{
	GENERATED_BODY()

	UPROPERTY()
	FArcMassTestEngineFastArray Inner;
};

// Sanity actor — engine FastArray as the top-level UPROPERTY (no wrapper).
UCLASS(NotBlueprintable, Hidden, NotPlaceable)
class AArcMassTestEngineFastArrayDirectActor : public AActor
{
	GENERATED_BODY()

public:
	AArcMassTestEngineFastArrayDirectActor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	FArcMassTestEngineFastArray ReplicatedItems;
};

// Engine FastArray nested inside a single-member wrapper UPROPERTY.
UCLASS(NotBlueprintable, Hidden, NotPlaceable)
class AArcMassTestEngineFastArrayWrappedActor : public AActor
{
	GENERATED_BODY()

public:
	AArcMassTestEngineFastArrayWrappedActor();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	FArcMassTestEngineFastArrayWrapper Wrapper;
};
