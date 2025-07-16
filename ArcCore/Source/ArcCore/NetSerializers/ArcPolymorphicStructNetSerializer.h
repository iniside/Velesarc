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

#pragma once

#include "Containers/Array.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "UObject/ObjectMacros.h"

#include "Iris/Core/IrisLog.h"
#include "Iris/Core/NetObjectReference.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializers.h"

#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsArray.h"
#include "ArcPolymorphicStructNetSerializer.generated.h"

class UScriptStruct;

namespace Arcx::Net
{
	using namespace UE::Net;

	struct FPolymorphicNetSerializerScriptStructCache
	{
		enum : uint32
		{
			RegisteredTypeBits = 5U
		};

		enum : uint32
		{
			MaxRegisteredTypeCount = (1U << RegisteredTypeBits) - 1U
		};

		enum : uint32
		{
			InvalidTypeIndex = 0U
		};

		struct FTypeInfo
		{
			const UScriptStruct* ScriptStruct;
			TRefCountPtr<const FReplicationStateDescriptor> Descriptor;
		};

		ARCCORE_API void InitForType(const UScriptStruct* InScriptStruct);

		inline const uint32 GetTypeIndex(const UScriptStruct* ScriptStruct) const;

		inline const FTypeInfo* GetTypeInfo(uint32 TypeIndex) const;

		inline bool CanHaveNetReferences() const
		{
			return EnumHasAnyFlags(CommonTraits
				, EReplicationStateTraits::HasObjectReference);
		}

	private:
		TArray<FTypeInfo> RegisteredTypes;
		EReplicationStateTraits CommonTraits = EReplicationStateTraits::None;
	};
} // namespace Arcx::Net

USTRUCT()
struct FArcPolymorphicStructNetSerializerConfig : public FNetSerializerConfig
{
	GENERATED_BODY()

	Arcx::Net::FPolymorphicNetSerializerScriptStructCache RegisteredTypes;
};

namespace Arcx::Net
{
	const FPolymorphicNetSerializerScriptStructCache::FTypeInfo*
	FPolymorphicNetSerializerScriptStructCache::GetTypeInfo(uint32 TypeIndex) const
	{
		static_assert(InvalidTypeIndex == 0U, "Expected InvalidTypeIndex to be 0");
		if ((TypeIndex - 1U) > static_cast<uint32>(RegisteredTypes.Num()))
		{
			return nullptr;
		}
		return &RegisteredTypes.GetData()[TypeIndex - 1U];
	}

	const uint32 FPolymorphicNetSerializerScriptStructCache::GetTypeIndex(const UScriptStruct* ScriptStruct) const
	{
		if (ScriptStruct)
		{
			const int32 ArrayIndex = RegisteredTypes.IndexOfByPredicate([&ScriptStruct] (const FTypeInfo& TypeInfo)
			{
				return TypeInfo.ScriptStruct == ScriptStruct;
			});
			if (ArrayIndex != INDEX_NONE)
			{
				return static_cast<uint32>(ArrayIndex + 1);
			}
		}

		return InvalidTypeIndex;
	}
} // namespace Arcx::Net

namespace UE::Net
{
	class FNetReferenceCollector;
	extern IRISCORE_API const FName NetError_PolymorphicStructNetSerializer_InvalidStructType;
} // namespace UE::Net

namespace Arcx::Net::Private
{
	// Wrapper to allow access to the internal context
	struct FPolymorphicStructNetSerializerInternal
	{
	protected:
		ARCCORE_API static void* Alloc(FNetSerializationContext& Context
									   , SIZE_T Size
									   , SIZE_T Alignment);

		ARCCORE_API static void Free(FNetSerializationContext& Context
									 , void* Ptr);

		ARCCORE_API static void CollectReferences(FNetSerializationContext& Context
												  , UE::Net::FNetReferenceCollector& Collector
												  , const FNetSerializerChangeMaskParam& OuterChangeMaskInfo
												  , const uint8* RESTRICT SrcInternalBuffer
												  , const FReplicationStateDescriptor* Descriptor);

		ARCCORE_API static void CloneQuantizedState(FNetSerializationContext& Context
													, uint8* RESTRICT DstInternalBuffer
													, const uint8* RESTRICT SrcInternalBuffer
													, const FReplicationStateDescriptor* Descriptor);
	};
} // namespace Arcx::Net::Private

namespace Arcx::Net
{
	using namespace UE::Net;

	/**
	 * TPolymorphicStructNetSerializerImpl
	 *
	 * Helper to implement serializers that requires dynamic polymorphism.
	 * It can either be used to declare a typed serializer or be used as an internal
	 * helper. ExternalSourceType is the class/struct that has the
	 * TSharedPtr<ExternalSourceItemType> data. ExternalSourceItemType is the polymorphic
	 * struct type GetItem is a function that will return a reference to the
	 * TSharedPtr<ExternalSourceItemType>
	 *
	 * !BIG DISCLAIMER:!
	 *
	 * This serializer was written to mimic the behavior seen in
	 * FGameplayAbilityTargetDataHandle and FGameplayEffectContextHandle which both are
	 * written with the intent of being used for RPCs and not being used for replicated
	 * properties and uses a TSharedPointer to hold the polymorphic struct
	 *
	 * That said, IF the serializer is used for replicated properties it has very specific
	 * requirements on the implementation of the SourceType to work correctly.
	 *
	 * 1. The sourcetype MUST provide a custom assignment operator performing a
	 * deep-copy/clone
	 * 2. The sourcetype MUST define a comparison operator that compares the instance data
	 * of the stored ExternalSourceItemType
	 * 3. TStructOpsTypeTraits::WithCopy and
	 * TStructOpsTypeTraits::WithIdenticalViaEquality must be specified
	 *
	 */
	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	struct TPolymorphicStructNetSerializerImpl : protected Private::FPolymorphicStructNetSerializerInternal
	{
		struct FQuantizedData
		{
			void* StructData;
			uint32 TypeIndex;
		};

		// TODO we really double allocating (item and buffer), but at least there are no
		// new allocations everytime something is changed and replicated.
		static TMap<FArcItemId, TSharedPtr<ExternalSourceItemType>> Items;
		// Traits
		static constexpr bool bHasDynamicState = true;
		static constexpr bool bIsForwardingSerializer = true; // Triggers asserts if a function is missing
		static constexpr bool bHasCustomNetReference = true; // We must support this as we do not know the type
		static constexpr bool bUseDefaultDelta = true;
		using SourceType = ExternalSourceType;
		using QuantizedType = FQuantizedData;
		using ConfigType = FArcPolymorphicStructNetSerializerConfig;

		static void Serialize(FNetSerializationContext&
							  , const FNetSerializeArgs&);

		static void Deserialize(FNetSerializationContext&
								, const FNetDeserializeArgs&);

		static void SerializeDelta(FNetSerializationContext&
								   , const FNetSerializeDeltaArgs&);

		static void DeserializeDelta(FNetSerializationContext&
									 , const FNetDeserializeDeltaArgs&);

		static void Quantize(FNetSerializationContext&
							 , const FNetQuantizeArgs&);

		static void Dequantize(FNetSerializationContext&
							   , const FNetDequantizeArgs&);

		static bool IsEqual(FNetSerializationContext&
							, const FNetIsEqualArgs&);

		static bool Validate(FNetSerializationContext&
							 , const FNetValidateArgs&);

		static void CloneDynamicState(FNetSerializationContext&
									  , const FNetCloneDynamicStateArgs&);

		static void FreeDynamicState(FNetSerializationContext&
									 , const FNetFreeDynamicStateArgs&);

		static void CollectNetReferences(FNetSerializationContext&
										 , const FNetCollectReferencesArgs&);

		static void OnItemRemoved(FArcItemId InItemId)
		{
			Items.Remove(InItemId);
		}

	public:
		using ThisType = TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>;

		using SourceItemType = ExternalSourceItemType;
		using FTypeInfo = FPolymorphicNetSerializerScriptStructCache::FTypeInfo;

		TPolymorphicStructNetSerializerImpl()
		{
			FArcItemDataInternal::OnItemRemovedDelegate.AddRaw(this
				, &TPolymorphicStructNetSerializerImpl::OnItemRemoved);
		}

		struct FSourceItemTypeDeleter
		{
			void operator()(SourceItemType* Object) const
			{
				check(Object);
				UScriptStruct* ScriptStruct = Object->GetScriptStruct();
				check(ScriptStruct);
				ScriptStruct->DestroyStruct(Object);
				FMemory::Free(Object);
			}
		};

		template <typename SerializerType>
		static void InitTypeCache()
		{
			FPolymorphicNetSerializerScriptStructCache* Cache = const_cast<FPolymorphicNetSerializerScriptStructCache*>(
				&SerializerType::DefaultConfig.RegisteredTypes);
			Cache->InitForType(SerializerType::SourceItemType::StaticStruct());
		}

	private:
		static void InternalFreeItem(FNetSerializationContext& Context
									 , const ConfigType& Config
									 , QuantizedType& Value);
	};

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	TMap<FArcItemId, TSharedPtr<ExternalSourceItemType>> Arcx::Net::TPolymorphicStructNetSerializerImpl<
		ExternalSourceType, ExternalSourceItemType, GetItem>::Items;

	/** TPolymorphicStructNetSerializerImpl */
	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	void TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::Serialize(
		FNetSerializationContext& Context
		, const FNetSerializeArgs& Args)
	{
		FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();
		const QuantizedType& Value = *reinterpret_cast<const QuantizedType*>(Args.Source);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		const FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(Value.TypeIndex);
		if (Writer.WriteBool(TypeInfo != nullptr))
		{
			CA_ASSUME(TypeInfo != nullptr);
			Writer.WriteBits(Value.TypeIndex
				, FPolymorphicNetSerializerScriptStructCache::RegisteredTypeBits);
			UE::Net::FReplicationStateOperations::Serialize(Context
				, static_cast<const uint8*>(Value.StructData)
				, TypeInfo->Descriptor);
		}
	}

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	void TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::Deserialize(
		FNetSerializationContext& Context
		, const FNetDeserializeArgs& Args)
	{
		FNetBitStreamReader& Reader = *Context.GetBitStreamReader();
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		InternalFreeItem(Context
			, Config
			, Target);

		QuantizedType TempValue = {};
		if (const bool bIsValidType = Reader.ReadBool())
		{
			const uint32 TypeIndex = Reader.ReadBits(FPolymorphicNetSerializerScriptStructCache::RegisteredTypeBits);
			if (const FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(TypeIndex))
			{
				const FReplicationStateDescriptor* Descriptor = TypeInfo->Descriptor;

				// Allocate storage and read struct data
				TempValue.StructData = FPolymorphicStructNetSerializerInternal::Alloc(Context
					, Descriptor->InternalSize
					, Descriptor->InternalAlignment);
				TempValue.TypeIndex = TypeIndex;

				FMemory::Memzero(TempValue.StructData
					, Descriptor->InternalSize);
				FReplicationStateOperations::Deserialize(Context
					, static_cast<uint8*>(TempValue.StructData)
					, Descriptor);
			}
			else
			{
				Context.SetError(NetError_PolymorphicStructNetSerializer_InvalidStructType);
				// Fall through to clear the target
			}
		}

		Target = TempValue;
	}

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	void TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::SerializeDelta(
		FNetSerializationContext& Context
		, const FNetSerializeDeltaArgs& Args)
	{
		FNetBitStreamWriter& Writer = *Context.GetBitStreamWriter();
		const QuantizedType& Value = *reinterpret_cast<const QuantizedType*>(Args.Source);
		const QuantizedType& PrevValue = *reinterpret_cast<const QuantizedType*>(Args.Prev);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		// If prev has the same type we can delta-compress
		if (Writer.WriteBool(Value.TypeIndex == PrevValue.TypeIndex))
		{
			const FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(Value.TypeIndex);
			if (TypeInfo != nullptr)
			{
				UE::Net::FReplicationStateOperations::SerializeDelta(Context
					, static_cast<const uint8*>(Value.StructData)
					, static_cast<const uint8*>(PrevValue.StructData)
					, TypeInfo->Descriptor);
			}
		}
		else
		{
			const FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(Value.TypeIndex);
			if (Writer.WriteBool(TypeInfo != nullptr))
			{
				CA_ASSUME(TypeInfo != nullptr);
				Writer.WriteBits(Value.TypeIndex
					, FPolymorphicNetSerializerScriptStructCache::RegisteredTypeBits);
				UE::Net::FReplicationStateOperations::Serialize(Context
					, static_cast<const uint8*>(Value.StructData)
					, TypeInfo->Descriptor);
			}
		}
	}

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	void TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::DeserializeDelta(
		FNetSerializationContext& Context
		, const FNetDeserializeDeltaArgs& Args)
	{
		FNetBitStreamReader& Reader = *Context.GetBitStreamReader();
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const QuantizedType& PrevValue = *reinterpret_cast<const QuantizedType*>(Args.Prev);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		InternalFreeItem(Context
			, Config
			, Target);

		QuantizedType TempValue = {};

		// If prev has the same type we can delta-compress
		if (const bool bIsSameType = Reader.ReadBool())
		{
			const uint32 TypeIndex = PrevValue.TypeIndex;
			if (const FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(TypeIndex))
			{
				const FReplicationStateDescriptor* Descriptor = TypeInfo->Descriptor;

				// Allocate storage and read struct data
				TempValue.StructData = FPolymorphicStructNetSerializerInternal::Alloc(Context
					, Descriptor->InternalSize
					, Descriptor->InternalAlignment);
				TempValue.TypeIndex = TypeIndex;

				FMemory::Memzero(TempValue.StructData
					, Descriptor->InternalSize);
				FReplicationStateOperations::DeserializeDelta(Context
					, static_cast<uint8*>(TempValue.StructData)
					, static_cast<uint8*>(PrevValue.StructData)
					, Descriptor);
			}
		}
		else
		{
			if (const bool bIsValidType = Reader.ReadBool())
			{
				const uint32 TypeIndex = Reader.
						ReadBits(FPolymorphicNetSerializerScriptStructCache::RegisteredTypeBits);
				if (const FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(TypeIndex))
				{
					const FReplicationStateDescriptor* Descriptor = TypeInfo->Descriptor;

					// Allocate storage and read struct data
					TempValue.StructData = FPolymorphicStructNetSerializerInternal::Alloc(Context
						, Descriptor->InternalSize
						, Descriptor->InternalAlignment);
					TempValue.TypeIndex = TypeIndex;

					FMemory::Memzero(TempValue.StructData
						, Descriptor->InternalSize);
					FReplicationStateOperations::Deserialize(Context
						, static_cast<uint8*>(TempValue.StructData)
						, Descriptor);
				}
				else
				{
					Context.SetError(NetError_PolymorphicStructNetSerializer_InvalidStructType);
					// Fall through to clear the target
				}
			}
		}

		Target = TempValue;
	}

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	void TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::Quantize(
		FNetSerializationContext& Context
		, const FNetQuantizeArgs& Args)
	{
		SourceType& SourceValue = *reinterpret_cast<SourceType*>(Args.Source);
		QuantizedType& TargetValue = *reinterpret_cast<QuantizedType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		FPolymorphicStructNetSerializerInternal::Free(Context
			, TargetValue.StructData);

		const TSharedPtr<SourceItemType>& Item = GetItem(SourceValue);
		const UScriptStruct* ScriptStruct = Item.IsValid() ? Item->GetScriptStruct() : nullptr;
		const uint32 TypeIndex = Config.RegisteredTypes.GetTypeIndex(ScriptStruct);

		// Quantize polymorphic data
		QuantizedType TempValue = {};
		if (TypeIndex != FPolymorphicNetSerializerScriptStructCache::InvalidTypeIndex)
		{
			const FPolymorphicNetSerializerScriptStructCache::FTypeInfo* TypeInfo = Config.RegisteredTypes.
																						   GetTypeInfo(TypeIndex);
			const FReplicationStateDescriptor* Descriptor = TypeInfo->Descriptor;

			TempValue.TypeIndex = TypeIndex;
			TempValue.StructData = FPolymorphicStructNetSerializerInternal::Alloc(Context
				, Descriptor->InternalSize
				, Descriptor->InternalAlignment);
			FMemory::Memzero(TempValue.StructData
				, Descriptor->InternalSize);

			FReplicationStateOperations::Quantize(Context
				, static_cast<uint8*>(TempValue.StructData)
				, reinterpret_cast<const uint8*>(Item.Get())
				, Descriptor);
		}
		else
		{
			if (ScriptStruct)
			{
				Context.SetError(NetError_PolymorphicStructNetSerializer_InvalidStructType);
				UE_LOG(LogIris
					, Warning
					, TEXT(
						"TPolymorphicStructNetSerializerImpl::Quantize Trying to quantize unregistered ScriptStruct type "
						"%s.")
					, ToCStr(ScriptStruct->GetName()));
			}
		}

		TargetValue = TempValue;
	}

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	void TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::Dequantize(
		FNetSerializationContext& Context
		, const FNetDequantizeArgs& Args)
	{
		const QuantizedType& SourceValue = *reinterpret_cast<const QuantizedType*>(Args.Source);
		SourceType& TargetValue = *reinterpret_cast<SourceType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		// const TSharedPtr<ExternalSourceItemType>& Item = GetItem(SourceValue);

		// SourceItemType* S = reinterpret_cast<SourceItemType*>(SourceValue.StructData);
		//  Dequantize polymorphic data
		if (const FPolymorphicNetSerializerScriptStructCache::FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(
			SourceValue.TypeIndex))
		{
			const FReplicationStateDescriptor* Descriptor = TypeInfo->Descriptor;
			const UScriptStruct* ScriptStruct = TypeInfo->ScriptStruct;
			// if(ScriptStruct == FArcItemData::StaticStruct())
			// if(false)
			{
				FArcItemId ItemId = TargetValue.GetItemId();

				TSharedPtr<ExternalSourceItemType>* Ptr = Items.Find(ItemId);
				if (Ptr != nullptr && (*Ptr).IsValid())
				{
					TSharedPtr<ExternalSourceItemType> Entry = *Ptr;
					// SourceItemType* Item = static_cast<SourceItemType*>(Entry);
					// FMemory::Memzero(Entry);
					// ScriptStruct->GetCppStructOps()->Construct(Item);
					FReplicationStateOperations::Dequantize(Context
						, reinterpret_cast<uint8*>(Entry.Get())
						, static_cast<const uint8*>(SourceValue.StructData)
						, Descriptor);
					GetItem(TargetValue) = Entry;
				}
				else
				{
					void* NewData = nullptr;
					NewData = FMemory::Malloc(ScriptStruct->GetStructureSize()
						, ScriptStruct->GetMinAlignment());
					ScriptStruct->InitializeStruct(NewData);

					// NewData =
					// FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize());
					// ScriptStruct->GetCppStructOps()->Construct(NewData);
					SourceItemType* Item = static_cast<SourceItemType*>(NewData);

					FReplicationStateOperations::Dequantize(Context
						, reinterpret_cast<uint8*>(NewData)
						, static_cast<const uint8*>(SourceValue.StructData)
						, Descriptor);
					TSharedPtr<ExternalSourceItemType> Entry = MakeShareable(
						static_cast<ExternalSourceItemType*>(NewData));
					Items.FindOrAdd(ItemId
						, Entry);
					GetItem(TargetValue) = Entry;
				}
				// NOTE: We always allocate new memory in order to behave like the code we
				// are trying to mimic expects, see GameplayEffectContextHandle this
				// should really be a policy of this class as it is far from optimal.

				// Allocate external struct, owned by external state therefore using
				// global allocator SourceItemType* NewData =
				// static_cast<SourceItemType*>(FMemory::Malloc(ScriptStruct->GetStructureSize(),
				// ScriptStruct->GetMinAlignment()));
			}
			// else
			//{
			//	SourceItemType* NewData = nullptr;
			//	if(NewData == nullptr)
			//	{
			//		NewData =
			// static_cast<SourceItemType*>(FMemory::Malloc(ScriptStruct->GetStructureSize(),
			// ScriptStruct->GetMinAlignment()));
			//	}
			//	// NOTE: We always allocate new memory in order to behave like the code we
			// are trying to mimic expects,
			// see
			// GameplayEffectContextHandle
			//	// this should really be a policy of this class as it is far from optimal.
			//
			//	// Allocate external struct, owned by external state therefore using
			// global allocator
			//	//SourceItemType* NewData =
			// static_cast<SourceItemType*>(FMemory::Malloc(ScriptStruct->GetStructureSize(),
			// ScriptStruct->GetMinAlignment()));
			// ScriptStruct->InitializeStruct(NewData);
			//
			//	FReplicationStateOperations::Dequantize(Context,
			// reinterpret_cast<uint8*>(NewData), static_cast<const
			// uint8*>(SourceValue.StructData), Descriptor);
			//
			//	GetItem(TargetValue) = MakeShareable(NewData, FSourceItemTypeDeleter());
			// }
		}
		else
		{
			GetItem(TargetValue).Reset();
		}
	}

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	bool TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::IsEqual(
		FNetSerializationContext& Context
		, const FNetIsEqualArgs& Args)
	{
		if (Args.bStateIsQuantized)
		{
			const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
			const QuantizedType& ValueA = *reinterpret_cast<const QuantizedType*>(Args.Source0);
			const QuantizedType& ValueB = *reinterpret_cast<const QuantizedType*>(Args.Source1);

			if (ValueA.TypeIndex != ValueB.TypeIndex)
			{
				return false;
			}

			const FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(ValueA.TypeIndex);
			checkSlow(TypeInfo != nullptr);
			if (TypeInfo != nullptr && !UE::Net::FReplicationStateOperations::IsEqualQuantizedState(Context
					, static_cast<const uint8*>(ValueA.StructData)
					, static_cast<const uint8*>(ValueB.StructData)
					, TypeInfo->Descriptor))
			{
				return false;
			}
		}
		else
		{
			const SourceType& ValueA = *reinterpret_cast<const SourceType*>(Args.Source0);
			const SourceType& ValueB = *reinterpret_cast<const SourceType*>(Args.Source1);

			// Assuming there's a custom operator== because if there's not we would be
			// hitting TSharedRef== which checks if the reference is identical, not the
			// instance data.
			return ValueA == ValueB;
		}

		return true;
	}

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	bool TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::Validate(
		FNetSerializationContext& Context
		, const FNetValidateArgs& Args)
	{
		SourceType& SourceValue = *reinterpret_cast<SourceType*>(Args.Source);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		const TSharedPtr<SourceItemType>& Item = GetItem(SourceValue);
		const UScriptStruct* ScriptStruct = Item.IsValid() ? Item->GetScriptStruct() : nullptr;
		const uint32 TypeIndex = Config.RegisteredTypes.GetTypeIndex(ScriptStruct);

		if (ScriptStruct != nullptr && (TypeIndex == FPolymorphicNetSerializerScriptStructCache::InvalidTypeIndex))
		{
			return false;
		}

		const FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(TypeIndex);
		if (TypeInfo != nullptr && !UE::Net::FReplicationStateOperations::Validate(Context
				, reinterpret_cast<const uint8*>(Item.Get())
				, TypeInfo->Descriptor))
		{
			return false;
		}

		return true;
	}

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	void TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::CloneDynamicState(
		FNetSerializationContext& Context
		, const FNetCloneDynamicStateArgs& Args)
	{
		const QuantizedType& SourceValue = *reinterpret_cast<const QuantizedType*>(Args.Source);
		QuantizedType& TargetValue = *reinterpret_cast<QuantizedType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		// Clone polymorphic data
		const FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(SourceValue.TypeIndex);
		TargetValue.TypeIndex = SourceValue.TypeIndex;

		if (TypeInfo)
		{
			const FReplicationStateDescriptor* Descriptor = TypeInfo->Descriptor;

			// We need some memory to store the state for the polymorphic struct
			TargetValue.StructData = FPolymorphicStructNetSerializerInternal::Alloc(Context
				, Descriptor->InternalSize
				, Descriptor->InternalAlignment);
			FPolymorphicStructNetSerializerInternal::CloneQuantizedState(Context
				, static_cast<uint8*>(TargetValue.StructData)
				, static_cast<const uint8*>(SourceValue.StructData)
				, Descriptor);
		}
		else
		{
			TargetValue.StructData = nullptr;
		}
	}

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	void TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::FreeDynamicState(
		FNetSerializationContext& Context
		, const FNetFreeDynamicStateArgs& Args)
	{
		QuantizedType& SourceValue = *reinterpret_cast<QuantizedType*>(Args.Source);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		InternalFreeItem(Context
			, Config
			, SourceValue);
	}

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	void TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::CollectNetReferences(
		FNetSerializationContext& Context
		, const FNetCollectReferencesArgs& Args)
	{
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		const QuantizedType& Value = *reinterpret_cast<const QuantizedType*>(Args.Source);
		FNetReferenceCollector& Collector = *reinterpret_cast<UE::Net::FNetReferenceCollector*>(Args.Collector);

		// No references nothing to do
		if (Value.TypeIndex == FPolymorphicNetSerializerScriptStructCache::InvalidTypeIndex || !Config.RegisteredTypes.
																									   CanHaveNetReferences())
		{
			return;
		}

		const FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(Value.TypeIndex);
		checkSlow(TypeInfo != nullptr);
		const FReplicationStateDescriptor* Descriptor = TypeInfo ? TypeInfo->Descriptor : nullptr;
		if (Descriptor != nullptr && EnumHasAnyFlags(Descriptor->Traits
				, EReplicationStateTraits::HasObjectReference))
		{
			FPolymorphicStructNetSerializerInternal::CollectReferences(Context
				, Collector
				, Args.ChangeMaskInfo
				, static_cast<const uint8*>(Value.StructData)
				, Descriptor);
		}
	}

	template <typename ExternalSourceType, typename ExternalSourceItemType, TSharedPtr<ExternalSourceItemType>& (*
				  GetItem)(ExternalSourceType&)>
	void TPolymorphicStructNetSerializerImpl<ExternalSourceType, ExternalSourceItemType, GetItem>::InternalFreeItem(
		FNetSerializationContext& Context
		, const ConfigType& Config
		, QuantizedType& Value)
	{
		const FTypeInfo* TypeInfo = Config.RegisteredTypes.GetTypeInfo(Value.TypeIndex);
		const FReplicationStateDescriptor* Descriptor = TypeInfo ? TypeInfo->Descriptor : nullptr;

		if (Value.StructData != nullptr)
		{
			checkSlow(Descriptor != nullptr);
			if (Descriptor && EnumHasAnyFlags(Descriptor->Traits
					, EReplicationStateTraits::HasDynamicState))
			{
				FReplicationStateOperations::FreeDynamicState(Context
					, static_cast<uint8*>(Value.StructData)
					, Descriptor);
			}
			FPolymorphicStructNetSerializerInternal::Free(Context
				, Value.StructData);
			Value.StructData = nullptr;
			Value.TypeIndex = FPolymorphicNetSerializerScriptStructCache::InvalidTypeIndex;
		}
	}
}
