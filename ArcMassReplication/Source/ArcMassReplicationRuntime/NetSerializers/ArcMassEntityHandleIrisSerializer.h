// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetSerializerConfig.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/ObjectNetSerializer.h"
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

	/**
	 * Forwarding NetSerializer that wires FMassEntityHandle through Iris's standard
	 * UObject reference path. The wire format is the FNetRefHandle of the entity's
	 * UArcMassEntityVessel; resolution machinery, retry-on-unresolved, and reference
	 * collection are delegated entirely to FObjectNetSerializer.
	 *
	 * Server-side Quantize: FMassEntityHandle -> FindVesselForEntity -> UObject* ->
	 *   FObjectNetSerializer::Quantize stores it as a NetObjectReference.
	 * Client-side Dequantize: FObjectNetSerializer::Dequantize resolves to UObject*
	 *   -> Cast to UArcMassEntityVessel -> read Vessel->EntityHandle.
	 *
	 * Quantized storage matches FObjectNetSerializer's quantized type
	 * (FObjectNetSerializerQuantizedReferenceStorage) so we can forward all the
	 * dynamic-state / reference-collection callbacks 1:1.
	 */
	struct FArcMassEntityHandleNetSerializer
	{
		static const uint32 Version = 0;

		static constexpr bool bIsForwardingSerializer = true;
		static constexpr bool bHasDynamicState = true;
		static constexpr bool bHasCustomNetReference = true;
		// bUseDefaultDelta = true matches FObjectNetSerializer's own default (it
		// inherits the NetSerializer.h default of true and provides no explicit
		// Delta methods, so Iris synthesizes them via NetSerializeDeltaDefault).
		// Forwarding serializers (bIsForwardingSerializer = true) are nonetheless
		// required by NetSerializerBuilder to implement SerializeDelta /
		// DeserializeDelta themselves, so we do — they simply forward to
		// FObjectNetSerializer's (synthesized) Delta entry points.
		static constexpr bool bUseDefaultDelta = true;

		using QuantizedType = FObjectNetSerializerQuantizedReferenceStorage;
		using ConfigType = FArcMassEntityHandleIrisSerializerConfig;
		using SourceType = FMassEntityHandle;

		static ConfigType DefaultConfig;

		static void Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args);
		static void Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args);
		static void Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args);
		static void Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args);
		static void SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args);
		static void DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args);
		static bool IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args);
		static bool Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args);

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

		static FArcMassEntityHandleNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
	};
} // namespace ArcMassReplication
