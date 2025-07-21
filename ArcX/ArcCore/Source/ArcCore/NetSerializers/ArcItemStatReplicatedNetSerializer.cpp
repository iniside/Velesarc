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

#include "ArcItemStatReplicatedNetSerializer.h"

#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/Serialization/InternalNetSerializers.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"

namespace Arcx::Net
{
	using namespace UE::Net;

	struct FArcItemStatReplicatedNetSerializer
	{
		// Version
		static const uint32 Version = 0;

		// Traits
		static constexpr bool bIsForwardingSerializer = true; // Triggers asserts if a function is missing
		static constexpr bool bHasDynamicState = true;
		static constexpr bool bUseDefaultDelta = false;

		// Types
		struct FQuantizedType
		{
			float StatValue;
			alignas(8) uint8 Attribute[512];
		};

		using QuantizedType = FQuantizedType;
		using ConfigType = FArcItemStatReplicatedNetSerializerConfig;

		static ConfigType DefaultConfig;
		using SourceType = FArcItemStatReplicated;

		// just for testing, I'm going to manually bitpack the FArcItemData
		static void Serialize(FNetSerializationContext& Context
							  , const FNetSerializeArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
			UE::Net::FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
			{
				const FNetSerializer& FloatSerializer = UE_NET_GET_SERIALIZER(FFloatNetSerializer);

				FNetSerializeArgs FloatNetSerializeArgs = {};
				FloatNetSerializeArgs.Version = FloatSerializer.Version;
				FloatNetSerializeArgs.Source = NetSerializerValuePointer(&Source->StatValue);
				FloatNetSerializeArgs.NetSerializerConfig = NetSerializerConfigParam(FloatSerializer.DefaultConfig);
				FloatSerializer.Serialize(Context
					, FloatNetSerializeArgs);
			}

			FNetSerializeArgs StructArgs = Args;
			StructArgs.Source = NetSerializerValuePointer(&Source->Attribute);
			StructArgs.NetSerializerConfig = &StructNetSerializerConfig;
			StructNetSerializer->Serialize(Context
				, StructArgs);
		};

		static void Deserialize(FNetSerializationContext& Context
								, const FNetDeserializeArgs& Args)
		{
			QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
			UE::Net::FNetBitStreamReader* Reader = Context.GetBitStreamReader();
			{
				const FNetSerializer& FloatSerializer = UE_NET_GET_SERIALIZER(FFloatNetSerializer);

				FNetDeserializeArgs FloatNetDeserializeArgs = {};
				FloatNetDeserializeArgs.Version = FloatSerializer.Version;
				FloatNetDeserializeArgs.Target = NetSerializerValuePointer(&Target->StatValue);
				FloatNetDeserializeArgs.NetSerializerConfig = NetSerializerConfigParam(FloatSerializer.DefaultConfig);
				FloatSerializer.Deserialize(Context
					, FloatNetDeserializeArgs);
			}

			FNetDeserializeArgs StructArgs = Args;
			StructArgs.Target = NetSerializerValuePointer(&Target->Attribute);
			StructArgs.NetSerializerConfig = &StructNetSerializerConfig;
			StructNetSerializer->Deserialize(Context
				, StructArgs);
		};

		static void Quantize(FNetSerializationContext& Context
							 , const FNetQuantizeArgs& Args)
		{
			SourceType* Source = reinterpret_cast<SourceType*>(Args.Source);
			QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);

			Target->StatValue = Source->FinalValue;

			FNetQuantizeArgs StructArgs = Args;
			StructArgs.Target = NetSerializerValuePointer(&Target->Attribute);
			StructArgs.Source = NetSerializerValuePointer(&Source->Attribute);
			StructArgs.NetSerializerConfig = &StructNetSerializerConfig;
			StructNetSerializer->Quantize(Context
				, StructArgs);
		};

		static void Dequantize(FNetSerializationContext& Context
							   , const FNetDequantizeArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
			SourceType* Target = reinterpret_cast<SourceType*>(Args.Target);

			Target->FinalValue = Source->StatValue;

			FNetDequantizeArgs StructArgs = Args;
			StructArgs.Target = NetSerializerValuePointer(&Target->Attribute);
			StructArgs.Source = NetSerializerValuePointer(&Source->Attribute);
			StructArgs.NetSerializerConfig = &StructNetSerializerConfig;
			StructNetSerializer->Dequantize(Context
				, StructArgs);
		};

		static void SerializeDelta(FNetSerializationContext& Context
								   , const FNetSerializeDeltaArgs& Args)
		{
			// let's just assume attributes do not change.
			UE::Net::FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

			QuantizedType* Prev = reinterpret_cast<QuantizedType*>(Args.Prev);
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
			bool bStatValueChanged = Prev->StatValue != Source->StatValue;

			Writer->WriteBool(bStatValueChanged);
			if (bStatValueChanged)
			{
				const FNetSerializer& FloatSerializer = UE_NET_GET_SERIALIZER(FFloatNetSerializer);

				FNetSerializeDeltaArgs FloatNetSerializeArgs = {};
				FloatNetSerializeArgs.Version = FloatSerializer.Version;
				FloatNetSerializeArgs.Prev = NetSerializerValuePointer(&Prev->StatValue);
				FloatNetSerializeArgs.Source = NetSerializerValuePointer(&Source->StatValue);
				FloatNetSerializeArgs.NetSerializerConfig = NetSerializerConfigParam(FloatSerializer.DefaultConfig);
				FloatSerializer.SerializeDelta(Context
					, FloatNetSerializeArgs);
			}
		};

		static void DeserializeDelta(FNetSerializationContext& Context
									 , const FNetDeserializeDeltaArgs& Args)
		{
			UE::Net::FNetBitStreamReader* Reader = Context.GetBitStreamReader();

			QuantizedType* Prev = reinterpret_cast<QuantizedType*>(Args.Prev);
			QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
			bool bStatValueChanged = Reader->ReadBool();

			if (bStatValueChanged)
			{
				const FNetSerializer& FloatSerializer = UE_NET_GET_SERIALIZER(FFloatNetSerializer);

				FNetDeserializeDeltaArgs FloatNetDeserializeArgs = {};
				FloatNetDeserializeArgs.Version = FloatSerializer.Version;
				FloatNetDeserializeArgs.Prev = NetSerializerValuePointer(&Prev->StatValue);
				FloatNetDeserializeArgs.Target = NetSerializerValuePointer(&Target->StatValue);
				FloatNetDeserializeArgs.NetSerializerConfig = NetSerializerConfigParam(FloatSerializer.DefaultConfig);
				FloatSerializer.DeserializeDelta(Context
					, FloatNetDeserializeArgs);
			}
		};

		static void CloneStructMember(FNetSerializationContext& Context
									  , const FNetCloneDynamicStateArgs& CloneArgs)
		{
		}

		static bool IsEqual(FNetSerializationContext& Context
							, const FNetIsEqualArgs& Args)
		{
			bool bEqual = false;
			QuantizedType* Source0 = reinterpret_cast<QuantizedType*>(Args.Source0);
			QuantizedType* Source1 = reinterpret_cast<QuantizedType*>(Args.Source1);

			FNetIsEqualArgs StructArgs = Args;
			StructArgs.Source0 = NetSerializerValuePointer(&Source0->Attribute);
			StructArgs.Source1 = NetSerializerValuePointer(&Source1->Attribute);
			StructArgs.NetSerializerConfig = &StructNetSerializerConfig;

			if (Source0->StatValue == Source1->StatValue && StructNetSerializer->IsEqual(Context
					, StructArgs))
			{
				bEqual = true;
			}
			return bEqual;
		};

		static bool Validate(FNetSerializationContext& Context
							 , const FNetValidateArgs& Args)
		{
			SourceType* Source = reinterpret_cast<SourceType*>(Args.Source);
			FNetValidateArgs ValidateArgs = {};
			ValidateArgs.NetSerializerConfig = &StructNetSerializerConfig;
			ValidateArgs.Source = NetSerializerValuePointer(&Source->Attribute);
			if (!StructNetSerializer->Validate(Context
				, ValidateArgs))
			{
				return false;
			}

			return true;
		};

		static void CloneDynamicState(FNetSerializationContext& Context
									  , const FNetCloneDynamicStateArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
			SourceType* Target = reinterpret_cast<SourceType*>(Args.Target);
			FNetCloneDynamicStateArgs StructArgs = Args;
			StructArgs.Source = NetSerializerValuePointer(&Source->Attribute);
			StructArgs.Target = NetSerializerValuePointer(&Target->Attribute);
			StructArgs.NetSerializerConfig = &StructNetSerializerConfig;
			StructNetSerializer->CloneDynamicState(Context
				, StructArgs);
		};

		static void FreeDynamicState(FNetSerializationContext& Context
									 , const FNetFreeDynamicStateArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
			FNetFreeDynamicStateArgs StructArgs = Args;
			StructArgs.Source = NetSerializerValuePointer(&Source->Attribute);
			StructArgs.NetSerializerConfig = &StructNetSerializerConfig;
			StructNetSerializer->FreeDynamicState(Context
				, StructArgs);
		};

		static void CollectNetReferences(FNetSerializationContext& Context
										 , const FNetCollectReferencesArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
			FNetCollectReferencesArgs StructArgs = Args;
			StructArgs.Source = NetSerializerValuePointer(&Source->Attribute);
			StructArgs.NetSerializerConfig = &StructNetSerializerConfig;
			StructNetSerializer->CollectNetReferences(Context
				, StructArgs);
		};

		class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			virtual ~FNetSerializerRegistryDelegates() override;

		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;

			virtual void OnPostFreezeNetSerializerRegistry() override;
		};

		static FArcItemStatReplicatedNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
		static FStructNetSerializerConfig StructNetSerializerConfig;
		static const FNetSerializer* StructNetSerializer;
	};

	UE_NET_IMPLEMENT_SERIALIZER(FArcItemStatReplicatedNetSerializer);
	FArcItemStatReplicatedNetSerializer::ConfigType FArcItemStatReplicatedNetSerializer::DefaultConfig =
			FArcItemStatReplicatedNetSerializer::ConfigType();
	const FNetSerializer* FArcItemStatReplicatedNetSerializer::StructNetSerializer = &UE_NET_GET_SERIALIZER(
		FStructNetSerializer);
	FStructNetSerializerConfig FArcItemStatReplicatedNetSerializer::StructNetSerializerConfig;

	FArcItemStatReplicatedNetSerializer::FNetSerializerRegistryDelegates
	FArcItemStatReplicatedNetSerializer::NetSerializerRegistryDelegates;
	//static const FName PropertyNetSerializerRegistry_NAME_ArcItemStatReplicated("ArcItemStatReplicated");
	//UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemStatReplicated
	//	, FArcItemStatReplicatedNetSerializer);

	FArcItemStatReplicatedNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		//UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemStatReplicated);
	}

	void FArcItemStatReplicatedNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		//UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemStatReplicated);
	}

	void FArcItemStatReplicatedNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		UStruct* Struct = FGameplayAttribute::StaticStruct();
		FReplicationStateDescriptorBuilder::FParameters Params;
		Params.SkipCheckForCustomNetSerializerForStruct = false;
		StructNetSerializerConfig.StateDescriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(Struct
			, Params);

		const FReplicationStateDescriptor* Descriptor = StructNetSerializerConfig.StateDescriptor.GetReference();

		constexpr SIZE_T OffsetOfGameplayAbilityRepAnimMontage = offsetof(FQuantizedType
			, Attribute);
		if ((sizeof(FQuantizedType::Attribute) < Descriptor->InternalSize) || (
				((OffsetOfGameplayAbilityRepAnimMontage / Descriptor->InternalAlignment) * Descriptor->
				 InternalAlignment) != OffsetOfGameplayAbilityRepAnimMontage))
		{
			LowLevelFatalError(
				TEXT( "FQuantizedType::GameplayAbilityRepAnimMontage has size %u but requires size %u and alignment %u."
				)
				, uint32(sizeof(FQuantizedType::Attribute))
				, uint32(Descriptor->InternalSize)
				, uint32(Descriptor->InternalAlignment));
		}
	}
}
