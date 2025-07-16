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

#include "ArcItemDataInternalNetSerializer.h"

#include "ArcItemDataNetSerializer.h"
#include "ArcItemIdNetSerializer.h"
#include "ArcItemInstanceNetSerializer.h"
#include "ArcItemSpecNetSerializer.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"

namespace Arcx::Net
{
	using namespace UE::Net;

	struct FItemDataPtrAccessorForNetSerializer
	{
	public:
		static TSharedPtr<FArcItemData>& GetItem(FArcItemDataInternal& Source)
		{
			return Source.GetItem();
		}
	};
	
	struct FArcItemDataInternalNetSerializer
	{
	public:
		// Version
		static const uint32 Version = 0;
		static constexpr bool bUseDefaultDelta = true;
		// static void InitTypeCache();
		//  Traits
		static constexpr bool bIsForwardingSerializer = true; // Triggers asserts if a function is missing
		static constexpr bool bHasDynamicState = true;
		static constexpr bool bHasCustomNetReference = true;
		
		//using InternalNetSerializerType = UE::Net::TPolymorphicStructNetSerializerImpl<
		//		FArcItemDataInternal, FArcItemData, FItemDataPtrAccessorForNetSerializer::GetItem>;
		
		using ConfigType = FArcItemDataInternalNetSerializerConfig;

		struct FQuantizedType
		{
			FArcItemDataPtrNetSerializer::QuantizedType Item;
			FArcItemDataPtrNetSerializer::QuantizedType ItemData;
			//FArcItemSpecPtrNetSerializer::QuantizedType Spec;
			FArcItemIdNetSerializer::QuantizedType ItemId;

			alignas(16) uint8 ItemInstancesBuffer[1024];
		};

		static const ConfigType DefaultConfig;
		inline static const FArcItemDataPtrNetSerializer::InternalNetSerializerType::ConfigType ItemDataConfig;
		using QuantizedType = FQuantizedType;
		using SourceType = FArcItemDataInternal;

		class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			virtual ~FNetSerializerRegistryDelegates() override;

		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;

			virtual void OnPostFreezeNetSerializerRegistry() override;
		};

		static bool bIsPostFreezeCalled;
		static FArcItemDataInternalNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;

		static const UE::Net::FNetSerializer* ItemEntrySer;
		static const UE::Net::FNetSerializer* ItemSpecSer;
		static const UE::Net::FNetSerializer* ItemIdSerializer;
		
		inline static FStructNetSerializerConfig StructNetSerializerConfig;
		static const UE::Net::FNetSerializer* StructSerializer;
		static void Serialize(UE::Net::FNetSerializationContext& Context
							  , const UE::Net::FNetSerializeArgs& Args)
		{
			const FQuantizedType& SourceValue = *reinterpret_cast<const FQuantizedType*>(Args.Source);
			{
				FNetSerializeArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemId);
				InternalArgs.NetSerializerConfig = &FArcItemIdNetSerializer::DefaultConfig;
				ItemIdSerializer->Serialize(Context
					, InternalArgs);
			}
			{
				FNetSerializeArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemData);
				InternalArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;
				ItemEntrySer->Serialize(Context
					, InternalArgs);
			}
			//{
			//	FNetSerializeArgs InternalArgs = Args;
			//	InternalArgs.Source = NetSerializerValuePointer(&SourceValue.Spec);
			//	InternalArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
			//	ItemSpecSer->Serialize(Context
			//		, InternalArgs);
			//}
			{
				FNetSerializeArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemInstancesBuffer);
				InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
				StructSerializer->Serialize(Context
					, InternalArgs);
			}
			//{
			//	FNetSerializeArgs InternalArgs = Args;
			//	InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemInstances);
			//	InternalArgs.NetSerializerConfig = &FArcItemInstanceArrayNetSerializer::DefaultConfig;
			//	FArcItemInstanceArrayNetSerializer::InternalNetSerializerType::Serialize(Context
			//		, InternalArgs);
			//}
		}

		static void Deserialize(UE::Net::FNetSerializationContext& Context
								, const UE::Net::FNetDeserializeArgs& Args)
		{
			UE::Net::FNetBitStreamReader* Reader = Context.GetBitStreamReader();

			const FQuantizedType& TargetValue = *reinterpret_cast<const FQuantizedType*>(Args.Target);
			{
				FNetDeserializeArgs InternalArgs = Args;
				InternalArgs.Target = NetSerializerValuePointer(&TargetValue.ItemId);
				InternalArgs.NetSerializerConfig = &FArcItemIdNetSerializer::DefaultConfig;
				ItemIdSerializer->Deserialize(Context
					, InternalArgs);
			}
			{
				FNetDeserializeArgs InternalArgs = Args;
				InternalArgs.Target = NetSerializerValuePointer(&TargetValue.ItemData);
				InternalArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;;
				ItemEntrySer->Deserialize(Context
					, InternalArgs);
			}
			//{
			//	FNetDeserializeArgs InternalArgs = Args;
			//	InternalArgs.Target = NetSerializerValuePointer(&TargetValue.Spec);
			//	InternalArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
			//	ItemSpecSer->Deserialize(Context
			//		, InternalArgs);
			//}
			{
				FNetDeserializeArgs InternalArgs = Args;
				InternalArgs.Target = NetSerializerValuePointer(&TargetValue.ItemInstancesBuffer);
				InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
				StructSerializer->Deserialize(Context
					, InternalArgs);
			}
		};

		static void SerializeDelta(UE::Net::FNetSerializationContext& Context
								   , const UE::Net::FNetSerializeDeltaArgs& Args)
		{
			FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();
			const FQuantizedType& SourceValue = *reinterpret_cast<const FQuantizedType*>(Args.Source);
			const FQuantizedType& PrevValue = *reinterpret_cast<const FQuantizedType*>(Args.Prev);
			{
				bool bEqual = false;
				{
					FNetIsEqualArgs MemberArgs;
					MemberArgs.Version = 0;
					MemberArgs.bStateIsQuantized = true;
					MemberArgs.Source0 = NetSerializerValuePointer(&SourceValue.ItemId);
					MemberArgs.Source1 = NetSerializerValuePointer(&PrevValue.ItemId);
					MemberArgs.NetSerializerConfig = &FArcItemIdNetSerializer::DefaultConfig;
					
					bEqual = ItemIdSerializer->IsEqual(Context, MemberArgs);
					if(bEqual)
					{
						Writer->WriteBool(true);
					}
					else
					{
						Writer->WriteBool(false);
					}
				}
				if (bEqual == false)
				{
					FNetSerializeDeltaArgs InternalArgs = Args;
					InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemId);
					InternalArgs.Prev = NetSerializerValuePointer(&PrevValue.ItemId);
					InternalArgs.NetSerializerConfig = &FArcItemIdNetSerializer::DefaultConfig;
					ItemIdSerializer->SerializeDelta(Context
						, InternalArgs);
				}
			}
			{
				bool bEqual = false;
				{
					FNetIsEqualArgs MemberArgs;
					MemberArgs.Version = 0;
					MemberArgs.bStateIsQuantized = true;
					MemberArgs.Source0 = NetSerializerValuePointer(&SourceValue.ItemData);
					MemberArgs.Source1 = NetSerializerValuePointer(&PrevValue.ItemData);
					MemberArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;;
					bEqual = ItemEntrySer->IsEqual(Context, MemberArgs);
					if(bEqual)
					{
						Writer->WriteBool(true);
					}
					else
					{
						Writer->WriteBool(false);
					}
				}
				if (bEqual == false)
				{
					FNetSerializeDeltaArgs InternalArgs = Args;
					InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemData);
					InternalArgs.Prev = NetSerializerValuePointer(&PrevValue.ItemData);
					InternalArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;;
					ItemEntrySer->SerializeDelta(Context
						, InternalArgs);
				}
			}
		//{
		//	bool bEqual = false;
		//	{
		//		FNetIsEqualArgs MemberArgs;
		//		MemberArgs.Version = 0;
		//		MemberArgs.bStateIsQuantized = true;
		//		MemberArgs.Source0 = NetSerializerValuePointer(&SourceValue.Spec);
		//		MemberArgs.Source1 = NetSerializerValuePointer(&PrevValue.Spec);
		//		MemberArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
		//		bEqual = ItemSpecSer->IsEqual(Context, MemberArgs);
		//		if(bEqual)
		//		{
		//			Writer->WriteBool(true);
		//		}
		//		else
		//		{
		//			Writer->WriteBool(false);
		//		}
		//	}
		//	if (bEqual == false)
		//	{
		//		FNetSerializeDeltaArgs InternalArgs = Args;
		//		InternalArgs.Source = NetSerializerValuePointer(&SourceValue.Spec);
		//		InternalArgs.Prev = NetSerializerValuePointer(&PrevValue.Spec);
		//		InternalArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
		//		ItemSpecSer->SerializeDelta(Context
		//			, InternalArgs);
		//	}
		//}
			{
				FNetSerializeDeltaArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemInstancesBuffer);
				InternalArgs.Prev = NetSerializerValuePointer(&PrevValue.ItemInstancesBuffer);
				InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
				StructSerializer->SerializeDelta(Context
					, InternalArgs);
			}
		};

		static void DeserializeDelta(UE::Net::FNetSerializationContext& Context
									 , const UE::Net::FNetDeserializeDeltaArgs& Args)
		{
			FNetBitStreamReader* Reader = Context.GetBitStreamReader();
			const FQuantizedType& TargetValue = *reinterpret_cast<const FQuantizedType*>(Args.Target);
			const FQuantizedType& PrevValue = *reinterpret_cast<const FQuantizedType*>(Args.Prev);
			{
				bool bEqual = Reader->ReadBool();
				if (bEqual == false)
				{
					FNetDeserializeDeltaArgs InternalArgs = Args;
					InternalArgs.Target = NetSerializerValuePointer(&TargetValue.ItemId);
					InternalArgs.Prev = NetSerializerValuePointer(&PrevValue.ItemId);
					InternalArgs.NetSerializerConfig = &FArcItemIdNetSerializer::DefaultConfig;
					ItemIdSerializer->DeserializeDelta(Context
						, InternalArgs);
				}
			}
			{
				bool bEqual = Reader->ReadBool();
				if (bEqual == false)
				{
					FNetDeserializeDeltaArgs InternalArgs = Args;
					InternalArgs.Target = NetSerializerValuePointer(&TargetValue.ItemData);
					InternalArgs.Prev = NetSerializerValuePointer(&PrevValue.ItemData);
					InternalArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;;
					ItemEntrySer->DeserializeDelta(Context
						, InternalArgs);
				}
			}
			//{
			//	bool bEqual = Reader->ReadBool();
			//	if (bEqual == false)
			//	{
			//		FNetDeserializeDeltaArgs InternalArgs = Args;
			//		InternalArgs.Target = NetSerializerValuePointer(&TargetValue.Spec);
			//		InternalArgs.Prev = NetSerializerValuePointer(&PrevValue.Spec);
			//		InternalArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
			//		ItemSpecSer->DeserializeDelta(Context
			//			, InternalArgs);
			//	}
			//}
			{
				FNetDeserializeDeltaArgs InternalArgs = Args;
				InternalArgs.Target = NetSerializerValuePointer(&TargetValue.ItemInstancesBuffer);
				InternalArgs.Prev = NetSerializerValuePointer(&PrevValue.ItemInstancesBuffer);
				InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
				StructSerializer->DeserializeDelta(Context
					, InternalArgs);
			}
		};

		static void Quantize(UE::Net::FNetSerializationContext& Context
							 , const UE::Net::FNetQuantizeArgs& Args)
		{
			QuantizedType* TargetValue = reinterpret_cast<QuantizedType*>(Args.Target);
			SourceType* SourceValue = reinterpret_cast<SourceType*>(Args.Source);
			{
				FNetQuantizeArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue->ItemId);
				InternalArgs.Target = NetSerializerValuePointer(&TargetValue->ItemId);
				InternalArgs.NetSerializerConfig = &FArcItemIdNetSerializer::DefaultConfig;
				ItemIdSerializer->Quantize(Context
					, InternalArgs);
			}
			{
				FNetQuantizeArgs InternalArgs = Args;
				InternalArgs.Target = NetSerializerValuePointer(&TargetValue->ItemData);
				InternalArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;;
				ItemEntrySer->Quantize(Context
					, InternalArgs);
			}
			//{
			//	FNetQuantizeArgs InternalArgs = Args;
			//	InternalArgs.Target = NetSerializerValuePointer(&TargetValue->Spec);
			//	InternalArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
			//	ItemSpecSer->Quantize(Context
			//		, InternalArgs);
			//}
			//{
			//	FNetQuantizeArgs InternalArgs = Args;
			//	InternalArgs.Source = NetSerializerValuePointer(&SourceValue->ItemInstances);
			//	InternalArgs.Target = NetSerializerValuePointer(&TargetValue->ItemInstancesBuffer);
			//	InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
			//	StructSerializer->Quantize(Context
			//		, InternalArgs);
			//}
		};

		static void Dequantize(UE::Net::FNetSerializationContext& Context
							   , const UE::Net::FNetDequantizeArgs& Args)
		{
			QuantizedType* SourceValue = reinterpret_cast<QuantizedType*>(Args.Source);
			SourceType* TargetValue = reinterpret_cast<SourceType*>(Args.Target);

			
			// TODO: Skip Dequanitize/Quanitnize if states are equal (we don't delta serialize, might as well skip this).
			{
				FNetDequantizeArgs InternalArgs = Args;
				InternalArgs.Target = NetSerializerValuePointer(&TargetValue->ItemId);
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue->ItemId);
				InternalArgs.NetSerializerConfig = &FArcItemIdNetSerializer::DefaultConfig;
				ItemIdSerializer->Dequantize(Context
					, InternalArgs);
			}
			{
				FNetDequantizeArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue->ItemData);
				InternalArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;;
				ItemEntrySer->Dequantize(Context
					, InternalArgs);
			}
			//{
			//	FNetDequantizeArgs InternalArgs = Args;
			//	InternalArgs.Source = NetSerializerValuePointer(&SourceValue->Spec);
			//	InternalArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
			//	ItemSpecSer->Dequantize(Context
			//		, InternalArgs);
			//}
			//{
			//	FNetDequantizeArgs InternalArgs = Args;
			//	InternalArgs.Target = NetSerializerValuePointer(&TargetValue->ItemInstances);
			//	InternalArgs.Source = NetSerializerValuePointer(&SourceValue->ItemInstancesBuffer);
			//	InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
			//	StructSerializer->Dequantize(Context
			//		, InternalArgs);
			//}
		};

		static bool IsEqual(UE::Net::FNetSerializationContext& Context
							, const UE::Net::FNetIsEqualArgs& Args)
		{
			bool bValidA = false;
			bool bValidB = false;
			bool bValidC = false;
			const FQuantizedType& SourceValue0 = *reinterpret_cast<const FQuantizedType*>(Args.Source0);
			const FQuantizedType& SourceValue1 = *reinterpret_cast<const FQuantizedType*>(Args.Source1);
			{
				FNetIsEqualArgs InternalArgs = Args;
				InternalArgs.Source0 = NetSerializerValuePointer(&SourceValue0.ItemData);
				InternalArgs.Source1 = NetSerializerValuePointer(&SourceValue1.ItemData);
				InternalArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;;
				bValidA = ItemEntrySer->IsEqual(Context
					, InternalArgs);
			}
			//{
			//	FNetIsEqualArgs InternalArgs = Args;
			//	InternalArgs.Source0 = NetSerializerValuePointer(&SourceValue0.Spec);
			//	InternalArgs.Source1 = NetSerializerValuePointer(&SourceValue1.Spec);
			//	InternalArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
			//	bValidB = ItemSpecSer->IsEqual(Context
			//		, InternalArgs);
			//}
			{
				FNetIsEqualArgs InternalArgs = Args;
				InternalArgs.Source0 = NetSerializerValuePointer(&SourceValue0.ItemInstancesBuffer);
				InternalArgs.Source1 = NetSerializerValuePointer(&SourceValue1.ItemInstancesBuffer);
				InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
				bValidC = StructSerializer->IsEqual(Context
					, InternalArgs);
			}
			return bValidA && bValidB && bValidC;
		};

		static bool Validate(UE::Net::FNetSerializationContext& Context
							 , const UE::Net::FNetValidateArgs& Args)
		{
			bool bValidA = false;
			bool bValidB = false;
			bool bValidC = false;
			const FQuantizedType& SourceValue = *reinterpret_cast<const FQuantizedType*>(Args.Source);
			{
				FNetValidateArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemData);
				InternalArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;;
				bValidA = ItemEntrySer->Validate(Context
					, InternalArgs);
			}
			//{
			//	FNetValidateArgs InternalArgs = Args;
			//	InternalArgs.Source = NetSerializerValuePointer(&SourceValue.Spec);
			//	InternalArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
			//	bValidB = ItemSpecSer->Validate(Context
			//		, InternalArgs);
			//}
			{
				FNetValidateArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemInstancesBuffer);
				InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
				bValidC = StructSerializer->Validate(Context
					, InternalArgs);
			}
			return bValidA && bValidB && bValidC;
		};

		static void CloneDynamicState(UE::Net::FNetSerializationContext& Context
									  , const UE::Net::FNetCloneDynamicStateArgs& Args)
		{
			const FQuantizedType& SourceValue = *reinterpret_cast<const FQuantizedType*>(Args.Source);
			FQuantizedType& TargetValue = *reinterpret_cast<FQuantizedType*>(Args.Target);

			{
				FNetCloneDynamicStateArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemData);
				InternalArgs.Target = NetSerializerValuePointer(&TargetValue.ItemData);
				InternalArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;;
				ItemEntrySer->CloneDynamicState(Context
					, InternalArgs);
			}
			//{
			//	FNetCloneDynamicStateArgs InternalArgs = Args;
			//	InternalArgs.Source = NetSerializerValuePointer(&SourceValue.Spec);
			//	InternalArgs.Target = NetSerializerValuePointer(&TargetValue.Spec);
			//	InternalArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
			//	ItemSpecSer->CloneDynamicState(Context
			//		, InternalArgs);
			//}
			{
				FNetCloneDynamicStateArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemInstancesBuffer);
				InternalArgs.Target = NetSerializerValuePointer(&TargetValue.ItemInstancesBuffer);
				InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
				StructSerializer->CloneDynamicState(Context
					, InternalArgs);
			}
		};

		static void FreeDynamicState(UE::Net::FNetSerializationContext& Context
									 , const UE::Net::FNetFreeDynamicStateArgs& Args)
		{
			const FQuantizedType& SourceValue = *reinterpret_cast<const FQuantizedType*>(Args.Source);
			{
				FNetFreeDynamicStateArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemData);
				InternalArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;;
				ItemEntrySer->FreeDynamicState(Context
					, InternalArgs);
			}
			//{
			//	FNetFreeDynamicStateArgs InternalArgs = Args;
			//	InternalArgs.Source = NetSerializerValuePointer(&SourceValue.Spec);
			//	InternalArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
			//	ItemSpecSer->FreeDynamicState(Context
			//		, InternalArgs);
			//}
			{
				FNetFreeDynamicStateArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemInstancesBuffer);
				InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
				StructSerializer->FreeDynamicState(Context
					, InternalArgs);
			}
		};

		static void CollectNetReferences(UE::Net::FNetSerializationContext& Context
										 , const UE::Net::FNetCollectReferencesArgs& Args)
		{
			const FQuantizedType& SourceValue = *reinterpret_cast<const FQuantizedType*>(Args.Source);
			{
				FNetCollectReferencesArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemData);
				InternalArgs.NetSerializerConfig = &FArcItemDataPtrNetSerializer::DefaultConfig;;
				ItemEntrySer->CollectNetReferences(Context
					, InternalArgs);
			}
			//{
			//	FNetCollectReferencesArgs InternalArgs = Args;
			//	InternalArgs.Source = NetSerializerValuePointer(&SourceValue.Spec);
			//	InternalArgs.NetSerializerConfig = &FArcItemSpecPtrNetSerializer::DefaultConfig;
			//	ItemSpecSer->CollectNetReferences(Context
			//		, InternalArgs);
			//}
			{
				FNetCollectReferencesArgs InternalArgs = Args;
				InternalArgs.Source = NetSerializerValuePointer(&SourceValue.ItemInstancesBuffer);
				InternalArgs.NetSerializerConfig = &StructNetSerializerConfig;
				StructSerializer->CollectNetReferences(Context
					, InternalArgs);
			}
		};
	};

	UE_NET_IMPLEMENT_SERIALIZER(FArcItemDataInternalNetSerializer);

	const UE::Net::FNetSerializer* FArcItemDataInternalNetSerializer::ItemEntrySer = &UE_NET_GET_SERIALIZER(
		FArcItemDataPtrNetSerializer);
	//const UE::Net::FNetSerializer* FArcItemDataInternalNetSerializer::ItemSpecSer = &UE_NET_GET_SERIALIZER(
	//	FArcItemSpecPtrNetSerializer);
	const UE::Net::FNetSerializer* FArcItemDataInternalNetSerializer::ItemIdSerializer = &UE_NET_GET_SERIALIZER(
		FArcItemIdNetSerializer);

	const UE::Net::FNetSerializer* FArcItemDataInternalNetSerializer::StructSerializer = &UE_NET_GET_SERIALIZER(
		FStructNetSerializer);
	
	const FArcItemDataInternalNetSerializer::ConfigType FArcItemDataInternalNetSerializer::DefaultConfig =
			FArcItemDataInternalNetSerializer::ConfigType();
	//const FArcItemDataInternalNetSerializer::FArcItemDataPtrNetSerializer::InternalNetSerializerType::ConfigType FArcItemDataInternalNetSerializer::ItemDataConfig =
	//	FArcItemDataInternalNetSerializer::FArcItemDataPtrNetSerializer::InternalNetSerializerType::ConfigType();
	
	FArcItemDataInternalNetSerializer::FNetSerializerRegistryDelegates
	
	FArcItemDataInternalNetSerializer::NetSerializerRegistryDelegates;
	bool FArcItemDataInternalNetSerializer::bIsPostFreezeCalled = false;

	static const FName PropertyNetSerializerRegistry_NAME_ArcItemEntryInternal("ArcItemDataInternal");
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemEntryInternal
		, FArcItemDataInternalNetSerializer);

	
	void InitArcItemDataInternalNetSerializerTypeCache()
	{
		//Arcx::Net::FArcItemDataInternalNetSerializer::bIsPostFreezeCalled = true;
		//Arcx::Net::FArcItemDataInternalNetSerializer::InitTypeCache();
	}
	
	FArcItemDataInternalNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemEntryInternal);
	}

	void FArcItemDataInternalNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemEntryInternal);
	}

	void FArcItemDataInternalNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		const UStruct* ReplicationProxyStruct = FArcItemInstanceArray::StaticStruct();
		FReplicationStateDescriptorBuilder::FParameters Params;
		StructNetSerializerConfig.StateDescriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(ReplicationProxyStruct, Params);
		
		//InitArcItemDataInternalNetSerializerTypeCache();
	}
}
