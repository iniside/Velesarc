// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

// FArcIrisReplicatedMap — thin base USTRUCT for Iris-replicated TMap<K,V> fragments.
//
// Usage:
//   1. Declare a companion entry struct with exactly two UPROPERTY members named "Key" and "Value":
//        USTRUCT() struct FMyEntry { GENERATED_BODY() UPROPERTY() K Key; UPROPERTY() V Value; };
//   2. Subclass FArcIrisReplicatedMap, add UPROPERTY() TMap<K,V> Entries, override GetEntryStruct():
//        virtual const UScriptStruct* GetEntryStruct() const override { return FMyEntry::StaticStruct(); }
//   3. Keys must be hashable (engine enforces CPF_HasGetValueTypeHash at property registration).
//   4. No mutator API — change detection is auto-diff (Quantize snapshots; SerializeDelta diffs
//      current vs per-connection acked baseline). Delta compression must be enabled on the owning
//      replicated root for SerializeDelta to fire; without it the serializer sends full state.

#include "CoreMinimal.h"
#include "ArcIrisReplicatedMap.generated.h"

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcIrisReplicatedMap
{
	GENERATED_BODY()

	virtual ~FArcIrisReplicatedMap() = default;

	/** Subclasses return the UScriptStruct for their companion FXxxEntry { Key; Value; } struct. */
	virtual const UScriptStruct* GetEntryStruct() const { return nullptr; }

	// --- Optional per-entry notification callbacks ---
	// Signature: void(void* KeyMem, void* ValueMem, FArcIrisReplicatedMap& Map)
	// KeyMem / ValueMem point into a temporary entry struct instance at
	// EntryKeyOffset / EntryValueOffset respectively.
	using FEntryCallback = void(*)(void* KeyMem, void* ValueMem, FArcIrisReplicatedMap& Map);

	FEntryCallback PostReplicatedAddCallback    = nullptr;
	FEntryCallback PostReplicatedChangeCallback = nullptr;
	FEntryCallback PreReplicatedRemoveCallback  = nullptr;

	/** Helper to wire typed lambdas for concrete subclasses. */
	template<typename ConcreteMapType, typename KeyType, typename ValueType>
	void InitCallbacks()
	{
		PostReplicatedAddCallback = [](void* KeyMem, void* ValueMem, FArcIrisReplicatedMap& Map)
		{
			static_cast<ConcreteMapType&>(Map).PostReplicatedAdd(
				*static_cast<KeyType*>(KeyMem),
				*static_cast<ValueType*>(ValueMem));
		};
		PostReplicatedChangeCallback = [](void* KeyMem, void* ValueMem, FArcIrisReplicatedMap& Map)
		{
			static_cast<ConcreteMapType&>(Map).PostReplicatedChange(
				*static_cast<KeyType*>(KeyMem),
				*static_cast<ValueType*>(ValueMem));
		};
		PreReplicatedRemoveCallback = [](void* KeyMem, void* ValueMem, FArcIrisReplicatedMap& Map)
		{
			static_cast<ConcreteMapType&>(Map).PreReplicatedRemove(
				*static_cast<KeyType*>(KeyMem),
				*static_cast<ValueType*>(ValueMem));
		};
	}
};
