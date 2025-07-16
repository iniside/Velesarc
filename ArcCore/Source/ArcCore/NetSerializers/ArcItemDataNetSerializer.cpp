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

#include "ArcItemDataNetSerializer.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/Serialization/InternalNetSerializers.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"

#include "Iris/ReplicationSystem/ReplicationOperationsInternal.h"

namespace Arcx::Net
{
	UE_NET_IMPLEMENT_SERIALIZER(FArcItemDataPtrNetSerializer);

	
	
	//void FArcItemDataPtrNetSerializer::InitTypeCache()
	//{
	//	// When post freeze is called we expect all custom serializers to have been
	//	// registered so that the type cache will get the appropriate serializer when
	//	// creating the ReplicationStateDescriptor.
	//	if (bIsPostFreezeCalled)
	//	{
	//		InternalNetSerializerType::InitTypeCache<FArcItemDataPtrNetSerializer>();
	//	}
	//}
	
	const FArcItemDataPtrNetSerializer::ConfigType FArcItemDataPtrNetSerializer::DefaultConfig =
			FArcItemDataPtrNetSerializer::ConfigType();
	
	FArcItemDataPtrNetSerializer::FNetSerializerRegistryDelegates
	FArcItemDataPtrNetSerializer::NetSerializerRegistryDelegates;
	bool FArcItemDataPtrNetSerializer::bIsPostFreezeCalled = false;

	//static const FName PropertyNetSerializerRegistry_NAME_ArcItemData("ArcItemData");
	//UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemData
	//	, FArcItemDataPtrNetSerializer);
	
	void InitItemDataNetSerializerTypeCache()
	{
		
	}
	
	FArcItemDataPtrNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		//UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemData);
	}
	
	void FArcItemDataPtrNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		//UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemData);
	}
	
	void FArcItemDataPtrNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		Arcx::Net::FArcItemDataPtrNetSerializer::bIsPostFreezeCalled = true;
		InternalNetSerializerType::InitTypeCache<FArcItemDataPtrNetSerializer>();
		//InitItemDataNetSerializerTypeCache();
	}
}


/*
namespace Arcx::Net
{
	// using namespace UE::Net;

	UE_NET_IMPLEMENT_SERIALIZER(FArcItemDataPtrNetSerializer);

	void FArcItemDataPtrNetSerializer::InitTypeCache()
	{
		// When post freeze is called we expect all custom serializers to have been
		// registered so that the type cache will get the appropriate serializer when
		// creating the ReplicationStateDescriptor.
		if (bIsPostFreezeCalled)
		{
			InternalNetSerializerType::InitTypeCache<FArcItemDataPtrNetSerializer>();
		}
	}

	const FArcItemDataPtrNetSerializer::ConfigType FArcItemDataPtrNetSerializer::DefaultConfig =
			FArcItemDataPtrNetSerializer::ConfigType();

	FArcItemDataPtrNetSerializer::FNetSerializerRegistryDelegates
	FArcItemDataPtrNetSerializer::NetSerializerRegistryDelegates;
	bool FArcItemDataPtrNetSerializer::bIsPostFreezeCalled = false;

	// static const FName
	// PropertyNetSerializerRegistry_NAME_ArcItemEntryInternal("ArcItemEntryInternal");
	// UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemEntryInternal,
	// FArcItemDataInternalNetSerializer);

	void InitArcItemEntryPtrNetSerializerTypeCache()
	{
		Arcx::Net::FArcItemDataPtrNetSerializer::bIsPostFreezeCalled = true;
		Arcx::Net::FArcItemDataPtrNetSerializer::InitTypeCache();
	}

	FArcItemDataPtrNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
	}

	void FArcItemDataPtrNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
	}

	void FArcItemDataPtrNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		InitArcItemEntryPtrNetSerializerTypeCache();
	}

	struct FArcItemDataNetSerializer
	{
		// Version
		static const uint32 Version = 0;

		// Traits
		static constexpr bool bIsForwardingSerializer = true; // Triggers asserts if a function is missing
		static constexpr bool bHasDynamicState = true;
		static constexpr bool bHasCustomNetReference = true;
		static constexpr bool bUseDefaultDelta = false;

		// Types
		struct FQuantizedType
		{
			alignas(16) uint8 QuantizedStruct[320];
		};

		using QuantizedType = FQuantizedType;
		using ConfigType = FArcItemDataSerializerConfig;

		static ConfigType DefaultConfig;
		using SourceType = FArcItemData;

		// just for testing, I'm going to manually bitpack the FArcItemData
		static void Serialize(FNetSerializationContext& Context
							  , const FNetSerializeArgs& Args)
		{
			QuantizedType* Val = reinterpret_cast<QuantizedType*>(Args.Source);
			const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
			
			{
				FNetSerializeArgs MemberArgs;
				MemberArgs.Version = 0;
				MemberArgs.NetSerializerConfig = &StructNetSerializerConfig;
				MemberArgs.Source = NetSerializerValuePointer(Val);
				StructNetSerializer->Serialize(Context, MemberArgs);
				return;
			}
			
			const FReplicationStateDescriptor* Descriptor = Config->StateDescriptor;
			const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
			const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->
					MemberSerializerDescriptors;
			const FReplicationStateMemberDebugDescriptor* MemberDebugDescriptors = Descriptor->MemberDebugDescriptors;

			const uint32 MemberCount = Descriptor->MemberCount;

			for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
			{
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
				const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor =
						MemberSerializerDescriptors[MemberIt];
				const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;

				FNetSerializeArgs MemberArgs;
				MemberArgs.Version = 0;
				MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
				MemberArgs.Source = NetSerializerValuePointer(Val->QuantizedStruct) + MemberDescriptor.
									InternalMemberOffset;
				// Currently we pass on the changemask info unmodified to support
				// fastarrays, but if we decide to support other serializers utilizing
				// additional changemask we need to extend this
				MemberArgs.ChangeMaskInfo = Args.ChangeMaskInfo;

				// UE_NET_TRACE_DYNAMIC_NAME_SCOPE(MemberDebugDescriptors[MemberIt].DebugName,
				// *Context.GetBitStreamWriter(), Context.GetTraceCollector(),
				// ENetTraceVerbosity::Verbose);
				// UE_NET_TRACE_DYNAMIC_NAME_SCOPE(Serializer->Name,
				// *Context.GetBitStreamWriter(), Context.GetTraceCollector(),
				// ENetTraceVerbosity::VeryVerbose);

				Serializer->Serialize(Context
					, MemberArgs);
			}

			//{
			//	InternalArgs.Source = NetSerializerValuePointer(&Val->Id);
			//	const FNetSerializer* GuidSerializer =
			//&UE_NET_GET_SERIALIZER(FGuidNetSerializer);
			//	GuidSerializer->Serialize(Context, InternalArgs);
			//}
		};

		static void Deserialize(FNetSerializationContext& Context
								, const FNetDeserializeArgs& Args)
		{
			QuantizedType* Val = reinterpret_cast<QuantizedType*>(Args.Target);
			{
				FNetDeserializeArgs MemberArgs;
				MemberArgs.Version = 0;
				MemberArgs.NetSerializerConfig = &StructNetSerializerConfig;
				MemberArgs.Target = NetSerializerValuePointer(Val);
				StructNetSerializer->Deserialize(Context, MemberArgs);
				return;
			}
			const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
			const FReplicationStateDescriptor* Descriptor = Config->StateDescriptor;
			const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
			const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->
					MemberSerializerDescriptors;
			const FReplicationStateMemberDebugDescriptor* MemberDebugDescriptors = Descriptor->MemberDebugDescriptors;
			const uint32 MemberCount = Descriptor->MemberCount;

			for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
			{
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
				const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor =
						MemberSerializerDescriptors[MemberIt];
				const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;

				FNetDeserializeArgs MemberArgs;
				MemberArgs.Version = 0;
				MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
				MemberArgs.Target = NetSerializerValuePointer(Val->QuantizedStruct) + MemberDescriptor.
									InternalMemberOffset;
				// Currently we pass on the changemask info unmodified to support
				// fastarrays, but if we decide to support other serializers utilizing
				// additional changemask we need to extend this
				MemberArgs.ChangeMaskInfo = Args.ChangeMaskInfo;

				// UE_NET_TRACE_DYNAMIC_NAME_SCOPE(MemberDebugDescriptors[MemberIt].DebugName,
				// *Context.GetBitStreamReader(), Context.GetTraceCollector(),
				// ENetTraceVerbosity::Verbose);
				// UE_NET_TRACE_DYNAMIC_NAME_SCOPE(Serializer->Name,
				// *Context.GetBitStreamReader(), Context.GetTraceCollector(),
				// ENetTraceVerbosity::VeryVerbose);

				Serializer->Deserialize(Context
					, MemberArgs);
				if (Context.HasErrorOrOverflow())
				{
					// $IRIS TODO Provide information which member is failing to
					// deserialize (though could be red herring)
					return;
				}
			}
		};

		static void SerializeDelta(FNetSerializationContext& Context
								   , const FNetSerializeDeltaArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
			QuantizedType* Prev = reinterpret_cast<QuantizedType*>(Args.Prev);
			{
				FNetSerializeDeltaArgs MemberArgs;
				MemberArgs.Version = 0;
				MemberArgs.NetSerializerConfig = &StructNetSerializerConfig;
				MemberArgs.Prev = NetSerializerValuePointer(Prev);
				MemberArgs.Source = NetSerializerValuePointer(Source);
				StructNetSerializer->SerializeDelta(Context, MemberArgs);
				return;
			}
			FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

			bool bError = Context.HasErrorOrOverflow();
			const FNetBitArrayView* Mask = Context.GetChangeMask();

			const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
			const FReplicationStateDescriptor* Descriptor = Config->StateDescriptor;
			const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
			const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->
					MemberSerializerDescriptors;
			const FReplicationStateMemberDebugDescriptor* MemberDebugDescriptors = Descriptor->MemberDebugDescriptors;

			const uint32 MemberCount = Descriptor->MemberCount;

			for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
			{
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
				const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor =
						MemberSerializerDescriptors[MemberIt];
				const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;

				// UE_NET_TRACE_DYNAMIC_NAME_SCOPE(MemberDebugDescriptors[MemberIt].DebugName,
				// *Context.GetBitStreamWriter(), Context.GetTraceCollector(),
				// ENetTraceVerbosity::Verbose);
				// UE_NET_TRACE_DYNAMIC_NAME_SCOPE(Serializer->Name,
				// *Context.GetBitStreamWriter(), Context.GetTraceCollector(),
				// ENetTraceVerbosity::VeryVerbose);

				if (Serializer == &UE_NET_GET_SERIALIZER(FStructNetSerializer))
				{
					FNetIsEqualArgs MemberEqualArgs;
					MemberEqualArgs.bStateIsQuantized = true;
					MemberEqualArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
					MemberEqualArgs.Source0 = NetSerializerValuePointer(Source->QuantizedStruct) + MemberDescriptor.
											  InternalMemberOffset;
					MemberEqualArgs.Source1 = NetSerializerValuePointer(Prev->QuantizedStruct) + MemberDescriptor.
											  InternalMemberOffset;
					MemberEqualArgs.ChangeMaskInfo = Args.ChangeMaskInfo;
					bool bEqual = IsEqual(Context
						, MemberEqualArgs);
					if (Writer->WriteBool(bEqual))
					{
						continue;
					}
				}

				{
					FNetSerializeDeltaArgs MemberArgs;
					MemberArgs.Version = 0;
					MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
					MemberArgs.Source = NetSerializerValuePointer(Source->QuantizedStruct) + MemberDescriptor.
										InternalMemberOffset;
					MemberArgs.Prev = NetSerializerValuePointer(Prev->QuantizedStruct) + MemberDescriptor.
									  InternalMemberOffset;
					// Currently we pass on the changemask info unmodified to support
					// fastarrays, but if we decide to support other serializers utilizing
					// additional changemask we need to extend this
					MemberArgs.ChangeMaskInfo = Args.ChangeMaskInfo;

					Serializer->SerializeDelta(Context
						, MemberArgs);
				}
			}

			if (Args.Source)
			{
			}
		};

		static void CloneStructMember(FNetSerializationContext& Context
									  , const FNetCloneDynamicStateArgs& CloneArgs)
		{
			{
				FNetFreeDynamicStateArgs MemberArgs;
				MemberArgs.Version = 0;
				MemberArgs.NetSerializerConfig = &StructNetSerializerConfig;
				MemberArgs.Prev = NetSerializerValuePointer(Prev);
				MemberArgs.Source = NetSerializerValuePointer(Source);
				StructNetSerializer->CloneStructMember(Context, MemberArgs);
				return;
			}
			
			const FReplicationStateDescriptor* StateDescriptor = static_cast<const ConfigType*>(CloneArgs.
				NetSerializerConfig)->StateDescriptor;
			const bool bNeedFreeingAndCloning = EnumHasAnyFlags(StateDescriptor->Traits
				, EReplicationStateTraits::HasDynamicState);

			if (bNeedFreeingAndCloning)
			{
				FNetFreeDynamicStateArgs FreeArgs;
				FreeArgs.NetSerializerConfig = CloneArgs.NetSerializerConfig;
				FreeArgs.Version = CloneArgs.Version;
				FreeArgs.Source = CloneArgs.Target;
				FreeDynamicState(Context
					, FreeArgs);
			}

			FPlatformMemory::Memcpy(reinterpret_cast<void*>(CloneArgs.Target)
				, reinterpret_cast<const void*>(CloneArgs.Source)
				, StateDescriptor->InternalSize);

			if (bNeedFreeingAndCloning)
			{
				CloneDynamicState(Context
					, CloneArgs);
			}
		}

		static void DeserializeDelta(FNetSerializationContext& Context
									 , const FNetDeserializeDeltaArgs& Args)
		{
			QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);
			QuantizedType* Prev = reinterpret_cast<QuantizedType*>(Args.Prev);

			FNetBitStreamReader* Reader = Context.GetBitStreamReader();

			bool bError = Context.HasErrorOrOverflow();
			const FNetBitArrayView* Mask = Context.GetChangeMask();

			const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
			const FReplicationStateDescriptor* Descriptor = Config->StateDescriptor;
			const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
			const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->
					MemberSerializerDescriptors;
			const FReplicationStateMemberTraitsDescriptor* MemberTraitsDescriptors = Descriptor->
					MemberTraitsDescriptors;
			const FReplicationStateMemberDebugDescriptor* MemberDebugDescriptors = Descriptor->MemberDebugDescriptors;

			const uint32 MemberCount = Descriptor->MemberCount;

			for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
			{
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
				const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor =
						MemberSerializerDescriptors[MemberIt];
				const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;

				// UE_NET_TRACE_DYNAMIC_NAME_SCOPE(MemberDebugDescriptors[MemberIt].DebugName,
				// *Context.GetBitStreamReader(), Context.GetTraceCollector(),
				// ENetTraceVerbosity::Verbose);
				// UE_NET_TRACE_DYNAMIC_NAME_SCOPE(Serializer->Name,
				// *Context.GetBitStreamReader(), Context.GetTraceCollector(),
				// ENetTraceVerbosity::VeryVerbose);

				if (Serializer == &UE_NET_GET_SERIALIZER(FStructNetSerializer))
				{
					bool bEqual = Reader->ReadBool();
					if (bEqual)
					{
						FNetCloneDynamicStateArgs CloneArgs;
						CloneArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
						CloneArgs.Version = Args.Version;
						CloneArgs.Source = NetSerializerValuePointer(Prev->QuantizedStruct) + MemberDescriptor.
										   InternalMemberOffset;
						CloneArgs.Target = NetSerializerValuePointer(Target->QuantizedStruct) + MemberDescriptor.
										   InternalMemberOffset;
						CloneStructMember(Context
							, CloneArgs);
						continue;
					}
				}

				{
					FNetDeserializeDeltaArgs MemberArgs;
					MemberArgs.Version = 0;
					MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
					MemberArgs.Target = NetSerializerValuePointer(Target->QuantizedStruct) + MemberDescriptor.
										InternalMemberOffset;
					MemberArgs.Prev = NetSerializerValuePointer(Prev->QuantizedStruct) + MemberDescriptor.
									  InternalMemberOffset;
					// Currently we pass on the changemask info unmodified to support
					// fastarrays, but if we decide to support other serializers utilizing
					// additional changemask we need to extend this
					MemberArgs.ChangeMaskInfo = Args.ChangeMaskInfo;

					Serializer->DeserializeDelta(Context
						, MemberArgs);
					if (Context.HasErrorOrOverflow())
					{
						// $IRIS TODO Provide information which member is failing to
						// deserialize (though could be red herring)
						return;
					}
				}
			}

			if (Args.Prev)
			{
			}
		};

		static void Quantize(FNetSerializationContext& Context
							 , const FNetQuantizeArgs& Args)
		{
			SourceType* Source = reinterpret_cast<SourceType*>(Args.Source);
			QuantizedType* Target = reinterpret_cast<QuantizedType*>(Args.Target);

			const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
			const FReplicationStateDescriptor* Descriptor = Config->StateDescriptor;
			const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
			const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->
					MemberSerializerDescriptors;
			const FReplicationStateMemberDebugDescriptor* MemberDebugDescriptors = Descriptor->MemberDebugDescriptors;
			const uint32 MemberCount = Descriptor->MemberCount;

			for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
			{
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
				const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor =
						MemberSerializerDescriptors[MemberIt];
				const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;

				FNetQuantizeArgs MemberArgs;
				// MemberArgs = Args;
				MemberArgs.Version = 0;
				MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
				MemberArgs.Source = Args.Source + MemberDescriptor.ExternalMemberOffset;
				MemberArgs.Target = NetSerializerValuePointer(Target->QuantizedStruct) + MemberDescriptor.
									InternalMemberOffset;

				Serializer->Quantize(Context
					, MemberArgs);
			}

			if (Context.HasError())
			{
			}
		};

		static void Dequantize(FNetSerializationContext& Context
							   , const FNetDequantizeArgs& Args)
		{
			QuantizedType* Source = reinterpret_cast<QuantizedType*>(Args.Source);
			SourceType* Target = reinterpret_cast<SourceType*>(Args.Target);

			const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
			const FReplicationStateDescriptor* Descriptor = Config->StateDescriptor;
			const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
			const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->
					MemberSerializerDescriptors;
			const FReplicationStateMemberDebugDescriptor* MemberDebugDescriptors = Descriptor->MemberDebugDescriptors;
			const uint32 MemberCount = Descriptor->MemberCount;

			for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
			{
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
				const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor =
						MemberSerializerDescriptors[MemberIt];
				const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;

				FNetDequantizeArgs MemberArgs;
				// MemberArgs = Args;
				MemberArgs.Version = 0;
				MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
				MemberArgs.Source = NetSerializerValuePointer(Source->QuantizedStruct) + MemberDescriptor.
									InternalMemberOffset;
				MemberArgs.Target = Args.Target + MemberDescriptor.ExternalMemberOffset;
				Serializer->Dequantize(Context
					, MemberArgs);
			}

			if (Context.HasError())
			{
			}
		};

		static bool IsEqual(FNetSerializationContext& Context
							, const FNetIsEqualArgs& Args)
		{
			const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
			const FReplicationStateDescriptor* Descriptor = Config->StateDescriptor;

			// Optimized case for quantized state without dynamic state
			if (Args.bStateIsQuantized && !EnumHasAnyFlags(Descriptor->Traits
					, EReplicationStateTraits::HasDynamicState))
			{
				return !FPlatformMemory::Memcmp(reinterpret_cast<const void*>(Args.Source0)
					, reinterpret_cast<const void*>(Args.Source1)
					, Descriptor->InternalSize);
			}

			// Per member equality
			{
				const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
				const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->
						MemberSerializerDescriptors;
				const uint32 MemberCount = Descriptor->MemberCount;

				FNetIsEqualArgs MemberArgs;
				MemberArgs.Version = 0;
				MemberArgs.bStateIsQuantized = Args.bStateIsQuantized;

				const bool bIsQuantized = Args.bStateIsQuantized;
				for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
				{
					const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
					const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor =
							MemberSerializerDescriptors[MemberIt];
					const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;

					MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
					const uint32 MemberOffset = (bIsQuantized
												 ? MemberDescriptor.InternalMemberOffset
												 : MemberDescriptor.ExternalMemberOffset);
					MemberArgs.Source0 = Args.Source0 + MemberOffset;
					MemberArgs.Source1 = Args.Source1 + MemberOffset;

					if (!Serializer->IsEqual(Context
						, MemberArgs))
					{
						return false;
					}
				}
			}

			return true;
		};

		static bool Validate(FNetSerializationContext& Context
							 , const FNetValidateArgs& Args)
		{
			const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
			const FReplicationStateDescriptor* Descriptor = Config->StateDescriptor;
			if (Descriptor == nullptr)
			{
				return false;
			}

			const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
			const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->
					MemberSerializerDescriptors;
			const uint32 MemberCount = Descriptor->MemberCount;

			for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
			{
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];
				const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor =
						MemberSerializerDescriptors[MemberIt];
				const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;

				FNetValidateArgs MemberArgs;
				MemberArgs.Version = 0;
				MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
				MemberArgs.Source = Args.Source + MemberDescriptor.ExternalMemberOffset;

				if (!Serializer->Validate(Context
					, MemberArgs))
				{
					return false;
				}
			}

			return true;
		};

		static void CloneDynamicState(FNetSerializationContext& Context
									  , const FNetCloneDynamicStateArgs& Args)
		{
			const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
			const FReplicationStateDescriptor* Descriptor = Config->StateDescriptor;
			// If no member has dynamic state then there's nothing for us to do
			if (!EnumHasAnyFlags(Descriptor->Traits
				, EReplicationStateTraits::HasDynamicState))
			{
				return;
			}

			const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
			const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->
					MemberSerializerDescriptors;
			const FReplicationStateMemberTraitsDescriptor* MemberTraitsDescriptors = Descriptor->
					MemberTraitsDescriptors;
			const uint32 MemberCount = Descriptor->MemberCount;

			for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
			{
				const FReplicationStateMemberTraitsDescriptor& MemberTraitsDescriptor = MemberTraitsDescriptors[
					MemberIt];
				if (!EnumHasAnyFlags(MemberTraitsDescriptor.Traits
					, EReplicationStateMemberTraits::HasDynamicState))
				{
					continue;
				}

				const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor =
						MemberSerializerDescriptors[MemberIt];
				const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];

				FNetCloneDynamicStateArgs MemberArgs;
				MemberArgs.Version = 0;
				MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
				MemberArgs.Source = Args.Source + MemberDescriptor.InternalMemberOffset;
				MemberArgs.Target = Args.Target + MemberDescriptor.InternalMemberOffset;

				Serializer->CloneDynamicState(Context
					, MemberArgs);
			}
		};

		static void FreeDynamicState(FNetSerializationContext& Context
									 , const FNetFreeDynamicStateArgs& Args)
		{
			const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
			const FReplicationStateDescriptor* Descriptor = Config->StateDescriptor;
			// If no member has dynamic state then there's nothing for us to do
			if (!EnumHasAnyFlags(Descriptor->Traits
				, EReplicationStateTraits::HasDynamicState))
			{
				return;
			}

			const FReplicationStateMemberDescriptor* MemberDescriptors = Descriptor->MemberDescriptors;
			const FReplicationStateMemberSerializerDescriptor* MemberSerializerDescriptors = Descriptor->
					MemberSerializerDescriptors;
			const FReplicationStateMemberTraitsDescriptor* MemberTraitsDescriptors = Descriptor->
					MemberTraitsDescriptors;
			const uint32 MemberCount = Descriptor->MemberCount;

			for (uint32 MemberIt = 0; MemberIt < MemberCount; ++MemberIt)
			{
				const FReplicationStateMemberTraitsDescriptor& MemberTraitsDescriptor = MemberTraitsDescriptors[
					MemberIt];
				if (!EnumHasAnyFlags(MemberTraitsDescriptor.Traits
					, EReplicationStateMemberTraits::HasDynamicState))
				{
					continue;
				}

				const FReplicationStateMemberSerializerDescriptor& MemberSerializerDescriptor =
						MemberSerializerDescriptors[MemberIt];
				const FNetSerializer* Serializer = MemberSerializerDescriptor.Serializer;
				const FReplicationStateMemberDescriptor& MemberDescriptor = MemberDescriptors[MemberIt];

				FNetFreeDynamicStateArgs MemberArgs;
				MemberArgs.Version = 0;
				MemberArgs.NetSerializerConfig = MemberSerializerDescriptor.SerializerConfig;
				MemberArgs.Source = Args.Source + MemberDescriptor.InternalMemberOffset;

				Serializer->FreeDynamicState(Context
					, MemberArgs);
			}
		};

		static void CollectNetReferences(FNetSerializationContext& Context
										 , const FNetCollectReferencesArgs& Args)
		{
			// User implemented forwarding serializers don't have access to
			// ReplicationStateOperationsInternal
			const ConfigType* Config = static_cast<const ConfigType*>(Args.NetSerializerConfig);
			const FReplicationStateDescriptor* Descriptor = Config->StateDescriptor;
			UE::Net::Private::FReplicationStateOperationsInternal::CollectReferences(Context
				, *reinterpret_cast<FNetReferenceCollector*>(Args.Collector)
				, Args.ChangeMaskInfo
				, reinterpret_cast<const uint8*>(Args.Source)
				, Descriptor);
		};

		class FNetSerializerRegistryDelegates final : private UE::Net::FNetSerializerRegistryDelegates
		{
		public:
			virtual ~FNetSerializerRegistryDelegates() override;

		private:
			virtual void OnPreFreezeNetSerializerRegistry() override;

			virtual void OnPostFreezeNetSerializerRegistry() override;
		};

		static FArcItemDataNetSerializer::FNetSerializerRegistryDelegates NetSerializerRegistryDelegates;
		static FStructNetSerializerConfig StructNetSerializerConfig;
		static const FNetSerializer* StructNetSerializer;
		static const FNetSerializer* ArrayNetSerializer;
	};

	UE_NET_IMPLEMENT_SERIALIZER(FArcItemDataNetSerializer);
	FArcItemDataNetSerializer::ConfigType FArcItemDataNetSerializer::DefaultConfig =
			FArcItemDataNetSerializer::ConfigType();
	const FNetSerializer* FArcItemDataNetSerializer::StructNetSerializer = &
			UE_NET_GET_SERIALIZER(FStructNetSerializer);
	const FNetSerializer* FArcItemDataNetSerializer::ArrayNetSerializer = &UE_NET_GET_SERIALIZER(
		FArrayPropertyNetSerializer);
	FStructNetSerializerConfig FArcItemDataNetSerializer::StructNetSerializerConfig;

	FArcItemDataNetSerializer::FNetSerializerRegistryDelegates
	FArcItemDataNetSerializer::NetSerializerRegistryDelegates;
	static const FName PropertyNetSerializerRegistry_NAME_ArcItemData("ArcItemData");
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemData
		, FArcItemDataNetSerializer);

	FArcItemDataNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemData);
	}

	void FArcItemDataNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcItemData);
	}

	void FArcItemDataNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		UStruct* Struct = FArcItemData::StaticStruct();
		FReplicationStateDescriptorBuilder::FParameters Params;
		Params.SkipCheckForCustomNetSerializerForStruct = true;
		DefaultConfig.StateDescriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(Struct
			, Params);
		StructNetSerializerConfig.StateDescriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(Struct
			, Params);
		const FReplicationStateDescriptor* Descriptor = StructNetSerializerConfig.StateDescriptor.GetReference();
	}
}
*/