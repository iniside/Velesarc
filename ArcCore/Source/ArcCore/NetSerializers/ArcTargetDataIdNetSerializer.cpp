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

#include "ArcTargetDataIdNetSerializer.h"

#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/Serialization/InternalNetSerializers.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"

namespace Arcx::Net
{
	using namespace UE::Net;

	// just for testing, I'm going to manually bitpack the FArcItemData
	void FArcTargetDataIdNetSerializer::Serialize(FNetSerializationContext& Context
												  , const FNetSerializeArgs& Args)
	{
		QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
		Writer->WriteBits(Source->Id
			, 16U);
	};

	void FArcTargetDataIdNetSerializer::Deserialize(FNetSerializationContext& Context
													, const FNetDeserializeArgs& Args)
	{
		QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		Target->Id = Reader->ReadBits(16U);
	};

	void FArcTargetDataIdNetSerializer::Quantize(FNetSerializationContext& Context
												 , const FNetQuantizeArgs& Args)
	{
		QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
		SourceType* Source = reinterpret_cast<SourceType*>(Args.Source);
		Target->Id = Source->Handle;
	};

	void FArcTargetDataIdNetSerializer::Dequantize(FNetSerializationContext& Context
												   , const FNetDequantizeArgs& Args)
	{
		QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
		SourceType* Target = reinterpret_cast<SourceType*>(Args.Target);
		Target->Handle = Source->Id;
	};

	void FArcTargetDataIdNetSerializer::SerializeDelta(FNetSerializationContext& Context
													   , const FNetSerializeDeltaArgs& Args)
	{
		NetSerializeDeltaDefault<Serialize>(Context
			, Args);
	};

	void FArcTargetDataIdNetSerializer::DeserializeDelta(FNetSerializationContext& Context
														 , const FNetDeserializeDeltaArgs& Args)
	{
		NetDeserializeDeltaDefault<Deserialize>(Context
			, Args);
	};

	void FArcTargetDataIdNetSerializer::CloneStructMember(FNetSerializationContext& Context
														  , const FNetCloneDynamicStateArgs& CloneArgs)
	{
	}

	bool FArcTargetDataIdNetSerializer::IsEqual(FNetSerializationContext& Context
												, const FNetIsEqualArgs& Args)
	{
		const QuantizedType& Value0 = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& Value1 = *reinterpret_cast<const QuantizedType*>(Args.Source1);
		if (Value0.Id == Value1.Id)
		{
			return true;
		}
		return false;
	};

	bool FArcTargetDataIdNetSerializer::Validate(FNetSerializationContext& Context
												 , const FNetValidateArgs& Args)
	{
		return true;
	};

	UE_NET_IMPLEMENT_SERIALIZER(FArcTargetDataIdNetSerializer);
	FArcTargetDataIdNetSerializer::ConfigType FArcTargetDataIdNetSerializer::DefaultConfig =
			FArcTargetDataIdNetSerializer::ConfigType();

	FArcTargetDataIdNetSerializer::FNetSerializerRegistryDelegates
	FArcTargetDataIdNetSerializer::NetSerializerRegistryDelegates;
	static const FName PropertyNetSerializerRegistry_NAME_ArcTargetDataId("ArcTargetDataId");
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcTargetDataId
		, FArcTargetDataIdNetSerializer);

	FArcTargetDataIdNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcTargetDataId);
	}

	void FArcTargetDataIdNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcTargetDataId);
	}

	void FArcTargetDataIdNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
	}
}
