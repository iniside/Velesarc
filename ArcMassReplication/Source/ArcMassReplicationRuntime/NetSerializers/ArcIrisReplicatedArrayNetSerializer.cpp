// Copyright Lukasz Baran. All Rights Reserved.

#include "NetSerializers/ArcIrisReplicatedArrayNetSerializer.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Containers/ScriptArray.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcIrisReplicatedArray, Verbose, All);

namespace ArcMassReplication
{
	using namespace UE::Net;

	namespace ReplicatedArrayPrivate
	{
		FArcIrisReplicatedArrayNetSerializer::FDynamicState* EnsureState(FArcIrisReplicatedArrayNetSerializer::QuantizedType& Target)
		{
			UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("EnsureState: State=%s"), Target.State ? TEXT("exists") : TEXT("null, allocating"));
			if (Target.State == nullptr)
			{
				Target.State = new FArcIrisReplicatedArrayNetSerializer::FDynamicState();
			}
			return Target.State;
		}

		uint8* AllocInternalBuffer(const FReplicationStateDescriptor* Descriptor)
		{
			uint32 InternalSize = Descriptor->InternalSize;
			uint32 InternalAlignment = Descriptor->InternalAlignment > 16U ? Descriptor->InternalAlignment : 16U;
			uint8* Buffer = static_cast<uint8*>(FMemory::Malloc(InternalSize, InternalAlignment));
			FMemory::Memzero(Buffer, InternalSize);
			return Buffer;
		}

		void FreeInternalBuffer(FNetSerializationContext& Context, const FReplicationStateDescriptor* Descriptor, uint8* Buffer)
		{
			if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
			{
				const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
				FStructNetSerializerConfig StructConfig;
				StructConfig.StateDescriptor = Descriptor;

				FNetFreeDynamicStateArgs FreeArgs;
				FreeArgs.Version = 0;
				FreeArgs.NetSerializerConfig = &StructConfig;
				FreeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(Buffer);
				StructSerializer->FreeDynamicState(Context, FreeArgs);
			}
			FMemory::Free(Buffer);
		}
	} // namespace ReplicatedArrayPrivate

	// --- Per-item helpers ---

	void FArcIrisReplicatedArrayNetSerializer::QuantizeItem(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const uint8* SrcMemory,
		FNetSerializerAlignedStorage& OutQuantizedState)
	{
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("QuantizeItem: Descriptor=%s HasDynState=%d InternalSize=%u"),
			Descriptor ? TEXT("valid") : TEXT("null"),
			(Descriptor && EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState)) ? 1 : 0,
			Descriptor ? Descriptor->InternalSize : 0u);

		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		FreeQuantizedItemState(Context, Descriptor, OutQuantizedState);

		uint8* InternalBuffer = ReplicatedArrayPrivate::AllocInternalBuffer(Descriptor);

		FNetQuantizeArgs QuantizeArgs;
		QuantizeArgs.Version = 0;
		QuantizeArgs.NetSerializerConfig = &StructConfig;
		QuantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(SrcMemory);
		QuantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		StructSerializer->Quantize(Context, QuantizeArgs);
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("QuantizeItem: Quantize done HasError=%d"), Context.HasErrorOrOverflow() ? 1 : 0);

		OutQuantizedState.AdjustSize(Context, Descriptor->InternalSize, Descriptor->InternalAlignment);
		FMemory::Memcpy(OutQuantizedState.GetData(), InternalBuffer, Descriptor->InternalSize);

		FMemory::Free(InternalBuffer);
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("QuantizeItem: exit QuantizedStateSize=%d"), OutQuantizedState.Num());
	}

	void FArcIrisReplicatedArrayNetSerializer::SerializeItemFromQuantized(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const FNetSerializerAlignedStorage& QuantizedState)
	{
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("SerializeItemFromQuantized: Descriptor=%s QuantizedStateSize=%d"),
			Descriptor ? TEXT("valid") : TEXT("null"), QuantizedState.Num());

		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		uint8* InternalBuffer = ReplicatedArrayPrivate::AllocInternalBuffer(Descriptor);
		FMemory::Memcpy(InternalBuffer, QuantizedState.GetData(), Descriptor->InternalSize);

		FNetSerializeArgs SerializeArgs;
		SerializeArgs.Version = 0;
		SerializeArgs.NetSerializerConfig = &StructConfig;
		SerializeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		StructSerializer->Serialize(Context, SerializeArgs);
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("SerializeItemFromQuantized: Serialize done HasError=%d"), Context.HasErrorOrOverflow() ? 1 : 0);

		FMemory::Free(InternalBuffer);
	}

	void FArcIrisReplicatedArrayNetSerializer::DeserializeItemToQuantized(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		FNetSerializerAlignedStorage& OutQuantizedState)
	{
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("DeserializeItemToQuantized: Descriptor=%s HasDynState=%d InternalSize=%u"),
			Descriptor ? TEXT("valid") : TEXT("null"),
			(Descriptor && EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState)) ? 1 : 0,
			Descriptor ? Descriptor->InternalSize : 0u);

		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		uint8* InternalBuffer = ReplicatedArrayPrivate::AllocInternalBuffer(Descriptor);

		FNetDeserializeArgs DeserializeArgs;
		DeserializeArgs.Version = 0;
		DeserializeArgs.NetSerializerConfig = &StructConfig;
		DeserializeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		StructSerializer->Deserialize(Context, DeserializeArgs);
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("DeserializeItemToQuantized: Deserialize done HasError=%d"), Context.HasErrorOrOverflow() ? 1 : 0);

		if (Context.HasErrorOrOverflow())
		{
			ReplicatedArrayPrivate::FreeInternalBuffer(Context, Descriptor, InternalBuffer);
			return;
		}

		FreeQuantizedItemState(Context, Descriptor, OutQuantizedState);
		OutQuantizedState.AdjustSize(Context, Descriptor->InternalSize, Descriptor->InternalAlignment);
		FMemory::Memcpy(OutQuantizedState.GetData(), InternalBuffer, Descriptor->InternalSize);

		FMemory::Free(InternalBuffer);
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("DeserializeItemToQuantized: exit QuantizedStateSize=%d"), OutQuantizedState.Num());
	}

	void FArcIrisReplicatedArrayNetSerializer::DequantizeItem(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const FNetSerializerAlignedStorage& QuantizedState,
		uint8* DstMemory)
	{
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("DequantizeItem: Descriptor=%s QuantizedStateSize=%d DstMemory=%s"),
			Descriptor ? TEXT("valid") : TEXT("null"),
			QuantizedState.Num(),
			DstMemory ? TEXT("valid") : TEXT("null"));

		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		uint8* InternalBuffer = ReplicatedArrayPrivate::AllocInternalBuffer(Descriptor);
		FMemory::Memcpy(InternalBuffer, QuantizedState.GetData(), Descriptor->InternalSize);

		FNetDequantizeArgs DequantizeArgs;
		DequantizeArgs.Version = 0;
		DequantizeArgs.NetSerializerConfig = &StructConfig;
		DequantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		DequantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(DstMemory);
		StructSerializer->Dequantize(Context, DequantizeArgs);
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("DequantizeItem: Dequantize done HasError=%d"), Context.HasErrorOrOverflow() ? 1 : 0);

		FMemory::Free(InternalBuffer);
	}

	void FArcIrisReplicatedArrayNetSerializer::CloneQuantizedItemState(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const FQuantizedItem& Source,
		FQuantizedItem& Target)
	{
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("CloneQuantizedItemState: RepID=%d SrcSize=%d"),
			Source.ReplicationID, Source.QuantizedState.Num());

		Target.ReplicationID = Source.ReplicationID;

		if (Source.QuantizedState.Num() == 0)
		{
			UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("CloneQuantizedItemState: QuantizedState empty, skip"));
			return;
		}

		Target.QuantizedState.AdjustSize(Context, Source.QuantizedState.Num(), Source.QuantizedState.GetAlignment());
		FMemory::Memcpy(Target.QuantizedState.GetData(), Source.QuantizedState.GetData(), Source.QuantizedState.Num());

		if (Descriptor != nullptr && EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
		{
			UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("CloneQuantizedItemState: cloning dynamic state"));
			const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
			FStructNetSerializerConfig StructConfig;
			StructConfig.StateDescriptor = Descriptor;

			FNetCloneDynamicStateArgs CloneArgs;
			CloneArgs.Version = 0;
			CloneArgs.NetSerializerConfig = &StructConfig;
			CloneArgs.Source = reinterpret_cast<NetSerializerValuePointer>(Source.QuantizedState.GetData());
			CloneArgs.Target = reinterpret_cast<NetSerializerValuePointer>(Target.QuantizedState.GetData());
			StructSerializer->CloneDynamicState(Context, CloneArgs);
		}
	}

	void FArcIrisReplicatedArrayNetSerializer::FreeQuantizedItemState(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		FNetSerializerAlignedStorage& QuantizedState)
	{
		if (QuantizedState.Num() == 0)
		{
			return;
		}

		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("FreeQuantizedItemState: Num=%d HasDynState=%d"),
			QuantizedState.Num(),
			(Descriptor && EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState)) ? 1 : 0);

		if (Descriptor != nullptr && EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
		{
			const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
			FStructNetSerializerConfig StructConfig;
			StructConfig.StateDescriptor = Descriptor;

			FNetFreeDynamicStateArgs FreeArgs;
			FreeArgs.Version = 0;
			FreeArgs.NetSerializerConfig = &StructConfig;
			FreeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(QuantizedState.GetData());
			StructSerializer->FreeDynamicState(Context, FreeArgs);
		}

		QuantizedState.Free(Context);
	}

	// --- Quantize ---

	void FArcIrisReplicatedArrayNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
	{
		const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		FDynamicState* State = ReplicatedArrayPrivate::EnsureState(Target);

		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Quantize: enter ConfigDesc=%s ConfigOffset=%u ConfigStride=%u PendingRemovals=%d AddedIDs=%d ChangedIDs=%d"),
			Config.ItemDescriptor.IsValid() ? TEXT("valid") : TEXT("null"),
			Config.ItemsArrayOffset, Config.ItemStride,
			Source.PendingRemovals.Num(), Source.ReceivedAddedIDs.Num(), Source.ReceivedChangedIDs.Num());

		if (!State->ItemDescriptor.IsValid() && Config.ItemDescriptor.IsValid())
		{
			State->ItemDescriptor = Config.ItemDescriptor;
			State->ItemsArrayOffset = Config.ItemsArrayOffset;
			State->ItemStride = Config.ItemStride;
		}

		const FReplicationStateDescriptor* Descriptor = State->ItemDescriptor.GetReference();
		if (Descriptor == nullptr)
		{
			UE_LOG(LogArcIrisReplicatedArray, Warning, TEXT("Quantize: Descriptor null, returning early"));
			return;
		}

		State->AddedIDs.Reset();
		State->ChangedIDs.Reset();
		State->RemovedIDs.Reset();
		bool bHasChanges = false;

		for (int32 RemovedID : Source.PendingRemovals)
		{
			UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Quantize: removing RepID=%d from AllItems"), RemovedID);
			if (FQuantizedItem* RemovedItem = State->AllItems.Find(RemovedID))
			{
				FreeQuantizedItemState(Context, Descriptor, RemovedItem->QuantizedState);
				State->AllItems.Remove(RemovedID);
			}
			State->RemovedIDs.Add(RemovedID);
			bHasChanges = true;
		}

		const uint8* ArrayBase = reinterpret_cast<const uint8*>(&Source) + State->ItemsArrayOffset;
		const FScriptArray& RawArray = *reinterpret_cast<const FScriptArray*>(ArrayBase);
		const int32 ItemCount = RawArray.Num();
		const uint8* ArrayData = static_cast<const uint8*>(RawArray.GetData());

		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Quantize: ItemCount=%d at Offset=%u"), ItemCount, State->ItemsArrayOffset);

		for (int32 AddedID : Source.ReceivedAddedIDs)
		{
			const uint8* ItemMemory = nullptr;
			for (int32 Idx = 0; Idx < ItemCount; ++Idx)
			{
				const FArcIrisReplicatedArrayItem* Candidate = reinterpret_cast<const FArcIrisReplicatedArrayItem*>(ArrayData + Idx * State->ItemStride);
				if (Candidate->IrisRepID == AddedID)
				{
					ItemMemory = ArrayData + Idx * State->ItemStride;
					break;
				}
			}
			if (ItemMemory == nullptr)
			{
				UE_LOG(LogArcIrisReplicatedArray, Warning, TEXT("Quantize: AddedID=%d not found in items array, skipping"), AddedID);
				continue;
			}
			FQuantizedItem& QItem = State->AllItems.FindOrAdd(AddedID);
			QItem.ReplicationID = AddedID;
			QuantizeItem(Context, Descriptor, ItemMemory, QItem.QuantizedState);
			State->AddedIDs.Add(AddedID);
			bHasChanges = true;
			UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Quantize: added RepID=%d"), AddedID);
		}

		for (int32 ChangedID : Source.ReceivedChangedIDs)
		{
			const uint8* ItemMemory = nullptr;
			for (int32 Idx = 0; Idx < ItemCount; ++Idx)
			{
				const FArcIrisReplicatedArrayItem* Candidate = reinterpret_cast<const FArcIrisReplicatedArrayItem*>(ArrayData + Idx * State->ItemStride);
				if (Candidate->IrisRepID == ChangedID)
				{
					ItemMemory = ArrayData + Idx * State->ItemStride;
					break;
				}
			}
			if (ItemMemory == nullptr)
			{
				UE_LOG(LogArcIrisReplicatedArray, Warning, TEXT("Quantize: ChangedID=%d not found in items array, skipping"), ChangedID);
				continue;
			}
			FQuantizedItem& QItem = State->AllItems.FindOrAdd(ChangedID);
			QItem.ReplicationID = ChangedID;
			QuantizeItem(Context, Descriptor, ItemMemory, QItem.QuantizedState);
			State->ChangedIDs.Add(ChangedID);
			bHasChanges = true;
			UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Quantize: changed RepID=%d"), ChangedID);
		}

		if (bHasChanges)
		{
			++State->ChangeVersion;
			UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Quantize: ChangeVersion=%llu AllItems=%d Added=%d Changed=%d Removed=%d"),
				State->ChangeVersion, State->AllItems.Num(), State->AddedIDs.Num(), State->ChangedIDs.Num(), State->RemovedIDs.Num());
		}
	}

	// --- IsEqual ---

	bool FArcIrisReplicatedArrayNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
	{
		if (!Args.bStateIsQuantized)
		{
			return false;
		}

		const QuantizedType& A = *reinterpret_cast<const QuantizedType*>(Args.Source0);
		const QuantizedType& B = *reinterpret_cast<const QuantizedType*>(Args.Source1);

		uint64 VersionA = A.State ? A.State->ChangeVersion : 0;
		uint64 VersionB = B.State ? B.State->ChangeVersion : 0;
		bool bEqual = (VersionA == VersionB);
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("IsEqual: VersionA=%llu VersionB=%llu bEqual=%d"), VersionA, VersionB, bEqual ? 1 : 0);
		return bEqual;
	}

	// --- Serialize (full state) ---

	void FArcIrisReplicatedArrayNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

		if (Source.State == nullptr)
		{
			Writer->WriteBits(0U, 16U);
			return;
		}

		const FDynamicState& State = *Source.State;
		const FReplicationStateDescriptor* Descriptor = State.ItemDescriptor.GetReference();

		uint16 ItemCount = static_cast<uint16>(State.AllItems.Num());
		Writer->WriteBits(static_cast<uint32>(ItemCount), 16U);
		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Serialize: ItemCount=%d"), (int32)ItemCount);

		for (const TPair<int32, FQuantizedItem>& Pair : State.AllItems)
		{
			Writer->WriteBits(static_cast<uint32>(Pair.Key), 32U);
			if (Descriptor != nullptr)
			{
				SerializeItemFromQuantized(Context, Descriptor, Pair.Value.QuantizedState);
			}
		}
	}

	// --- Deserialize (full state) ---

	void FArcIrisReplicatedArrayNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
	{
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		FDynamicState* State = ReplicatedArrayPrivate::EnsureState(Target);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		if (!State->ItemDescriptor.IsValid() && Config.ItemDescriptor.IsValid())
		{
			State->ItemDescriptor = Config.ItemDescriptor;
			State->ItemsArrayOffset = Config.ItemsArrayOffset;
			State->ItemStride = Config.ItemStride;
		}

		const FReplicationStateDescriptor* Descriptor = State->ItemDescriptor.GetReference();

		for (TPair<int32, FQuantizedItem>& Pair : State->AllItems)
		{
			FreeQuantizedItemState(Context, Descriptor, Pair.Value.QuantizedState);
		}
		State->AllItems.Reset();
		State->AddedIDs.Reset();
		State->ChangedIDs.Reset();
		State->RemovedIDs.Reset();

		uint32 ItemCount = Reader->ReadBits(16U);
		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Deserialize: ItemCount=%u"), ItemCount);

		for (uint32 ItemIdx = 0; ItemIdx < ItemCount; ++ItemIdx)
		{
			int32 RepID = static_cast<int32>(Reader->ReadBits(32U));

			FQuantizedItem& QItem = State->AllItems.Add(RepID);
			QItem.ReplicationID = RepID;

			if (Descriptor != nullptr)
			{
				DeserializeItemToQuantized(Context, Descriptor, QItem.QuantizedState);
				if (Context.HasErrorOrOverflow())
				{
					UE_LOG(LogArcIrisReplicatedArray, Warning, TEXT("Deserialize: error at item[%u] RepID=%d, aborting"), ItemIdx, RepID);
					return;
				}
			}

			State->AddedIDs.Add(RepID);
		}

		++State->ChangeVersion;
		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Deserialize: ChangeVersion=%llu AllItems=%d Added=%d"),
			State->ChangeVersion, State->AllItems.Num(), State->AddedIDs.Num());
	}

	// --- SerializeDelta ---

	void FArcIrisReplicatedArrayNetSerializer::SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

		const FDynamicState* State = Source.State;
		const FReplicationStateDescriptor* Descriptor = State ? State->ItemDescriptor.GetReference() : nullptr;

		uint8 RemovedCount = State ? static_cast<uint8>(FMath::Min(State->RemovedIDs.Num(), 255)) : 0;
		Writer->WriteBits(static_cast<uint32>(RemovedCount), 8U);
		if (State)
		{
			for (int32 Idx = 0; Idx < RemovedCount; ++Idx)
			{
				Writer->WriteBits(static_cast<uint32>(State->RemovedIDs[Idx]), 32U);
			}
		}

		uint16 AddedCount = State ? static_cast<uint16>(State->AddedIDs.Num()) : 0;
		Writer->WriteBits(static_cast<uint32>(AddedCount), 16U);
		if (State)
		{
			for (int32 AddedID : State->AddedIDs)
			{
				Writer->WriteBits(static_cast<uint32>(AddedID), 32U);
				if (Descriptor)
				{
					if (const FQuantizedItem* QItem = State->AllItems.Find(AddedID))
					{
						SerializeItemFromQuantized(Context, Descriptor, QItem->QuantizedState);
					}
				}
			}
		}

		uint16 ChangedCount = State ? static_cast<uint16>(State->ChangedIDs.Num()) : 0;
		Writer->WriteBits(static_cast<uint32>(ChangedCount), 16U);
		if (State)
		{
			for (int32 ChangedID : State->ChangedIDs)
			{
				Writer->WriteBits(static_cast<uint32>(ChangedID), 32U);
				if (Descriptor)
				{
					if (const FQuantizedItem* QItem = State->AllItems.Find(ChangedID))
					{
						SerializeItemFromQuantized(Context, Descriptor, QItem->QuantizedState);
					}
				}
			}
		}

		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("SerializeDelta: Removed=%d Added=%d Changed=%d"),
			(int32)RemovedCount, (int32)AddedCount, (int32)ChangedCount);
	}

	// --- DeserializeDelta ---

	void FArcIrisReplicatedArrayNetSerializer::DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args)
	{
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const QuantizedType& Prev = *reinterpret_cast<const QuantizedType*>(Args.Prev);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		FDynamicState* State = ReplicatedArrayPrivate::EnsureState(Target);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		if (Prev.State && !State->ItemDescriptor.IsValid() && Prev.State->ItemDescriptor.IsValid())
		{
			State->ItemDescriptor = Prev.State->ItemDescriptor;
		}
		if (!State->ItemDescriptor.IsValid() && Config.ItemDescriptor.IsValid())
		{
			State->ItemDescriptor = Config.ItemDescriptor;
			State->ItemsArrayOffset = Config.ItemsArrayOffset;
			State->ItemStride = Config.ItemStride;
		}

		const FReplicationStateDescriptor* Descriptor = State->ItemDescriptor.GetReference();
		const FReplicationStateDescriptor* PrevDescriptor = (Prev.State) ? Prev.State->ItemDescriptor.GetReference() : nullptr;

		// Seed AllItems from prev baseline
		for (TPair<int32, FQuantizedItem>& Pair : State->AllItems)
		{
			FreeQuantizedItemState(Context, Descriptor, Pair.Value.QuantizedState);
		}
		State->AllItems.Reset();
		State->AddedIDs.Reset();
		State->ChangedIDs.Reset();
		State->RemovedIDs.Reset();

		if (Prev.State)
		{
			for (const TPair<int32, FQuantizedItem>& Pair : Prev.State->AllItems)
			{
				FQuantizedItem& NewItem = State->AllItems.Add(Pair.Key);
				CloneQuantizedItemState(Context, PrevDescriptor, Pair.Value, NewItem);
			}
		}

		// Read Removed
		uint32 RemovedCount = Reader->ReadBits(8U);
		for (uint32 Idx = 0; Idx < RemovedCount; ++Idx)
		{
			int32 RepID = static_cast<int32>(Reader->ReadBits(32U));
			if (FQuantizedItem* Item = State->AllItems.Find(RepID))
			{
				FreeQuantizedItemState(Context, Descriptor, Item->QuantizedState);
				State->AllItems.Remove(RepID);
			}
			State->RemovedIDs.Add(RepID);
		}

		// Read Added (with data)
		uint32 AddedCount = Reader->ReadBits(16U);
		for (uint32 ItemIdx = 0; ItemIdx < AddedCount; ++ItemIdx)
		{
			int32 RepID = static_cast<int32>(Reader->ReadBits(32U));
			FQuantizedItem& QItem = State->AllItems.FindOrAdd(RepID);
			QItem.ReplicationID = RepID;
			if (Descriptor)
			{
				DeserializeItemToQuantized(Context, Descriptor, QItem.QuantizedState);
				if (Context.HasErrorOrOverflow()) return;
			}
			State->AddedIDs.Add(RepID);
		}

		// Read Changed (with data)
		uint32 ChangedCount = Reader->ReadBits(16U);
		for (uint32 ItemIdx = 0; ItemIdx < ChangedCount; ++ItemIdx)
		{
			int32 RepID = static_cast<int32>(Reader->ReadBits(32U));
			FQuantizedItem& QItem = State->AllItems.FindOrAdd(RepID);
			QItem.ReplicationID = RepID;
			if (Descriptor)
			{
				DeserializeItemToQuantized(Context, Descriptor, QItem.QuantizedState);
				if (Context.HasErrorOrOverflow()) return;
			}
			State->ChangedIDs.Add(RepID);
		}

		++State->ChangeVersion;
		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("DeserializeDelta: ChangeVersion=%llu AllItems=%d Added=%d Changed=%d Removed=%d"),
			State->ChangeVersion, State->AllItems.Num(), State->AddedIDs.Num(), State->ChangedIDs.Num(), State->RemovedIDs.Num());
	}

	// --- Dequantize ---

	void FArcIrisReplicatedArrayNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		if (Source.State == nullptr)
		{
			return;
		}

		const FDynamicState& State = *Source.State;
		const FReplicationStateDescriptor* Descriptor = State.ItemDescriptor.GetReference();

		if (Descriptor == nullptr || Config.ItemStride == 0 || Config.ItemsArrayOffset == 0)
		{
			UE_LOG(LogArcIrisReplicatedArray, Warning, TEXT("Dequantize: early return Descriptor=%s Stride=%u Offset=%u"),
				Descriptor ? TEXT("valid") : TEXT("null"), Config.ItemStride, Config.ItemsArrayOffset);
			return;
		}

		uint32 Stride = Config.ItemStride;
		uint8* TargetBase = reinterpret_cast<uint8*>(&Target) + Config.ItemsArrayOffset;
		FScriptArray& DstRawArray = *reinterpret_cast<FScriptArray*>(TargetBase);
		uint32 ItemAlign = alignof(FArcIrisReplicatedArrayItem);
		DstRawArray.Empty(0, Stride, ItemAlign);

		for (const TPair<int32, FQuantizedItem>& Pair : State.AllItems)
		{
			int32 NewIdx = DstRawArray.AddZeroed(1, Stride, ItemAlign);
			uint8* ItemMemory = static_cast<uint8*>(DstRawArray.GetData()) + (NewIdx * Stride);
			DequantizeItem(Context, Descriptor, Pair.Value.QuantizedState, ItemMemory);
		}

		Target.PendingRemovals.Reset();
		for (int32 RemovedID : State.RemovedIDs)
		{
			Target.PendingRemovals.Add(RemovedID);
		}

		Target.ReceivedAddedIDs = State.AddedIDs;
		Target.ReceivedChangedIDs = State.ChangedIDs;

		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Dequantize: Items=%d Added=%d Changed=%d Removed=%d"),
			DstRawArray.Num(), State.AddedIDs.Num(), State.ChangedIDs.Num(), State.RemovedIDs.Num());
	}

	// --- Validate ---

	bool FArcIrisReplicatedArrayNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
	{
		return true;
	}

	// --- Apply ---

	void FArcIrisReplicatedArrayNetSerializer::Apply(FNetSerializationContext& Context, const FNetApplyArgs& Args)
	{
		const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
		SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		const uint32 Stride = Config.ItemStride;
		const uint32 ArrayOffset = Config.ItemsArrayOffset;
		const UScriptStruct* ItemStruct = Config.ItemStruct;

		if (Stride == 0 || ArrayOffset == 0 || ItemStruct == nullptr)
		{
			UE_LOG(LogArcIrisReplicatedArray, Warning, TEXT("Apply: missing config (Stride=%u Offset=%u ItemStruct=%s) — skipping"),
				Stride, ArrayOffset, ItemStruct ? *ItemStruct->GetName() : TEXT("null"));
			return;
		}

		const uint32 ItemAlign = ItemStruct->GetMinAlignment();

		// Source is the staged (dequantized) SourceType — Source.Items holds the full incoming snapshot,
		// and Source.PendingRemovals / ReceivedAddedIDs / ReceivedChangedIDs were populated by Dequantize.
		const uint8* SrcArrayBase = reinterpret_cast<const uint8*>(&Source) + ArrayOffset;
		const FScriptArray& SrcRawArray = *reinterpret_cast<const FScriptArray*>(SrcArrayBase);
		const int32 SrcItemCount = SrcRawArray.Num();
		const uint8* SrcData = static_cast<const uint8*>(SrcRawArray.GetData());

		// Target is the live SourceType — what the receiver had last frame.
		uint8* DstArrayBase = reinterpret_cast<uint8*>(&Target) + ArrayOffset;
		FScriptArray& DstRawArray = *reinterpret_cast<FScriptArray*>(DstArrayBase);

		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Apply: DstItems=%d SrcItems=%d Added=%d Changed=%d Removed=%d"),
			DstRawArray.Num(), SrcItemCount,
			Source.ReceivedAddedIDs.Num(), Source.ReceivedChangedIDs.Num(), Source.PendingRemovals.Num());

		auto FindSrcByID = [&](int32 ID) -> const uint8*
		{
			for (int32 Idx = 0; Idx < SrcItemCount; ++Idx)
			{
				const FArcIrisReplicatedArrayItem* Cand = reinterpret_cast<const FArcIrisReplicatedArrayItem*>(SrcData + Idx * Stride);
				if (Cand->IrisRepID == ID)
				{
					return SrcData + Idx * Stride;
				}
			}
			return nullptr;
		};

		auto FindDstIdxByID = [&](int32 ID) -> int32
		{
			const int32 N = DstRawArray.Num();
			const uint8* DstData = static_cast<const uint8*>(DstRawArray.GetData());
			for (int32 Idx = 0; Idx < N; ++Idx)
			{
				const FArcIrisReplicatedArrayItem* Cand = reinterpret_cast<const FArcIrisReplicatedArrayItem*>(DstData + Idx * Stride);
				if (Cand->IrisRepID == ID)
				{
					return Idx;
				}
			}
			return INDEX_NONE;
		};

		// 1) Removals — fire PreReplicatedRemove on the still-present item, collect indices, then erase.
		TArray<int32> RemoveIndices;
		RemoveIndices.Reserve(Source.PendingRemovals.Num());
		for (int32 RemovedID : Source.PendingRemovals)
		{
			const int32 DstIdx = FindDstIdxByID(RemovedID);
			if (DstIdx == INDEX_NONE)
			{
				UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("Apply: REMOVE RepID=%d not found on receiver, skipping"), RemovedID);
				continue;
			}
			uint8* ItemMem = static_cast<uint8*>(DstRawArray.GetData()) + DstIdx * Stride;
			UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Apply: REMOVE RepID=%d DstIdx=%d"), RemovedID, DstIdx);
			if (Target.PreReplicatedRemoveCallback)
			{
				Target.PreReplicatedRemoveCallback(ItemMem, Target);
			}
			RemoveIndices.Add(DstIdx);
		}
		RemoveIndices.Sort([](int32 A, int32 B) { return A > B; });
		for (int32 Idx : RemoveIndices)
		{
			uint8* ItemMem = static_cast<uint8*>(DstRawArray.GetData()) + Idx * Stride;
			ItemStruct->DestroyStruct(ItemMem);
			DstRawArray.Remove(Idx, 1, Stride, ItemAlign);
		}

		// 2) Adds — append (or upgrade-to-Change if already present with different contents),
		//    copy item state from staged Source, propagate IDs, fire callbacks.
		//
		// Why content-diff for "already present" Added IDs: when the wire path uses full-state
		// Serialize (instead of SerializeDelta), receiver-side ReceivedAddedIDs is populated
		// with every item ID and ReceivedChangedIDs is empty. Item-level compare here turns
		// "Added but actually changed" into a real Change callback, so the test (and consumers)
		// see modifications regardless of which wire path was taken.
		for (int32 AddedID : Source.ReceivedAddedIDs)
		{
			const uint8* SrcItem = FindSrcByID(AddedID);
			if (SrcItem == nullptr)
			{
				UE_LOG(LogArcIrisReplicatedArray, Warning, TEXT("Apply: ADD RepID=%d not in staged source, skipping"), AddedID);
				continue;
			}
			const int32 ExistingDstIdx = FindDstIdxByID(AddedID);
			if (ExistingDstIdx != INDEX_NONE)
			{
				uint8* DstItem = static_cast<uint8*>(DstRawArray.GetData()) + ExistingDstIdx * Stride;
				const bool bIdentical = ItemStruct->CompareScriptStruct(DstItem, SrcItem, PPF_None);
				if (bIdentical)
				{
					UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("Apply: ADD RepID=%d already present and identical, no-op"), AddedID);
					continue;
				}
				ItemStruct->CopyScriptStruct(DstItem, SrcItem);
				UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Apply: ADD RepID=%d already present but contents differ → fired CHANGE DstIdx=%d Callback=%s"),
					AddedID, ExistingDstIdx, Target.PostReplicatedChangeCallback ? TEXT("set") : TEXT("null"));
				if (Target.PostReplicatedChangeCallback)
				{
					Target.PostReplicatedChangeCallback(DstItem, Target);
				}
				continue;
			}

			const int32 NewIdx = DstRawArray.AddZeroed(1, Stride, ItemAlign);
			uint8* DstItem = static_cast<uint8*>(DstRawArray.GetData()) + NewIdx * Stride;
			ItemStruct->InitializeStruct(DstItem);
			ItemStruct->CopyScriptStruct(DstItem, SrcItem);

			UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Apply: ADD RepID=%d DstIdx=%d Callback=%s"),
				AddedID, NewIdx, Target.PostReplicatedAddCallback ? TEXT("set") : TEXT("null"));
			if (Target.PostReplicatedAddCallback)
			{
				Target.PostReplicatedAddCallback(DstItem, Target);
			}
		}

		// 3) Changes — copy item state, fire PostReplicatedChange. If the ID isn't on the receiver yet, treat as Add.
		for (int32 ChangedID : Source.ReceivedChangedIDs)
		{
			const uint8* SrcItem = FindSrcByID(ChangedID);
			if (SrcItem == nullptr)
			{
				UE_LOG(LogArcIrisReplicatedArray, Warning, TEXT("Apply: CHANGE RepID=%d not in staged source, skipping"), ChangedID);
				continue;
			}

			int32 DstIdx = FindDstIdxByID(ChangedID);
			if (DstIdx == INDEX_NONE)
			{
				const int32 NewIdx = DstRawArray.AddZeroed(1, Stride, ItemAlign);
				uint8* DstItem = static_cast<uint8*>(DstRawArray.GetData()) + NewIdx * Stride;
				ItemStruct->InitializeStruct(DstItem);
				ItemStruct->CopyScriptStruct(DstItem, SrcItem);
				UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Apply: CHANGE RepID=%d not present, fired ADD instead DstIdx=%d"), ChangedID, NewIdx);
				if (Target.PostReplicatedAddCallback)
				{
					Target.PostReplicatedAddCallback(DstItem, Target);
				}
				continue;
			}

			uint8* DstItem = static_cast<uint8*>(DstRawArray.GetData()) + DstIdx * Stride;
			ItemStruct->CopyScriptStruct(DstItem, SrcItem);
			UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("Apply: CHANGE RepID=%d DstIdx=%d Callback=%s"),
				ChangedID, DstIdx, Target.PostReplicatedChangeCallback ? TEXT("set") : TEXT("null"));
			if (Target.PostReplicatedChangeCallback)
			{
				Target.PostReplicatedChangeCallback(DstItem, Target);
			}
		}
	}

	// --- Dynamic state ---

	void FArcIrisReplicatedArrayNetSerializer::CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);

		if (Source.State == nullptr)
		{
			Target.State = nullptr;
			return;
		}

		FDynamicState* NewState = new FDynamicState();
		NewState->ItemDescriptor = Source.State->ItemDescriptor;
		NewState->ItemsArrayOffset = Source.State->ItemsArrayOffset;
		NewState->ItemStride = Source.State->ItemStride;
		NewState->ChangeVersion = Source.State->ChangeVersion;

		const FReplicationStateDescriptor* Descriptor = Source.State->ItemDescriptor.GetReference();
		for (const TPair<int32, FQuantizedItem>& Pair : Source.State->AllItems)
		{
			FQuantizedItem& TargetItem = NewState->AllItems.Add(Pair.Key);
			CloneQuantizedItemState(Context, Descriptor, Pair.Value, TargetItem);
		}
		NewState->AddedIDs = Source.State->AddedIDs;
		NewState->ChangedIDs = Source.State->ChangedIDs;
		NewState->RemovedIDs = Source.State->RemovedIDs;
		Target.State = NewState;
	}

	void FArcIrisReplicatedArrayNetSerializer::FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args)
	{
		QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Source);

		if (Value.State != nullptr)
		{
			const FReplicationStateDescriptor* Descriptor = Value.State->ItemDescriptor.GetReference();
			for (TPair<int32, FQuantizedItem>& Pair : Value.State->AllItems)
			{
				FreeQuantizedItemState(Context, Descriptor, Pair.Value.QuantizedState);
			}
			delete Value.State;
			Value.State = nullptr;
		}
	}

	// --- CollectNetReferences ---

	void FArcIrisReplicatedArrayNetSerializer::CollectNetReferences(FNetSerializationContext& Context, const FNetCollectReferencesArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		UE_LOG(LogArcIrisReplicatedArray, Verbose, TEXT("CollectNetReferences: State=%s"), Source.State ? TEXT("valid") : TEXT("null"));

		if (Source.State == nullptr)
		{
			return;
		}

		const FDynamicState& State = *Source.State;
		const FReplicationStateDescriptor* Descriptor = State.ItemDescriptor.GetReference();
		if (Descriptor == nullptr)
		{
			return;
		}

		if (!EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasObjectReference))
		{
			return;
		}

		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		const uint32 InternalAlignment = Descriptor->InternalAlignment > 16U ? Descriptor->InternalAlignment : 16U;

		for (const TPair<int32, FQuantizedItem>& Pair : State.AllItems)
		{
			const FQuantizedItem& QItem = Pair.Value;
			if (QItem.QuantizedState.Num() == 0)
			{
				continue;
			}

			uint8* TempBuffer = static_cast<uint8*>(FMemory::Malloc(Descriptor->InternalSize, InternalAlignment));
			FMemory::Memcpy(TempBuffer, QItem.QuantizedState.GetData(), Descriptor->InternalSize);

			FNetCollectReferencesArgs ItemArgs = Args;
			ItemArgs.Version = 0;
			ItemArgs.NetSerializerConfig = &StructConfig;
			ItemArgs.Source = reinterpret_cast<NetSerializerValuePointer>(TempBuffer);
			StructSerializer->CollectNetReferences(Context, ItemArgs);

			FMemory::Free(TempBuffer);
		}
	}

	// --- BuildNetSerializerConfig ---

	FArcIrisReplicatedArrayPropertyNetSerializerInfo::FArcIrisReplicatedArrayPropertyNetSerializerInfo(
		const FName InStructName, const FNetSerializer& InSerializer)
		: FNamedStructPropertyNetSerializerInfo(InStructName, InSerializer)
	{
	}

	bool FArcIrisReplicatedArrayPropertyNetSerializerInfo::IsSupported(const FProperty* Property) const
	{
		const FStructProperty* StructProp = CastField<FStructProperty>(Property);
		if (StructProp == nullptr)
		{
			return false;
		}
		const UScriptStruct* Struct = StructProp->Struct;
		while (Struct != nullptr)
		{
			if (IsSupportedStruct(Struct->GetFName()))
			{
				return true;
			}
			Struct = Cast<UScriptStruct>(Struct->GetSuperStruct());
		}
		return false;
	}

	FNetSerializerConfig* FArcIrisReplicatedArrayPropertyNetSerializerInfo::BuildNetSerializerConfig(
		void* NetSerializerConfigBuffer, const FProperty* Property) const
	{
		FArcIrisReplicatedArrayNetSerializerConfig* Config =
			new (NetSerializerConfigBuffer) FArcIrisReplicatedArrayNetSerializerConfig();

		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("BuildNetSerializerConfig: enter Property=%s"),
			Property ? *Property->GetName() : TEXT("null"));

		const UScriptStruct* ConcreteStruct = nullptr;
		if (Property != nullptr)
		{
			const FStructProperty* StructProp = CastField<FStructProperty>(Property);
			if (StructProp != nullptr)
			{
				ConcreteStruct = StructProp->Struct;
				UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("BuildNetSerializerConfig: ConcreteStruct from property = %s"),
					*ConcreteStruct->GetName());
			}
		}

		if (ConcreteStruct == nullptr)
		{
			FString StructFName = FString(TEXT("F")) + StructName.ToString();
			ConcreteStruct = FindObject<UScriptStruct>(nullptr, *StructFName);
			if (ConcreteStruct == nullptr)
			{
				ConcreteStruct = FindFirstObject<UScriptStruct>(*StructFName, EFindFirstObjectOptions::NativeFirst);
			}
			UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("BuildNetSerializerConfig: ConcreteStruct from name lookup = %s"),
				ConcreteStruct ? *ConcreteStruct->GetName() : TEXT("null"));
		}

		if (ConcreteStruct == nullptr)
		{
			UE_LOG(LogArcIrisReplicatedArray, Warning, TEXT("BuildNetSerializerConfig: ConcreteStruct null, returning empty config"));
			return Config;
		}

		bool bFoundArray = false;
		for (TFieldIterator<FArrayProperty> It(ConcreteStruct); It; ++It)
		{
			const FArrayProperty* ArrayProp = *It;
			const FStructProperty* InnerProp = CastField<FStructProperty>(ArrayProp->Inner);
			UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("BuildNetSerializerConfig: FArrayProperty '%s' InnerProp=%s InnerStruct=%s"),
				*ArrayProp->GetName(),
				InnerProp ? TEXT("valid") : TEXT("null"),
				(InnerProp && InnerProp->Struct) ? *InnerProp->Struct->GetName() : TEXT("none"));

			if (InnerProp == nullptr)
			{
				continue;
			}

			const UScriptStruct* ItemStruct = InnerProp->Struct;
			bool bIsItem = ItemStruct->IsChildOf(FFastArraySerializerItem::StaticStruct());
			UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("BuildNetSerializerConfig: ItemStruct=%s IsChildOfFastArraySerializerItem=%d"),
				*ItemStruct->GetName(), bIsItem ? 1 : 0);

			if (!bIsItem)
			{
				continue;
			}

			Config->ItemDescriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(ItemStruct);
			Config->ItemsArrayOffset = static_cast<uint32>(ArrayProp->GetOffset_ForGC());
			Config->ItemStride = static_cast<uint32>(ItemStruct->GetStructureSize());
			Config->ItemStruct = ItemStruct;

			UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("BuildNetSerializerConfig: SELECTED ItemStruct=%s ItemDescriptor=%s MemberCount=%d Offset=%u Stride=%u"),
				*ItemStruct->GetName(),
				Config->ItemDescriptor.IsValid() ? TEXT("valid") : TEXT("null/invalid"),
				Config->ItemDescriptor.IsValid() ? (int32)Config->ItemDescriptor->MemberCount : -1,
				Config->ItemsArrayOffset,
				Config->ItemStride);

			bFoundArray = true;
			break;
		}

		if (!bFoundArray)
		{
			UE_LOG(LogArcIrisReplicatedArray, Warning, TEXT("BuildNetSerializerConfig: no suitable FArrayProperty found in %s"), *ConcreteStruct->GetName());
		}

		return Config;
	}

	// --- Public helper: apply a replicated fragment payload onto a live target ---

	void ApplyReplicatedFragmentToTarget(
		uint8* DstFragmentMemory,
		const uint8* SrcFragmentMemory,
		const UScriptStruct* FragmentType,
		const FReplicationStateDescriptor* FragmentDescriptor)
	{
		if (DstFragmentMemory == nullptr || SrcFragmentMemory == nullptr || FragmentType == nullptr)
		{
			UE_LOG(LogArcIrisReplicatedArray, Warning, TEXT("ApplyReplicatedFragmentToTarget: null memory or type, skipping (Dst=%p Src=%p Type=%s)"),
				DstFragmentMemory, SrcFragmentMemory, FragmentType ? *FragmentType->GetName() : TEXT("null"));
			return;
		}

		// 1) Inner Apply for each FArcIrisReplicatedArray member — diffs against Dst's previous
		//    state, mutates Dst.Items, fires callbacks on the Dst instance.
		if (FragmentDescriptor != nullptr)
		{
			const FNetSerializer& ArcArraySerializer = UE_NET_GET_SERIALIZER(FArcIrisReplicatedArrayNetSerializer);
			FNetSerializationContext StubContext;
			for (uint32 MemberIdx = 0; MemberIdx < FragmentDescriptor->MemberCount; ++MemberIdx)
			{
				const FReplicationStateMemberSerializerDescriptor& MemberSer = FragmentDescriptor->MemberSerializerDescriptors[MemberIdx];
				if (MemberSer.Serializer != &ArcArraySerializer)
				{
					continue;
				}

				const uint32 ExternalOffset = FragmentDescriptor->MemberDescriptors[MemberIdx].ExternalMemberOffset;
				FNetApplyArgs Args;
				Args.Version = 0;
				Args.NetSerializerConfig = MemberSer.SerializerConfig;
				Args.Source = reinterpret_cast<NetSerializerValuePointer>(SrcFragmentMemory + ExternalOffset);
				Args.Target = reinterpret_cast<NetSerializerValuePointer>(DstFragmentMemory + ExternalOffset);

				UE_LOG(LogArcIrisReplicatedArray, Log,
					TEXT("ApplyReplicatedFragmentToTarget: invoking inner Apply Fragment=%s MemberIdx=%u Offset=%u"),
					*FragmentType->GetName(), MemberIdx, ExternalOffset);
				FArcIrisReplicatedArrayNetSerializer::Apply(StubContext, Args);
			}
		}

		// 2) Copy non-array UPROPERTY members one by one. Skip any FArcIrisReplicatedArray-
		//    derived members — inner Apply already handled those, and using
		//    CopyScriptStruct (which uses C++ operator= for STRUCT_CopyNative types) would
		//    overwrite the non-UPROPERTY callback counters that inner Apply just bumped.
		for (TFieldIterator<FProperty> PropIt(FragmentType); PropIt; ++PropIt)
		{
			bool bIsReplicatedArrayMember = false;
			if (const FStructProperty* StructProp = CastField<FStructProperty>(*PropIt))
			{
				const UScriptStruct* MemberStruct = StructProp->Struct;
				while (MemberStruct != nullptr)
				{
					if (MemberStruct->GetFName() == FName(TEXT("ArcIrisReplicatedArray")))
					{
						bIsReplicatedArrayMember = true;
						break;
					}
					MemberStruct = Cast<UScriptStruct>(MemberStruct->GetSuperStruct());
				}
			}
			if (bIsReplicatedArrayMember)
			{
				continue;
			}
			PropIt->CopyCompleteValue_InContainer(DstFragmentMemory, SrcFragmentMemory);
		}
	}

	// --- Registration ---

	UE_NET_IMPLEMENT_SERIALIZER(FArcIrisReplicatedArrayNetSerializer);
	FArcIrisReplicatedArrayNetSerializer::ConfigType FArcIrisReplicatedArrayNetSerializer::DefaultConfig = FArcIrisReplicatedArrayNetSerializer::ConfigType();

	FArcIrisReplicatedArrayNetSerializer::FNetSerializerRegistryDelegates FArcIrisReplicatedArrayNetSerializer::NetSerializerRegistryDelegates;

	static const FName PropertyNetSerializerRegistry_NAME_ArcIrisReplicatedArray(TEXT("ArcIrisReplicatedArray"));

	static FArcIrisReplicatedArrayPropertyNetSerializerInfo ArcIrisReplicatedArraySerializerInfo(
		PropertyNetSerializerRegistry_NAME_ArcIrisReplicatedArray,
		UE_NET_GET_SERIALIZER(FArcIrisReplicatedArrayNetSerializer));

	FArcIrisReplicatedArrayNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		FPropertyNetSerializerInfoRegistry::Unregister(&ArcIrisReplicatedArraySerializerInfo);
	}

	void FArcIrisReplicatedArrayNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("OnPreFreezeNetSerializerRegistry: registering FArcIrisReplicatedArrayNetSerializer"));
		FPropertyNetSerializerInfoRegistry::Register(&ArcIrisReplicatedArraySerializerInfo);
	}

	void FArcIrisReplicatedArrayNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
		UE_LOG(LogArcIrisReplicatedArray, Log, TEXT("OnPostFreezeNetSerializerRegistry: called"));
	}

} // namespace ArcMassReplication
