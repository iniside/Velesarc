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

#include "ArcPolymorphicStructNetSerializer.h"

#include "Iris/Core/IrisProfiler.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/ReplicationSystem/ReplicationOperationsInternal.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Iris/Serialization/PolymorphicNetSerializerImpl.h"
#include "UObject/UObjectIterator.h"

namespace Arcx::Net
{
	using namespace UE::Net;

	const FName NetError_PolymorphicStructNetSerializer_InvalidStructType(TEXT("Invalid struct type"));

	void FPolymorphicNetSerializerScriptStructCache::InitForType(const UScriptStruct* InScriptStruct)
	{
		IRIS_PROFILER_SCOPE(FPolymorphicNetSerializerScriptStructCache_InitForType);

		RegisteredTypes.Reset();

		// Find all script structs of this type and add them to the list and build
		// descriptor (not sure of a better way to do this but it should only happen once
		// at startup)
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			if (It->IsChildOf(InScriptStruct))
			{
				FTypeInfo Entry;
				Entry.ScriptStruct = *It;

				// Get or create descriptor
				FReplicationStateDescriptorBuilder::FParameters Params;
				Entry.Descriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(*It
					, Params);

				if (Entry.Descriptor.IsValid())
				{
					RegisteredTypes.Add(Entry);

					CommonTraits |= (Entry.Descriptor->Traits & EReplicationStateTraits::HasObjectReference);
				}
				else
				{
					UE_LOG(LogIris
						, Error
						, TEXT(
							"FPolymorphicNetSerializerScriptStructCache::InitForType Failed to create descriptor for type "
							"%s when building cache for base %s")
						, ToCStr((*It)->GetName())
						, ToCStr(InScriptStruct->GetName()));
				}
			}
		}

		RegisteredTypes.Sort([] (const FTypeInfo& A
								 , const FTypeInfo& B)
		{
			return A.ScriptStruct->GetName().ToLower() > B.ScriptStruct->GetName().ToLower();
		});

		if ((uint32)RegisteredTypes.Num() > MaxRegisteredTypeCount)
		{
			UE_LOG(LogIris
				, Error
				, TEXT( "FPolymorphicNetSerializerScriptStructCache::InitForType Too many types (%u of %u) of base %s")
				, RegisteredTypes.Num()
				, MaxRegisteredTypeCount
				, ToCStr(InScriptStruct->GetName()));
			check((uint32)RegisteredTypes.Num() <= MaxRegisteredTypeCount);
		}
	}
} // namespace Arcx::Net

namespace Arcx::Net::Private
{
	using namespace UE::Net::Private;

	void* FPolymorphicStructNetSerializerInternal::Alloc(FNetSerializationContext& Context
														 , SIZE_T Size
														 , SIZE_T Alignment)
	{
		return FMemory::Malloc(Size, Alignment);
	}

	void FPolymorphicStructNetSerializerInternal::Free(FNetSerializationContext& Context
													   , void* Ptr)
	{
		return FMemory::Free(Ptr);
	}

	void FPolymorphicStructNetSerializerInternal::CollectReferences(FNetSerializationContext& Context
																	, FNetReferenceCollector& Collector
																	, const FNetSerializerChangeMaskParam&
																	OuterChangeMaskInfo
																	, const uint8* RESTRICT SrcInternalBuffer
																	, const FReplicationStateDescriptor* Descriptor)
	{
		return UE::Net::Private::FReplicationStateOperationsInternal::CollectReferences(Context
			, Collector
			, OuterChangeMaskInfo
			, SrcInternalBuffer
			, Descriptor);
	}

	void FPolymorphicStructNetSerializerInternal::CloneQuantizedState(FNetSerializationContext& Context
																	  , uint8* RESTRICT DstInternalBuffer
																	  , const uint8* RESTRICT SrcInternalBuffer
																	  , const FReplicationStateDescriptor* Descriptor)
	{
		return UE::Net::Private::FReplicationStateOperationsInternal::CloneQuantizedState(Context
			, DstInternalBuffer
			, SrcInternalBuffer
			, Descriptor);
	}
}
