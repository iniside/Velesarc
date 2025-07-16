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

#include "ArcGameplayEffectContextNetSerializer.h"

#include "ArcGameplayEffectContext.h"
#include "GameplayEffectTypes.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Net/Core/NetBitArray.h"
#include "Serialization/GameplayEffectContextNetSerializer.h"

#include "ArcItemIdNetSerializer.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsComponent.h"
#include "Items/ArcItemsStoreComponent.h"

namespace Arcx::Net
{
	using namespace UE::Net;

	//
	struct FArcGameplayEffectContextNetSerializer
	{
		// Version
		static const uint32 Version = 0;

		// Traits
		static constexpr bool bIsForwardingSerializer = true; // Triggers asserts if a function is missing
		static constexpr bool bHasCustomNetReference = true;
		static constexpr bool bHasDynamicState = true;
		struct FQuantizedType
		{
			alignas(16) uint8 GameplayEffectContext[512]; /* Must be large enough to hold a quantized copy of the GE */
			FArcItemIdNetSerializer::QuantizedType ItemId;
		};

		// This is a bit backwards but we don't want to expose the quantized type in
		// public headers.

		using SourceType = FArcGameplayEffectContext;
		using QuantizedType = FQuantizedType;
		using ConfigType = FArcGameplayEffectContextNetSerializerConfig;

		static const ConfigType DefaultConfig;

		static void Serialize(FNetSerializationContext& Context
							  , const FNetSerializeArgs& Args)
		{
			const QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Source);
			// Forward to existing GameplayEffectContextNetSerializser
			{
				FNetSerializeArgs BaseArgs = Args;
				BaseArgs.NetSerializerConfig = &StructNetSerializerConfigForGE;
				BaseArgs.Source = NetSerializerValuePointer(&Value.GameplayEffectContext);
				StructNetSerializer->Serialize(Context, BaseArgs);
			}
			{
				FNetSerializeArgs Base = Args;
				Base.Source = NetSerializerValuePointer(&Value.ItemId);
				Base.NetSerializerConfig = &FArcItemIdNetSerializer::DefaultConfig;
				ArcItemIdNetSerializer->Serialize(Context
					, Args);
			}
		}

		static void Deserialize(FNetSerializationContext& Context
								, const FNetDeserializeArgs& Args)
		{
			QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
			// Forward to existing GameplayEffectContextNetSerializser
			{
				FNetDeserializeArgs BaseArgs = Args;
				BaseArgs.NetSerializerConfig = &StructNetSerializerConfigForGE;
				BaseArgs.Target = NetSerializerValuePointer(&Target.GameplayEffectContext);
				StructNetSerializer->Deserialize(Context, BaseArgs);
			}
			{
				FNetDeserializeArgs Base = Args;
				Base.Target = NetSerializerValuePointer(&Target.ItemId);
				Base.NetSerializerConfig = &FArcItemIdNetSerializer::DefaultConfig;
				ArcItemIdNetSerializer->Deserialize(Context
					, Args);
			}
		}

		static void SerializeDelta(FNetSerializationContext& Context
								   , const FNetSerializeDeltaArgs& Args)
		{
			NetSerializeDeltaDefault<Serialize>(Context
				, Args);
		}

		static void DeserializeDelta(FNetSerializationContext& Context
									 , const FNetDeserializeDeltaArgs& Args)
		{
			NetDeserializeDeltaDefault<Deserialize>(Context
				, Args);
		}

		static void Quantize(FNetSerializationContext& Context
							 , const FNetQuantizeArgs& Args)
		{
			const SourceType& SourceValue = *reinterpret_cast<const SourceType*>(Args.Source);
			QuantizedType& TargetValue = *reinterpret_cast<QuantizedType*>(Args.Target);
			// Forward to existing GameplayEffectContextNetSerializser
			{
				FNetQuantizeArgs GEQuantizeArgs = Args;
				GEQuantizeArgs.NetSerializerConfig = &StructNetSerializerConfigForGE;
				GEQuantizeArgs.Source = NetSerializerValuePointer(&SourceValue);
				GEQuantizeArgs.Target = NetSerializerValuePointer(&TargetValue.GameplayEffectContext);
				StructNetSerializer->Quantize(Context, GEQuantizeArgs);
			}
			{
				FNetQuantizeArgs Base = Args;
				Base.Target = NetSerializerValuePointer(&TargetValue.ItemId);
				Base.Source = NetSerializerValuePointer(&SourceValue.SourceItemId);
				Base.NetSerializerConfig = &FArcItemIdNetSerializer::DefaultConfig;
				ArcItemIdNetSerializer->Quantize(Context
					, Base);
			}
		}

		static void Dequantize(FNetSerializationContext& Context
							   , const FNetDequantizeArgs& Args)
		{
			const QuantizedType& SourceValue = *reinterpret_cast<const QuantizedType*>(Args.Source);
			SourceType* TargetValue = reinterpret_cast<SourceType*>(Args.Target);
			
			// Forward to existing GameplayEffectContextNetSerializser
			{
				FNetDequantizeArgs GEQuantizeArgs = Args;
				GEQuantizeArgs.NetSerializerConfig = &StructNetSerializerConfigForGE;
				GEQuantizeArgs.Source = NetSerializerValuePointer(&SourceValue.GameplayEffectContext);
				//GEQuantizeArgs.Target = NetSerializerValuePointer(&TargetValue);
				StructNetSerializer->Dequantize(Context, GEQuantizeArgs);
			}
			
			{
				FNetDequantizeArgs Base = Args;
				Base.Source = NetSerializerValuePointer(&SourceValue.ItemId);
				Base.Target = NetSerializerValuePointer(&TargetValue->SourceItemId);

				Base.NetSerializerConfig = &FArcItemIdNetSerializer::DefaultConfig;
				ArcItemIdNetSerializer->Dequantize(Context
					, Base);
			}
			if (TargetValue->GetSourceObject())
			{
				if (UArcItemsStoreComponent* Source = Cast<UArcItemsStoreComponent>(TargetValue->GetSourceObject()))
				{
					TargetValue->SourceItemPtr = Source->GetItemPtr(TargetValue->SourceItemId);
					if (TargetValue->SourceItemPtr)
					{
						TargetValue->SourceItem = TargetValue->SourceItemPtr->GetItemDefinition();
					}
				}
			}
		}

		static bool IsEqual(FNetSerializationContext& Context
							, const FNetIsEqualArgs& Args)
		{
			const QuantizedType& Value0 = *reinterpret_cast<const QuantizedType*>(Args.Source0);
			const QuantizedType& Value1 = *reinterpret_cast<const QuantizedType*>(Args.Source1);
			
			{
				FNetIsEqualArgs GEQuantizeArgs = Args;
				GEQuantizeArgs.NetSerializerConfig = &StructNetSerializerConfigForGE;
				GEQuantizeArgs.Source0 = NetSerializerValuePointer(&Value0.GameplayEffectContext);
				GEQuantizeArgs.Source1 = NetSerializerValuePointer(&Value1.GameplayEffectContext);
				return StructNetSerializer->IsEqual(Context, GEQuantizeArgs);
			}
		}

		static bool Validate(FNetSerializationContext& Context
							 , const FNetValidateArgs& Args)
		{
			const SourceType& SourceValue = *reinterpret_cast<const SourceType*>(Args.Source);

			{
				FNetValidateArgs GEQuantizeArgs = Args;
				GEQuantizeArgs.Source = NetSerializerValuePointer(&SourceValue);
				GEQuantizeArgs.NetSerializerConfig = &StructNetSerializerConfigForGE;
				return StructNetSerializer->Validate(Context, Args);
			}
		}

		static void CloneDynamicState(FNetSerializationContext& Context
									  , const FNetCloneDynamicStateArgs& Args)
		{
			const QuantizedType& SourceValue = *reinterpret_cast<const QuantizedType*>(Args.Source);
			QuantizedType& TargetValue = *reinterpret_cast<QuantizedType*>(Args.Target);

			{
				FNetCloneDynamicStateArgs GEQuantizeArgs = Args;
				GEQuantizeArgs.NetSerializerConfig = &StructNetSerializerConfigForGE;
				GEQuantizeArgs.Source = NetSerializerValuePointer(&SourceValue.GameplayEffectContext);
				GEQuantizeArgs.Target = NetSerializerValuePointer(&TargetValue.GameplayEffectContext);
				StructNetSerializer->CloneDynamicState(Context, GEQuantizeArgs);
			}
		}

		static void FreeDynamicState(FNetSerializationContext& Context
									 , const FNetFreeDynamicStateArgs& Args)
		{
			const QuantizedType& SourceValue = *reinterpret_cast<const QuantizedType*>(Args.Source);
			{
				FNetFreeDynamicStateArgs GEQuantizeArgs = Args;
				GEQuantizeArgs.NetSerializerConfig = &StructNetSerializerConfigForGE;
				GEQuantizeArgs.Source = NetSerializerValuePointer(&SourceValue.GameplayEffectContext);
				StructNetSerializer->FreeDynamicState(Context, GEQuantizeArgs);
			}
		}

		static void CollectNetReferences(FNetSerializationContext& Context
										 , const FNetCollectReferencesArgs& Args)
		{
			const QuantizedType& Value = *reinterpret_cast<const QuantizedType*>(Args.Source);
			{
				FNetCollectReferencesArgs GEQuantizeArgs = Args;// {};
				GEQuantizeArgs.NetSerializerConfig = &StructNetSerializerConfigForGE;
				GEQuantizeArgs.Source = NetSerializerValuePointer(&Value.GameplayEffectContext);
				StructNetSerializer->CollectNetReferences(Context, GEQuantizeArgs);
			}
		}

	private:
		class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			virtual ~FNetSerializerRegistryDelegates() override;

		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;

			virtual void OnPostFreezeNetSerializerRegistry() override;
		};

		static FArcGameplayEffectContextNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
		
		static const FNetSerializer* ArcItemIdNetSerializer;

		static FStructNetSerializerConfig StructNetSerializerConfigForGE;
		static const FNetSerializer* StructNetSerializer;
		static UE::Net::EReplicationStateTraits GEStateTraits;
	};

	UE_NET_IMPLEMENT_SERIALIZER(FArcGameplayEffectContextNetSerializer);

	const FArcGameplayEffectContextNetSerializer::ConfigType FArcGameplayEffectContextNetSerializer::DefaultConfig;
	FArcGameplayEffectContextNetSerializer::FNetSerializerRegistryDelegates FArcGameplayEffectContextNetSerializer::NetSerializerRegistryDelegates;

	// this config will be initialzied in PostFreeze
	FStructNetSerializerConfig FArcGameplayEffectContextNetSerializer::StructNetSerializerConfigForGE;
	// lookup default serializers
	const FNetSerializer* FArcGameplayEffectContextNetSerializer::StructNetSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
	
	
	const FNetSerializer* FArcGameplayEffectContextNetSerializer::ArcItemIdNetSerializer = &UE_NET_GET_SERIALIZER(FArcItemIdNetSerializer);
	EReplicationStateTraits FArcGameplayEffectContextNetSerializer::GEStateTraits;
	
	static const FName PropertyNetSerializerRegistry_NAME_ArcGameplayEffectContext("ArcGameplayEffectContext");
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcGameplayEffectContext, FArcGameplayEffectContextNetSerializer);

	FArcGameplayEffectContextNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcGameplayEffectContext);
	}

	void FArcGameplayEffectContextNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcGameplayEffectContext);
	}

	void FArcGameplayEffectContextNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		// Setup serializer for base class GameplayEffectContext, this will generate a descriptor which will use registered custom serializer for GameplayEffectContext
		{
			const UStruct* GEStruct = FGameplayEffectContext::StaticStruct();
			StructNetSerializerConfigForGE.StateDescriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(GEStruct);
			const FReplicationStateDescriptor* Descriptor = StructNetSerializerConfigForGE.StateDescriptor.GetReference();
			check(Descriptor != nullptr);
			GEStateTraits = Descriptor->Traits;
 
			// Validate our assumptions regarding quantized state size and alignment.
			constexpr SIZE_T OffsetOfGameplayEffectContext = offsetof(FQuantizedType, GameplayEffectContext);
			if ((sizeof(FQuantizedType::GameplayEffectContext) < Descriptor->InternalSize) || (((OffsetOfGameplayEffectContext/Descriptor->InternalAlignment)*Descriptor->InternalAlignment) != OffsetOfGameplayEffectContext))
			{
				LowLevelFatalError(TEXT("FQuantizedType::GameplayEffectContext has size %u but requires size %u and alignment %u."), uint32(sizeof(FQuantizedType::GameplayEffectContext)), uint32(Descriptor->InternalSize), uint32(Descriptor->InternalAlignment));
			}
		}
	}
};
