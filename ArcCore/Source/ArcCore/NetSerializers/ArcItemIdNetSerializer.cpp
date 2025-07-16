/**
 * This file is part of ArcX.
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

#include "ArcItemIdNetSerializer.h"

#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/Serialization/InternalNetSerializers.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"

namespace Arcx::Net
{
	using namespace UE::Net;

	// just for testing, I'm going to manually bitpack the FArcItemData
	void FArcItemIdNetSerializer::Serialize(FNetSerializationContext& Context
											, const FNetSerializeArgs& Args)
	{
		QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
		//if (Writer->WriteBool(Source->bValid))
		{
			Writer->WriteBits(Source->GuidA
				, 32U);
			Writer->WriteBits(Source->GuidB
				, 32U);
			Writer->WriteBits(Source->GuidC
				, 32U);
			Writer->WriteBits(Source->GuidD
				, 32U);
		}
	};

	void FArcItemIdNetSerializer::Deserialize(FNetSerializationContext& Context
											  , const FNetDeserializeArgs& Args)
	{
		QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		// Don't rely on the Guid default constructor.
		FGuid Value(0
			, 0
			, 0
			, 0);
		//if(Reader->ReadBool())
		{
			Target->GuidA = Reader->ReadBits(32U);
			Target->GuidB = Reader->ReadBits(32U);
			Target->GuidC = Reader->ReadBits(32U);
			Target->GuidD = Reader->ReadBits(32U);
		}
	};

	void FArcItemIdNetSerializer::Quantize(FNetSerializationContext& Context
										   , const FNetQuantizeArgs& Args)
	{
		QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
		SourceType* Source = reinterpret_cast<SourceType*>(Args.Source);
		Target->bValid = Source->IsValid();
		Source->GetNetSerializer(Target->GuidA
			, Target->GuidB
			, Target->GuidC
			, Target->GuidD);
	};

	void FArcItemIdNetSerializer::Dequantize(FNetSerializationContext& Context
											 , const FNetDequantizeArgs& Args)
	{
		QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
		SourceType* Target = reinterpret_cast<SourceType*>(Args.Target);
		Target->SetNetSerializer(Source->GuidA
			, Source->GuidB
			, Source->GuidC
			, Source->GuidD);
	};

	void FArcItemIdNetSerializer::SerializeDelta(FNetSerializationContext& Context
												 , const FNetSerializeDeltaArgs& Args)
	{
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
		const QuantizedType& SourceValue = *reinterpret_cast<const QuantizedType*>(Args.Source);
		const QuantizedType& PrevValue = *reinterpret_cast<const QuantizedType*>(Args.Prev);

NetSerializeDeltaDefault<Serialize>(Context
			, Args);
			
		//if (SourceValue.GuidA == PrevValue.GuidA
		//	&& SourceValue.GuidB == PrevValue.GuidB
		//	&& SourceValue.GuidC == PrevValue.GuidC
		//	&& SourceValue.GuidD == PrevValue.GuidD)
		//{
		//	Writer->WriteBool(true);
		//}
		//else
		//{
		//	Writer->WriteBool(false);
		//	NetSerializeDeltaDefault<Serialize>(Context
		//	, Args);
		//}
	};

	void FArcItemIdNetSerializer::DeserializeDelta(FNetSerializationContext& Context
												   , const FNetDeserializeDeltaArgs& Args)
	{
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();
		NetDeserializeDeltaDefault<Deserialize>(Context
				, Args);
		//const bool bEqual = Reader->ReadBool();
		//if (bEqual == false)
		//{
		//	NetDeserializeDeltaDefault<Deserialize>(Context
		//		, Args);
		//}
	};

	void FArcItemIdNetSerializer::CloneStructMember(FNetSerializationContext& Context
													, const FNetCloneDynamicStateArgs& CloneArgs)
	{
	}

	bool FArcItemIdNetSerializer::IsEqual(FNetSerializationContext& Context
										  , const FNetIsEqualArgs& Args)
	{
		const QuantizedType& Value0 = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& Value1 = *reinterpret_cast<const QuantizedType*>(Args.Source1);
		if (Value0.GuidA == Value1.GuidA && Value0.GuidB == Value1.GuidB && Value0.GuidC == Value1.GuidC && Value0.GuidD
			== Value1.GuidD)
		{
			return true;
		}
		return false;
	};

	bool FArcItemIdNetSerializer::Validate(FNetSerializationContext& Context
										   , const FNetValidateArgs& Args)
	{
		return true;
	};

	UE_NET_IMPLEMENT_SERIALIZER(FArcItemIdNetSerializer);
	FArcItemIdNetSerializer::ConfigType FArcItemIdNetSerializer::DefaultConfig = FArcItemIdNetSerializer::ConfigType();

	FArcItemIdNetSerializer::FNetSerializerRegistryDelegates FArcItemIdNetSerializer::NetSerializerRegistryDelegates;
	static const FName PropertyNetSerializerRegistry_NAME_ArcItemId("ArcItemId");
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemId
		, FArcItemIdNetSerializer);

	FArcItemIdNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemId);
	}

	void FArcItemIdNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemId);
	}

	void FArcItemIdNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
	}
}
