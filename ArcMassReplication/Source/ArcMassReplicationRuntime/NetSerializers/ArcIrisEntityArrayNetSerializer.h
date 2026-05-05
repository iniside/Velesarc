// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetSerializerArrayStorage.h"
#include "Iris/Serialization/NetSerializerConfig.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Replication/ArcIrisEntityArray.h"
#include "Replication/ArcMassReplicationDescriptorSet.h"
#include "ArcIrisEntityArrayNetSerializer.generated.h"

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcIrisEntityArrayNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

namespace ArcMassReplication
{
	UE_NET_DECLARE_SERIALIZER(FArcIrisEntityArrayNetSerializer, ARCMASSREPLICATIONRUNTIME_API);

	using namespace UE::Net;

	struct FArcIrisEntityArrayNetSerializer
	{
		static const uint32 Version = 0;

		static constexpr bool bIsForwardingSerializer = false;
		static constexpr bool bHasDynamicState = true;
		static constexpr bool bUseDefaultDelta = false;
		static constexpr bool bUseSerializerIsEqual = true;
		static constexpr bool bHasCustomNetReference = true;

		struct FQuantizedFragmentSlot
		{
			FNetSerializerAlignedStorage Buffer;
			const UE::Net::FReplicationStateDescriptor* Descriptor = nullptr;
			const UScriptStruct* FragmentType = nullptr;

			bool IsValid() const { return Buffer.Num() > 0 && Descriptor != nullptr; }
		};

		struct FQuantizedEntity
		{
			uint32 NetIdValue = 0;
			uint32 ReplicationKey = 0;
			uint8 FragmentCount = 0;
			TArray<FQuantizedFragmentSlot> FragmentSlots;
		};

		struct FDynamicState
		{
			uint32 DescriptorSetHash = 0;
			TMap<uint32, FQuantizedEntity> Baseline;
			TArray<uint32> RemovedNetIds;
			uint64 ChangeVersion = 0;

			TArray<uint32> ReceivedDirtyNetIds;
			TArray<uint32> ReceivedRemovedNetIds;
		};

		struct FQuantizedType
		{
			FDynamicState* State = nullptr;
		};

		using QuantizedType = FQuantizedType;
		using ConfigType = FArcIrisEntityArrayNetSerializerConfig;
		using SourceType = FArcIrisEntityArray;

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

		static FArcIrisEntityArrayNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;

	private:
		static void CollectFragmentReferences(UE::Net::FNetSerializationContext& Context, const FQuantizedFragmentSlot& Slot, const UE::Net::FNetCollectReferencesArgs& Args);
		static void QuantizeFragmentSlot(UE::Net::FNetSerializationContext& Context, const UE::Net::FReplicationStateDescriptor* Descriptor, const UScriptStruct* FragType, const uint8* SrcMemory, FQuantizedFragmentSlot& OutSlot);
		static void SerializeFragmentFromQuantized(UE::Net::FNetSerializationContext& Context, const FQuantizedFragmentSlot& Slot);
		static void SerializeFragmentDelta(UE::Net::FNetSerializationContext& Context, const FQuantizedFragmentSlot& CurrSlot, const FQuantizedFragmentSlot& PrevSlot);
		static void DeserializeFragmentToQuantized(UE::Net::FNetSerializationContext& Context, const UE::Net::FReplicationStateDescriptor* Descriptor, const UScriptStruct* FragType, FQuantizedFragmentSlot& Slot);
		static void DequantizeFragmentSlot(UE::Net::FNetSerializationContext& Context, const FQuantizedFragmentSlot& Slot, FInstancedStruct& OutDst);
		static void CloneFragmentSlot(UE::Net::FNetSerializationContext& Context, const FQuantizedFragmentSlot& Src, FQuantizedFragmentSlot& Dst);
		static void FreeFragmentSlot(UE::Net::FNetSerializationContext& Context, FQuantizedFragmentSlot& Slot);
		static uint32 FilterFragmentMaskForConnection(UE::Net::FNetSerializationContext& Context, uint32 NetIdValue, uint32 FragmentMask, const FArcMassReplicationDescriptorSet* DescSet);
		static bool ShouldFilterEntityForConnection(UE::Net::FNetSerializationContext& Context, uint32 NetIdValue);
		static const FArcMassReplicationDescriptorSet* GetDescriptorSetFromContext(UE::Net::FNetSerializationContext& Context, uint32 Hash);
	};
} // namespace ArcMassReplication

template <> struct TIsPODType<ArcMassReplication::FArcIrisEntityArrayNetSerializer::FQuantizedType> { enum { Value = true }; };
