// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetSerializerConfig.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Mass/EntityHandle.h"
#include "ArcMassEntityHandleIrisSerializer.generated.h"

USTRUCT()
struct ARCMASSREPLICATIONRUNTIME_API FArcMassEntityHandleIrisSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

namespace ArcMassReplication
{
	UE_NET_DECLARE_SERIALIZER(FArcMassEntityHandleNetSerializer, ARCMASSREPLICATIONRUNTIME_API);

	using namespace UE::Net;

	struct FArcMassEntityHandleNetSerializer
	{
		static const uint32 Version = 0;

		static constexpr bool bIsForwardingSerializer = true;
		static constexpr bool bHasDynamicState = false;
		static constexpr bool bUseDefaultDelta = false;

		struct FQuantizedType
		{
			uint32 NetId;
		};

		using QuantizedType = FQuantizedType;
		using ConfigType = FArcMassEntityHandleIrisSerializerConfig;
		using SourceType = FMassEntityHandle;

		static ConfigType DefaultConfig;

		static void Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args);
		static void Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args);
		static void Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args);
		static void Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args);
		static void SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args);
		static void DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args);
		static void CloneStructMember(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& CloneArgs);
		static bool IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args);
		static bool Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args);

		static void CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args) {}
		static void FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args) {}
		static void CollectNetReferences(FNetSerializationContext& Context, const FNetCollectReferencesArgs& Args) {}

		class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			virtual ~FNetSerializerRegistryDelegates() override;

		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;
			virtual void OnPostFreezeNetSerializerRegistry() override;
		};

		static FArcMassEntityHandleNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
	};
} // namespace ArcMassReplication
