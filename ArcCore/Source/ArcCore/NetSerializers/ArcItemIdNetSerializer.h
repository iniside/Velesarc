/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"
#include "Iris/Serialization/NetSerializer.h"
#include "Iris/Serialization/NetSerializerConfig.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/Serialization/PolymorphicNetSerializer.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"
#include "Items/ArcItemId.h"

#include "ArcItemIdNetSerializer.generated.h"

USTRUCT()
struct ARCCORE_API FArcItemIdNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()
};

namespace Arcx::Net
{
	UE_NET_DECLARE_SERIALIZER(FArcItemIdNetSerializer
		, ARCCORE_API);

	using namespace UE::Net;

	struct FArcItemIdNetSerializer
	{
		// Version
		static const uint32 Version = 0;

		// Traits
		static constexpr bool bIsForwardingSerializer = true; // Triggers asserts if a function is missing
		static constexpr bool bHasDynamicState = false;
		static constexpr bool bUseDefaultDelta = false;

		// Types
		struct FQuantizedType
		{
			bool bValid;
			uint32 GuidA;
			uint32 GuidB;
			uint32 GuidC;
			uint32 GuidD;
		};

		using QuantizedType = FQuantizedType;
		using ConfigType = FArcItemIdNetSerializerConfig;

		static ConfigType DefaultConfig;
		using SourceType = FArcItemId;

		// just for testing, I'm going to manually bitpack the FArcItemData
		static void Serialize(FNetSerializationContext& Context
							  , const FNetSerializeArgs& Args);

		static void Deserialize(FNetSerializationContext& Context
								, const FNetDeserializeArgs& Args);

		static void Quantize(FNetSerializationContext& Context
							 , const FNetQuantizeArgs& Args);

		static void Dequantize(FNetSerializationContext& Context
							   , const FNetDequantizeArgs& Args);

		static void SerializeDelta(FNetSerializationContext& Context
								   , const FNetSerializeDeltaArgs& Args);

		static void DeserializeDelta(FNetSerializationContext& Context
									 , const FNetDeserializeDeltaArgs& Args);

		static void CloneStructMember(FNetSerializationContext& Context
									  , const FNetCloneDynamicStateArgs& CloneArgs);

		static bool IsEqual(FNetSerializationContext& Context
							, const FNetIsEqualArgs& Args);

		static bool Validate(FNetSerializationContext& Context
							 , const FNetValidateArgs& Args);

		static void CloneDynamicState(FNetSerializationContext& Context
									  , const FNetCloneDynamicStateArgs& Args)
		{
		};

		static void FreeDynamicState(FNetSerializationContext& Context
									 , const FNetFreeDynamicStateArgs& Args)
		{
		};

		static void CollectNetReferences(FNetSerializationContext& Context
										 , const FNetCollectReferencesArgs& Args)
		{
		};

		class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			virtual ~FNetSerializerRegistryDelegates() override;

		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;

			virtual void OnPostFreezeNetSerializerRegistry() override;
		};

		static FArcItemIdNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
	};
}
