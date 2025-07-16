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

#include "ArcItemIdItemNetSerializer.h"

#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/Serialization/InternalNetSerializers.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"
#include "Items/ArcItemsComponent.h"
/*
namespace Arcx::Net
{
	using namespace UE::Net;
	struct FArcItemIdItemNetSerializer
	{

		// Version
		static const uint32 Version = 0;

		// Traits
		static constexpr bool bIsForwardingSerializer = true; // Triggers asserts if a
function is missing static constexpr bool bHasDynamicState = true; static constexpr bool
bUseDefaultDelta = false;
		// Types
		struct FQuantizedType
		{
			uint32 GuidA;
			uint32 GuidB;
			uint32 GuidC;
			uint32 GuidD;
			uint8 ChangeUpdate;
		};

		typedef FQuantizedType QuantizedType;
		typedef FArcItemIdItemNetSerializerConfig ConfigType;

		static ConfigType DefaultConfig;
		typedef FArcItemIdItem SourceType;

		//just for testing, I'm going to manually bitpack the FArcItemData
		static void Serialize(
			FNetSerializationContext& Context, const FNetSerializeArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
		};
		static void Deserialize(FNetSerializationContext& Context, const
FNetDeserializeArgs& Args)
		{
			QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
		};

		static void Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs&
Args)
		{
			SourceType* Source = reinterpret_cast<SourceType*>(Args.Source);
			QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
		};
		static void Dequantize(FNetSerializationContext& Context, const
FNetDequantizeArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
			SourceType* Target = reinterpret_cast<SourceType*>(Args.Target);
		};

		static void SerializeDelta(FNetSerializationContext& Context, const
FNetSerializeDeltaArgs& Args)
		{
			//let's just assume attributes do not change.
			UE::Net::FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

			QuantizedType* Prev =  reinterpret_cast<QuantizedType*>(Args.Prev);
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
		};

		static void DeserializeDelta(FNetSerializationContext& Context, const
FNetDeserializeDeltaArgs& Args)
		{
			UE::Net::FNetBitStreamReader* Reader = Context.GetBitStreamReader();

			QuantizedType* Prev =  reinterpret_cast<QuantizedType*>(Args.Prev);
			QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
		};

		static void CloneStructMember(FNetSerializationContext& Context, const
FNetCloneDynamicStateArgs& CloneArgs)
		{
		}

		static bool IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs&
Args)
		{
			bool bEqual = false;
			QuantizedType* Source0 = reinterpret_cast<QuantizedType*>(Args.Source0);
			QuantizedType* Source1 = reinterpret_cast<QuantizedType*>(Args.Source1);
			return bEqual;
		};
		static bool Validate(FNetSerializationContext& Context, const FNetValidateArgs&
Args)
		{
			SourceType* Source = reinterpret_cast<SourceType*>(Args.Source);
			return true;
		};

		static void CloneDynamicState(FNetSerializationContext& Context, const
FNetCloneDynamicStateArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
			SourceType* Target = reinterpret_cast<SourceType*>(Args.Target);

		};
		static void FreeDynamicState(FNetSerializationContext& Context, const
FNetFreeDynamicStateArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
		};

		static void CollectNetReferences(FNetSerializationContext& Context, const
FNetCollectReferencesArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
		};


		class FNetSerializerRegistryDelegates final : private
UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			virtual ~FNetSerializerRegistryDelegates() override;

		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;
			virtual void OnPostFreezeNetSerializerRegistry() override;
		};
		static FArcItemIdItemNetSerializer::FNetSerializerRegistryDelegates
NetSerializerRegistryDelegates; static FStructNetSerializerConfig
StructNetSerializerConfig; static const FNetSerializer* StructNetSerializer;
	};


	UE_NET_IMPLEMENT_SERIALIZER(FArcItemIdItemNetSerializer);
	FArcItemIdItemNetSerializer::ConfigType FArcItemIdItemNetSerializer::DefaultConfig =
FArcItemIdItemNetSerializer::ConfigType(); const FNetSerializer*
FArcItemIdItemNetSerializer::StructNetSerializer =
&UE_NET_GET_SERIALIZER(FStructNetSerializer); FStructNetSerializerConfig
FArcItemIdItemNetSerializer::StructNetSerializerConfig;

	FArcItemIdItemNetSerializer::FNetSerializerRegistryDelegates
FArcItemIdItemNetSerializer::NetSerializerRegistryDelegates; static const FName
PropertyNetSerializerRegistry_NAME_ArcItemIdItem("ArcItemIdItem");
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemIdItem,
FArcItemIdItemNetSerializer);
	FArcItemIdItemNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemIdItem);
	}

	void
FArcItemIdItemNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemIdItem);
	}

	void
FArcItemIdItemNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		//UStruct* Struct = FArcGameplayAttribute::StaticStruct();
		//FReplicationStateDescriptorBuilder::FParameters Params;
		//Params.SkipCheckForCustomNetSerializerForStruct = false;
		//StructNetSerializerConfig.StateDescriptor =
FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(Struct, Params);
		//
		//const FReplicationStateDescriptor* Descriptor =
StructNetSerializerConfig.StateDescriptor.GetReference();
		//
		//constexpr SIZE_T OffsetOfGameplayAbilityRepAnimMontage =
offsetof(FQuantizedType, Attribute);
		//if ((sizeof(FQuantizedType::Attribute) < Descriptor->InternalSize) ||
(((OffsetOfGameplayAbilityRepAnimMontage/Descriptor->InternalAlignment)*Descriptor->InternalAlignment)
!= OffsetOfGameplayAbilityRepAnimMontage))
		//{
		//	LowLevelFatalError(TEXT("FQuantizedType::GameplayAbilityRepAnimMontage has
size %u but requires size %u and alignment %u."),
uint32(sizeof(FQuantizedType::Attribute)), uint32(Descriptor->InternalSize),
uint32(Descriptor->InternalAlignment));
		//}
	}

}


*/
