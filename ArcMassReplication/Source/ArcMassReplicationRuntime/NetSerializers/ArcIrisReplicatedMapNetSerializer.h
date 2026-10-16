// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetSerializerArrayStorage.h"
#include "Iris/Serialization/NetSerializerConfig.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Replication/ArcIrisReplicatedMap.h"
#include "ArcIrisReplicatedMapNetSerializer.generated.h"

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcIrisReplicatedMapNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()

	FArcIrisReplicatedMapNetSerializerConfig();

	/** Descriptor built from the companion FXxxEntry struct via CreateDescriptorForStruct. */
	TRefCountPtr<const UE::Net::FReplicationStateDescriptor> EntryDescriptor;

	/** Path to the TMap<K,V> property on the concrete struct. */
	TFieldPath<FMapProperty> MapProperty;

	/** Byte offset of the TMap<K,V> field from the start of the concrete struct instance. */
	uint32 MapOffset = 0;

	/** Byte offset of the "Key" member within FXxxEntry. */
	uint32 EntryKeyOffset = 0;

	/** Byte offset of the "Value" member within FXxxEntry. */
	uint32 EntryValueOffset = 0;

	/** Set to true by BuildNetSerializerConfig on success. Use this (NOT MapOffset==0) as validity guard. */
	bool bInitialized = false;

	uint16 MaxElementCount = 65535;
	uint16 ElementCountBitCount = 16;
};

namespace ArcMassReplication
{
	UE_NET_DECLARE_SERIALIZER(FArcIrisReplicatedMapNetSerializer, ARCMASSREPLICATIONRUNTIME_API);

	using namespace UE::Net;

	struct ARCMASSREPLICATIONRUNTIME_API FArcIrisReplicatedMapPropertyNetSerializerInfo : public FNamedStructPropertyNetSerializerInfo
	{
		FArcIrisReplicatedMapPropertyNetSerializerInfo(const FName InStructName, const FNetSerializer& InSerializer);
		virtual bool IsSupported(const FProperty* Property) const override;
		virtual bool CanUseDefaultConfig(const FProperty* Property) const override { return false; }
		virtual FNetSerializerConfig* BuildNetSerializerConfig(void* NetSerializerConfigBuffer, const FProperty* Property) const override;
	};

	struct FArcIrisReplicatedMapNetSerializer
	{
		static const uint32 Version = 0;

		static constexpr bool bIsForwardingSerializer = false;
		static constexpr bool bHasDynamicState = true;
		static constexpr bool bUseDefaultDelta = false;
		static constexpr bool bUseSerializerIsEqual = true;
		static constexpr bool bHasCustomNetReference = true;

		/** One quantized blob per map entry (whole {Key, Value} as a single FXxxEntry). */
		struct FQuantizedEntry
		{
			FNetSerializerAlignedStorage EntryState;
		};

		struct FDynamicState
		{
			TRefCountPtr<const FReplicationStateDescriptor> EntryDescriptor;
			uint32 MapOffset = 0;
			uint32 EntryKeyOffset = 0;
			uint32 EntryValueOffset = 0;
			TArray<FQuantizedEntry> Entries; // Current snapshot — one blob per map entry; no cached delta lists.
		};

		struct FQuantizedType
		{
			FDynamicState* State = nullptr;
		};

		using QuantizedType = FQuantizedType;
		using ConfigType = FArcIrisReplicatedMapNetSerializerConfig;
		using SourceType = FArcIrisReplicatedMap;

		static ConfigType DefaultConfig;

		static void Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args);
		static void Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args);
		static void Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args);
		static void Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args);
		static void SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args);
		static void DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args);
		static bool IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args);
		static bool Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args);

		// Apply is deferred — not needed for data correctness (Dequantize + default member-apply suffice).
		// Add only when a concrete consumer needs per-entry callbacks.

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
		/** Quantize a single entry blob from the live entry struct memory. */
		static void QuantizeEntry(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const uint8* SrcEntryMemory, FNetSerializerAlignedStorage& OutState);
		/** Write a single entry blob to the bit stream. */
		static void SerializeEntryFromQuantized(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const FNetSerializerAlignedStorage& EntryState);
		/** Read a whole entry from the bit stream into a quantized blob. */
		static void DeserializeEntryToQuantized(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, FNetSerializerAlignedStorage& OutState);
		/** Dequantize a single entry blob into live entry struct memory. */
		static void DequantizeEntry(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const FNetSerializerAlignedStorage& EntryState, uint8* DstEntryMemory);
		/** Deep-clone a single quantized entry blob (handles inner dynamic state). */
		static void CloneQuantizedEntryState(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const FQuantizedEntry& Source, FQuantizedEntry& Target);
		/** Free a single quantized entry blob (releases inner dynamic state then storage). */
		static void FreeQuantizedEntryState(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, FNetSerializerAlignedStorage& EntryState);

		/**
		 * Compare the Key sub-region of two quantized entry blobs.
		 * Uses member[0] of the EntryDescriptor (Key member) via its registered serializer.
		 * O(1) per call; callers drive O(N*M) loops — acceptable for small/medium maps.
		 * Add a key-blob hash index if profiling demands it.
		 */
		static bool EntryKeysAreEqual(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const FNetSerializerAlignedStorage& A, const FNetSerializerAlignedStorage& B);

		/**
		 * Compare the Value sub-region of two quantized entry blobs.
		 * Uses member[1] of the EntryDescriptor (Value member).
		 */
		static bool EntryValuesAreEqual(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const FNetSerializerAlignedStorage& A, const FNetSerializerAlignedStorage& B);

		/** Write only the key bits of an entry blob (member[0] of EntryDescriptor). */
		static void SerializeEntryKeyOnly(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const FNetSerializerAlignedStorage& EntryState);

		/** Read key bits from the stream into a temp zeroed buffer; caller supplies the buffer. */
		static void DeserializeEntryKeyOnly(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, uint8* OutKeyBuffer, uint32 BufferSize);
	};

} // namespace ArcMassReplication

template <> struct TIsPODType<ArcMassReplication::FArcIrisReplicatedMapNetSerializer::FQuantizedType> { enum { Value = true }; };
