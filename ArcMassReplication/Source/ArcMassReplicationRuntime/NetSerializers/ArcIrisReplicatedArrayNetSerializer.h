// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetSerializerArrayStorage.h"
#include "Iris/Serialization/NetSerializerConfig.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Replication/ArcIrisReplicatedArray.h"		
#include "ArcIrisReplicatedArrayNetSerializer.generated.h"

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcIrisReplicatedArrayNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()

	TRefCountPtr<const UE::Net::FReplicationStateDescriptor> ItemDescriptor;
	uint32 ItemsArrayOffset = 0;
	uint32 ItemStride = 0;

	// The concrete UScriptStruct of the item (used by Apply to copy/destroy items between staged and live state).
	const UScriptStruct* ItemStruct = nullptr;
};

namespace ArcMassReplication
{
	UE_NET_DECLARE_SERIALIZER(FArcIrisReplicatedArrayNetSerializer, ARCMASSREPLICATIONRUNTIME_API);

	using namespace UE::Net;

	struct ARCMASSREPLICATIONRUNTIME_API FArcIrisReplicatedArrayPropertyNetSerializerInfo : public FNamedStructPropertyNetSerializerInfo
	{
		FArcIrisReplicatedArrayPropertyNetSerializerInfo(const FName InStructName, const FNetSerializer& InSerializer);
		virtual bool IsSupported(const FProperty* Property) const override;
		virtual bool CanUseDefaultConfig(const FProperty* Property) const override { return false; }
		virtual FNetSerializerConfig* BuildNetSerializerConfig(void* NetSerializerConfigBuffer, const FProperty* Property) const override;
	};

	struct FArcIrisReplicatedArrayNetSerializer
	{
		static const uint32 Version = 0;

		static constexpr bool bIsForwardingSerializer = false;
		static constexpr bool bHasDynamicState = true;
		static constexpr bool bUseDefaultDelta = false;
		static constexpr bool bUseSerializerIsEqual = true;
		static constexpr bool bHasCustomNetReference = true;

		struct FQuantizedItem
		{
			int32 ReplicationID = INDEX_NONE;
			FNetSerializerAlignedStorage QuantizedState;
		};

		struct FDynamicState
		{
			TRefCountPtr<const FReplicationStateDescriptor> ItemDescriptor;
			uint32 ItemsArrayOffset = 0;
			uint32 ItemStride = 0;
			uint64 ChangeVersion = 0;

			// Full snapshot of current items (for full Serialize to new connections)
			TMap<int32, FQuantizedItem> AllItems;

			// Per-quantize dirty lists (server→wire) / received lists (wire→client)
			TArray<int32> AddedIDs;
			TArray<int32> ChangedIDs;
			TArray<int32> RemovedIDs;
		};

		struct FQuantizedType
		{
			FDynamicState* State = nullptr;
		};

		using QuantizedType = FQuantizedType;
		using ConfigType = FArcIrisReplicatedArrayNetSerializerConfig;
		using SourceType = FArcIrisReplicatedArray;

		static ConfigType DefaultConfig;

		static void Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args);
		static void Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args);
		static void Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args);
		static void Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args);
		static void SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args);
		static void DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args);
		static bool IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args);
		static bool Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args);

		static void Apply(FNetSerializationContext& Context, const FNetApplyArgs& Args);

		static void CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args);
		static void FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args);
		static void CollectNetReferences(FNetSerializationContext& Context, const FNetCollectReferencesArgs& Args);

		class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			virtual ~FNetSerializerRegistryDelegates() override;
		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;
			virtual void OnPostFreezeNetSerializerRegistry() override;
		};

		static FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;

	private:
		static void QuantizeItem(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const uint8* SrcMemory, FNetSerializerAlignedStorage& OutQuantizedState);
		static void SerializeItemFromQuantized(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const FNetSerializerAlignedStorage& QuantizedState);
		static void DeserializeItemToQuantized(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, FNetSerializerAlignedStorage& OutQuantizedState);
		static void DequantizeItem(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const FNetSerializerAlignedStorage& QuantizedState, uint8* DstMemory);
		static void CloneQuantizedItemState(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const FQuantizedItem& Source, FQuantizedItem& Target);
		static void FreeQuantizedItemState(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, FNetSerializerAlignedStorage& QuantizedState);
	};
	// Copies a replicated fragment Src→Dst, firing per-member FastArray-style callbacks
	// (PreReplicatedRemove / PostReplicatedAdd / PostReplicatedChange) on Dst's previous
	// state for any FArcIrisReplicatedArray-typed member of the fragment. Non-array
	// UPROPERTY members are bulk-copied via CopyScriptStruct. Non-UPROPERTY counters on
	// Dst survive (CopyScriptStruct doesn't touch them).
	//
	// Use this on the receiver to apply a just-replicated fragment payload (e.g. coming
	// out of FArcIrisEntityArrayNetSerializer's FragmentSlots) into the live consumer
	// memory (e.g. Mass entity fragment), so callbacks fire on the consumer's instance.
	ARCMASSREPLICATIONRUNTIME_API void ApplyReplicatedFragmentToTarget(
		uint8* DstFragmentMemory,
		const uint8* SrcFragmentMemory,
		const UScriptStruct* FragmentType,
		const UE::Net::FReplicationStateDescriptor* FragmentDescriptor);

} // namespace ArcMassReplication

template <> struct TIsPODType<ArcMassReplication::FArcIrisReplicatedArrayNetSerializer::FQuantizedType> { enum { Value = true }; };
