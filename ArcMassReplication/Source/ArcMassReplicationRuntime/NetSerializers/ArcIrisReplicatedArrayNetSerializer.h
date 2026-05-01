// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
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

		struct FQuantizedItem
		{
			int32 ReplicationID = INDEX_NONE;
			uint32 ReplicationKey = 0;
			TArray<uint8> QuantizedState;
		};

		struct FDynamicState
		{
			TRefCountPtr<const FReplicationStateDescriptor> ItemDescriptor;
			uint32 ItemsArrayOffset = 0;
			uint32 ItemStride = 0;

			TMap<int32, FQuantizedItem> Baseline;
			TArray<int32> RemovedReplicationIDs;
			uint64 ChangeVersion = 0;

			TArray<int32> ReceivedDirtyIDs;
			TArray<int32> ReceivedRemovedIDs;
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
		static void CollectNetReferences(FNetSerializationContext& Context, const FNetCollectReferencesArgs& Args) {}

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
		static void QuantizeItem(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const uint8* SrcMemory, TArray<uint8>& OutQuantizedState);
		static void SerializeItemFromQuantized(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const TArray<uint8>& QuantizedState);
		static void DeserializeItemToQuantized(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, TArray<uint8>& OutQuantizedState);
		static void DequantizeItem(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, const TArray<uint8>& QuantizedState, uint8* DstMemory);
		static void FreeQuantizedItemState(const FReplicationStateDescriptor* Descriptor, TArray<uint8>& QuantizedState);
	};
} // namespace ArcMassReplication

template <> struct TIsPODType<ArcMassReplication::FArcIrisReplicatedArrayNetSerializer::FQuantizedType> { enum { Value = true }; };
