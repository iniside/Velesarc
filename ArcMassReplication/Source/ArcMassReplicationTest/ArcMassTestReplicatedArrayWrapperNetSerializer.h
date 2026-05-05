// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetSerializerConfig.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "NetSerializers/ArcIrisReplicatedArrayNetSerializer.h"
#include "ArcMassTestItemFragment.h"
#include "ArcMassTestReplicatedArrayWrapperNetSerializer.generated.h"

USTRUCT()
struct FArcMassTestReplicatedArrayWrapperNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()

	uint32 InnerMemberOffset = 0;
	FArcIrisReplicatedArrayNetSerializerConfig InnerConfig;
};

namespace ArcMassTestReplication
{
	UE_NET_DECLARE_SERIALIZER(FArcMassTestReplicatedArrayWrapperNetSerializer, );

	using namespace UE::Net;

	struct FArcMassTestReplicatedArrayWrapperPropertyNetSerializerInfo : public FNamedStructPropertyNetSerializerInfo
	{
		FArcMassTestReplicatedArrayWrapperPropertyNetSerializerInfo(const FName InStructName, const FNetSerializer& InSerializer);
		virtual bool IsSupported(const FProperty* Property) const override;
		virtual bool CanUseDefaultConfig(const FProperty* Property) const override { return false; }
		virtual FNetSerializerConfig* BuildNetSerializerConfig(void* NetSerializerConfigBuffer, const FProperty* Property) const override;
	};

	struct FArcMassTestReplicatedArrayWrapperNetSerializer
	{
		static const uint32 Version = 0;

		static constexpr bool bIsForwardingSerializer = false;
		static constexpr bool bHasDynamicState = true;
		static constexpr bool bUseDefaultDelta = false;
		static constexpr bool bUseSerializerIsEqual = true;
		static constexpr bool bHasCustomNetReference = true;

		using QuantizedType = ArcMassReplication::FArcIrisReplicatedArrayNetSerializer::QuantizedType;
		using ConfigType = FArcMassTestReplicatedArrayWrapperNetSerializerConfig;
		using SourceType = FArcMassTestReplicatedItemArrayWrapper;

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
	};
} // namespace ArcMassTestReplication
