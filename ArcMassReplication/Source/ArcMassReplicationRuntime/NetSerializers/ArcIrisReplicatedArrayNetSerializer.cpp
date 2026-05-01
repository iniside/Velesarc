// Copyright Lukasz Baran. All Rights Reserved.

#include "NetSerializers/ArcIrisReplicatedArrayNetSerializer.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializers.h"
#include "Iris/ReplicationState/ReplicationStateDescriptorBuilder.h"
#include "Containers/ScriptArray.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "UObject/UnrealType.h"

namespace ArcMassReplication
{
	using namespace UE::Net;

	namespace ReplicatedArrayPrivate
	{
		FArcIrisReplicatedArrayNetSerializer::FDynamicState* EnsureState(FArcIrisReplicatedArrayNetSerializer::QuantizedType& Target)
		{
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
		TArray<uint8>& OutQuantizedState)
	{
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		uint8* InternalBuffer = ReplicatedArrayPrivate::AllocInternalBuffer(Descriptor);

		FNetQuantizeArgs QuantizeArgs;
		QuantizeArgs.Version = 0;
		QuantizeArgs.NetSerializerConfig = &StructConfig;
		QuantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(SrcMemory);
		QuantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		StructSerializer->Quantize(Context, QuantizeArgs);

		OutQuantizedState.SetNumUninitialized(Descriptor->InternalSize);
		FMemory::Memcpy(OutQuantizedState.GetData(), InternalBuffer, Descriptor->InternalSize);

		ReplicatedArrayPrivate::FreeInternalBuffer(Context, Descriptor, InternalBuffer);
	}

	void FArcIrisReplicatedArrayNetSerializer::SerializeItemFromQuantized(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const TArray<uint8>& QuantizedState)
	{
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

		ReplicatedArrayPrivate::FreeInternalBuffer(Context, Descriptor, InternalBuffer);
	}

	void FArcIrisReplicatedArrayNetSerializer::DeserializeItemToQuantized(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		TArray<uint8>& OutQuantizedState)
	{
		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		uint8* InternalBuffer = ReplicatedArrayPrivate::AllocInternalBuffer(Descriptor);

		FNetDeserializeArgs DeserializeArgs;
		DeserializeArgs.Version = 0;
		DeserializeArgs.NetSerializerConfig = &StructConfig;
		DeserializeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		StructSerializer->Deserialize(Context, DeserializeArgs);

		if (Context.HasErrorOrOverflow())
		{
			ReplicatedArrayPrivate::FreeInternalBuffer(Context, Descriptor, InternalBuffer);
			return;
		}

		OutQuantizedState.SetNumUninitialized(Descriptor->InternalSize);
		FMemory::Memcpy(OutQuantizedState.GetData(), InternalBuffer, Descriptor->InternalSize);

		ReplicatedArrayPrivate::FreeInternalBuffer(Context, Descriptor, InternalBuffer);
	}

	void FArcIrisReplicatedArrayNetSerializer::DequantizeItem(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const TArray<uint8>& QuantizedState,
		uint8* DstMemory)
	{
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

		ReplicatedArrayPrivate::FreeInternalBuffer(Context, Descriptor, InternalBuffer);
	}

	void FArcIrisReplicatedArrayNetSerializer::FreeQuantizedItemState(
		const FReplicationStateDescriptor* Descriptor,
		TArray<uint8>& QuantizedState)
	{
		if (!EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
		{
			return;
		}

		if (QuantizedState.Num() == 0)
		{
			return;
		}

		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		uint32 InternalAlignment = Descriptor->InternalAlignment > 16U ? Descriptor->InternalAlignment : 16U;
		uint8* TempBuffer = static_cast<uint8*>(FMemory::Malloc(Descriptor->InternalSize, InternalAlignment));
		FMemory::Memcpy(TempBuffer, QuantizedState.GetData(), Descriptor->InternalSize);

		// FreeDynamicState needs a context but we don't have one here — use a default-constructed one
		FNetSerializationContext TempContext;
		FNetFreeDynamicStateArgs FreeArgs;
		FreeArgs.Version = 0;
		FreeArgs.NetSerializerConfig = &StructConfig;
		FreeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(TempBuffer);
		StructSerializer->FreeDynamicState(TempContext, FreeArgs);

		FMemory::Free(TempBuffer);
	}

	// --- Quantize ---

	void FArcIrisReplicatedArrayNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::Quantize"));
		const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);
		FDynamicState* State = ReplicatedArrayPrivate::EnsureState(Target);

		if (!State->ItemDescriptor.IsValid() && Config.ItemDescriptor.IsValid())
		{
			State->ItemDescriptor = Config.ItemDescriptor;
			State->ItemsArrayOffset = Config.ItemsArrayOffset;
			State->ItemStride = Config.ItemStride;
		}

		const FReplicationStateDescriptor* Descriptor = State->ItemDescriptor.GetReference();
		if (Descriptor == nullptr)
		{
			return;
		}

		bool bHasChanges = false;

		State->RemovedReplicationIDs.Reset();
		for (int32 RemovedID : Source.PendingRemovals)
		{
			State->Baseline.Remove(RemovedID);
			State->RemovedReplicationIDs.Add(RemovedID);
			bHasChanges = true;
		}

		const uint8* ArrayBase = reinterpret_cast<const uint8*>(&Source) + State->ItemsArrayOffset;
		const FScriptArray& RawArray = *reinterpret_cast<const FScriptArray*>(ArrayBase);
		int32 ItemCount = RawArray.Num();
		const uint8* ArrayData = static_cast<const uint8*>(RawArray.GetData());

		for (int32 ItemIdx = 0; ItemIdx < ItemCount; ++ItemIdx)
		{
			if (!Source.DirtyItems.IsValidIndex(ItemIdx) || !Source.DirtyItems[ItemIdx])
			{
				continue;
			}

			const uint8* ItemMemory = ArrayData + (ItemIdx * State->ItemStride);
			const FArcIrisReplicatedArrayItem* ItemBase = reinterpret_cast<const FArcIrisReplicatedArrayItem*>(ItemMemory);
			int32 RepID = ItemBase->IrisRepID;

			FQuantizedItem& QItem = State->Baseline.FindOrAdd(RepID);
			QItem.ReplicationID = RepID;
			QItem.ReplicationKey = static_cast<uint32>(ItemBase->IrisRepKey);

			QuantizeItem(Context, Descriptor, ItemMemory, QItem.QuantizedState);
			bHasChanges = true;
		}

		if (bHasChanges)
		{
			++State->ChangeVersion;
		}

		const_cast<SourceType&>(Source).ClearDirtyState(ItemCount);
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
		return (VersionA == VersionB);
	}

	// --- Serialize (full state) ---

	void FArcIrisReplicatedArrayNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::Serialize"));
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

		if (Source.State == nullptr)
		{
			Writer->WriteBits(0U, 8U);
			Writer->WriteBits(0U, 16U);
			return;
		}

		const FDynamicState& State = *Source.State;
		const FReplicationStateDescriptor* Descriptor = State.ItemDescriptor.GetReference();

		uint8 RemovedCount = static_cast<uint8>(FMath::Min(State.RemovedReplicationIDs.Num(), 255));
		Writer->WriteBits(static_cast<uint32>(RemovedCount), 8U);
		for (int32 Idx = 0; Idx < RemovedCount; ++Idx)
		{
			Writer->WriteBits(static_cast<uint32>(State.RemovedReplicationIDs[Idx]), 32U);
		}

		uint16 ItemCount = static_cast<uint16>(State.Baseline.Num());
		Writer->WriteBits(static_cast<uint32>(ItemCount), 16U);

		for (const TPair<int32, FQuantizedItem>& Pair : State.Baseline)
		{
			const FQuantizedItem& QItem = Pair.Value;
			Writer->WriteBits(static_cast<uint32>(QItem.ReplicationID), 32U);
			Writer->WriteBits(QItem.ReplicationKey, 32U);

			if (Descriptor != nullptr)
			{
				SerializeItemFromQuantized(Context, Descriptor, QItem.QuantizedState);
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
		UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::Deserialize Target=%p State=%p Descriptor=%p ConfigDesc=%p ConfigOffset=%u ConfigStride=%u"),
			&Target, Target.State, Descriptor, Config.ItemDescriptor.GetReference(), Config.ItemsArrayOffset, Config.ItemStride);

		State->Baseline.Reset();
		State->ReceivedDirtyIDs.Reset();
		State->ReceivedRemovedIDs.Reset();

		uint32 RemovedCount = Reader->ReadBits(8U);
		for (uint32 Idx = 0; Idx < RemovedCount; ++Idx)
		{
			int32 RemovedID = static_cast<int32>(Reader->ReadBits(32U));
			State->ReceivedRemovedIDs.Add(RemovedID);
		}

		uint32 ItemCount = Reader->ReadBits(16U);
		for (uint32 ItemIdx = 0; ItemIdx < ItemCount; ++ItemIdx)
		{
			int32 RepID = static_cast<int32>(Reader->ReadBits(32U));
			uint32 RepKey = Reader->ReadBits(32U);

			FQuantizedItem& QItem = State->Baseline.Add(RepID);
			QItem.ReplicationID = RepID;
			QItem.ReplicationKey = RepKey;

			if (Descriptor != nullptr)
			{
				DeserializeItemToQuantized(Context, Descriptor, QItem.QuantizedState);

				if (Context.HasErrorOrOverflow())
				{
					return;
				}
			}

			State->ReceivedDirtyIDs.Add(RepID);
		}

		++State->ChangeVersion;
	}

	// --- SerializeDelta ---

	void FArcIrisReplicatedArrayNetSerializer::SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::SerializeDelta"));
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		const QuantizedType& Prev = *reinterpret_cast<const QuantizedType*>(Args.Prev);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

		const FDynamicState* CurrentState = Source.State;
		const FDynamicState* PrevState = Prev.State;
		const FReplicationStateDescriptor* Descriptor = CurrentState ? CurrentState->ItemDescriptor.GetReference() : nullptr;

		uint8 RemovedCount = CurrentState ? static_cast<uint8>(FMath::Min(CurrentState->RemovedReplicationIDs.Num(), 255)) : 0;
		Writer->WriteBits(static_cast<uint32>(RemovedCount), 8U);
		if (CurrentState)
		{
			for (int32 Idx = 0; Idx < RemovedCount; ++Idx)
			{
				Writer->WriteBits(static_cast<uint32>(CurrentState->RemovedReplicationIDs[Idx]), 32U);
			}
		}

		TArray<const FQuantizedItem*> DirtyItems;
		if (CurrentState)
		{
			for (const TPair<int32, FQuantizedItem>& CurPair : CurrentState->Baseline)
			{
				bool bIsDirty = false;
				if (!PrevState || !PrevState->Baseline.Contains(CurPair.Key))
				{
					bIsDirty = true;
				}
				else
				{
					const FQuantizedItem& PrevItem = PrevState->Baseline.FindChecked(CurPair.Key);
					if (PrevItem.ReplicationKey != CurPair.Value.ReplicationKey)
					{
						bIsDirty = true;
					}
				}

				if (bIsDirty)
				{
					DirtyItems.Add(&CurPair.Value);
				}
			}
		}

		uint16 DirtyCount = static_cast<uint16>(DirtyItems.Num());
		Writer->WriteBits(static_cast<uint32>(DirtyCount), 16U);

		for (const FQuantizedItem* QItem : DirtyItems)
		{
			Writer->WriteBits(static_cast<uint32>(QItem->ReplicationID), 32U);
			Writer->WriteBits(QItem->ReplicationKey, 32U);

			if (Descriptor != nullptr)
			{
				SerializeItemFromQuantized(Context, Descriptor, QItem->QuantizedState);
			}
		}
	}

	// --- DeserializeDelta ---

	void FArcIrisReplicatedArrayNetSerializer::DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::DeserializeDelta"));
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const QuantizedType& Prev = *reinterpret_cast<const QuantizedType*>(Args.Prev);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		FDynamicState* State = ReplicatedArrayPrivate::EnsureState(Target);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		if (Prev.State)
		{
			State->Baseline = Prev.State->Baseline;
			if (!State->ItemDescriptor.IsValid() && Prev.State->ItemDescriptor.IsValid())
			{
				State->ItemDescriptor = Prev.State->ItemDescriptor;
			}
		}

		if (!State->ItemDescriptor.IsValid() && Config.ItemDescriptor.IsValid())
		{
			State->ItemDescriptor = Config.ItemDescriptor;
			State->ItemsArrayOffset = Config.ItemsArrayOffset;
			State->ItemStride = Config.ItemStride;
		}

		const FReplicationStateDescriptor* Descriptor = State->ItemDescriptor.GetReference();

		State->ReceivedDirtyIDs.Reset();
		State->ReceivedRemovedIDs.Reset();

		uint32 RemovedCount = Reader->ReadBits(8U);
		for (uint32 Idx = 0; Idx < RemovedCount; ++Idx)
		{
			int32 RemovedID = static_cast<int32>(Reader->ReadBits(32U));
			State->Baseline.Remove(RemovedID);
			State->ReceivedRemovedIDs.Add(RemovedID);
		}

		uint32 DirtyCount = Reader->ReadBits(16U);
		for (uint32 ItemIdx = 0; ItemIdx < DirtyCount; ++ItemIdx)
		{
			int32 RepID = static_cast<int32>(Reader->ReadBits(32U));
			uint32 RepKey = Reader->ReadBits(32U);

			FQuantizedItem& QItem = State->Baseline.FindOrAdd(RepID);
			QItem.ReplicationID = RepID;
			QItem.ReplicationKey = RepKey;

			if (Descriptor != nullptr)
			{
				DeserializeItemToQuantized(Context, Descriptor, QItem.QuantizedState);

				if (Context.HasErrorOrOverflow())
				{
					return;
				}
			}

			State->ReceivedDirtyIDs.Add(RepID);
		}

		++State->ChangeVersion;
	}

	// --- Dequantize ---

	void FArcIrisReplicatedArrayNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::Dequantize"));
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
			return;
		}

		uint32 Stride = Config.ItemStride;
		uint8* TargetBase = reinterpret_cast<uint8*>(&Target) + Config.ItemsArrayOffset;
		FScriptArray& DstRawArray = *reinterpret_cast<FScriptArray*>(TargetBase);
		uint32 ItemAlign = alignof(FArcIrisReplicatedArrayItem);
		DstRawArray.Empty(0, Stride, ItemAlign);

		Target.PendingRemovals.Reset();
		for (int32 RemovedID : State.RemovedReplicationIDs)
		{
			Target.PendingRemovals.Add(RemovedID);
		}

		for (const TPair<int32, FQuantizedItem>& Pair : State.Baseline)
		{
			const FQuantizedItem& QItem = Pair.Value;
			int32 NewIdx = DstRawArray.AddZeroed(1, Stride, ItemAlign);
			uint8* ItemMemory = static_cast<uint8*>(DstRawArray.GetData()) + (NewIdx * Stride);
			DequantizeItem(Context, Descriptor, QItem.QuantizedState, ItemMemory);
		}

		Target.DirtyItems.Init(false, DstRawArray.Num());
	}

	// --- Validate ---

	bool FArcIrisReplicatedArrayNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
	{
		return true;
	}

	// --- Apply ---

	void FArcIrisReplicatedArrayNetSerializer::Apply(FNetSerializationContext& Context, const FNetApplyArgs& Args)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::Apply"));
		const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
		SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);
		const ConfigType& Config = *static_cast<const ConfigType*>(Args.NetSerializerConfig);

		uint32 Stride = Config.ItemStride;
		uint32 ArrayOffset = Config.ItemsArrayOffset;
		if (Stride == 0 || ArrayOffset == 0)
		{
			return;
		}

		const uint8* SrcArrayBase = reinterpret_cast<const uint8*>(&Source) + ArrayOffset;
		const FScriptArray& SrcRawArray = *reinterpret_cast<const FScriptArray*>(SrcArrayBase);
		int32 SrcItemCount = SrcRawArray.Num();
		const uint8* SrcData = static_cast<const uint8*>(SrcRawArray.GetData());

		uint8* DstArrayBase = reinterpret_cast<uint8*>(&Target) + ArrayOffset;
		FScriptArray& DstRawArray = *reinterpret_cast<FScriptArray*>(DstArrayBase);
		int32 DstItemCount = DstRawArray.Num();
		uint32 ItemAlign = alignof(FArcIrisReplicatedArrayItem);

		for (int32 RemovedID : Source.PendingRemovals)
		{
			int32 Idx = Target.FindIndexByIrisRepID(
				reinterpret_cast<const FArcIrisReplicatedArrayItem*>(DstRawArray.GetData()),
				DstItemCount, RemovedID);
			if (Idx != INDEX_NONE)
			{
				UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::Apply - PreReplicatedRemove RepID=%d Callback=%p"), RemovedID, Target.PreReplicatedRemoveCallback);
				if (Target.PreReplicatedRemoveCallback)
				{
					uint8* DstData = static_cast<uint8*>(DstRawArray.GetData());
					Target.PreReplicatedRemoveCallback(DstData + (Idx * Stride), Target);
				}

				int32 LastIdx = DstItemCount - 1;
				if (Idx != LastIdx)
				{
					uint8* DstData = static_cast<uint8*>(DstRawArray.GetData());
					FMemory::Memcpy(
						DstData + (Idx * Stride),
						DstData + (LastIdx * Stride),
						Stride);
				}
				DstRawArray.Remove(LastIdx, 1, Stride, ItemAlign);
				--DstItemCount;
			}
		}

		for (int32 SrcIdx = 0; SrcIdx < SrcItemCount; ++SrcIdx)
		{
			const uint8* SrcItemMemory = SrcData + (SrcIdx * Stride);
			const FArcIrisReplicatedArrayItem* SrcItemBase = reinterpret_cast<const FArcIrisReplicatedArrayItem*>(SrcItemMemory);
			int32 RepID = SrcItemBase->IrisRepID;

			int32 TargetIdx = Target.FindIndexByIrisRepID(
				reinterpret_cast<const FArcIrisReplicatedArrayItem*>(DstRawArray.GetData()),
				DstItemCount, RepID);

			if (TargetIdx == INDEX_NONE)
			{
				int32 NewIdx = DstRawArray.AddZeroed(1, Stride, ItemAlign);
				uint8* DstData = static_cast<uint8*>(DstRawArray.GetData());
				FMemory::Memcpy(DstData + (NewIdx * Stride), SrcItemMemory, Stride);
				++DstItemCount;

				UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::Apply - PostReplicatedAdd RepID=%d Callback=%p"), RepID, Target.PostReplicatedAddCallback);
				if (Target.PostReplicatedAddCallback)
				{
					Target.PostReplicatedAddCallback(DstData + (NewIdx * Stride), Target);
				}
			}
			else
			{
				uint8* DstData = static_cast<uint8*>(DstRawArray.GetData());
				uint8* DstItemMemory = DstData + (TargetIdx * Stride);
				const FArcIrisReplicatedArrayItem* DstItemBase = reinterpret_cast<const FArcIrisReplicatedArrayItem*>(DstItemMemory);
				if (DstItemBase->IrisRepKey != SrcItemBase->IrisRepKey)
				{
					FMemory::Memcpy(DstItemMemory, SrcItemMemory, Stride);

					UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::Apply - PostReplicatedChange RepID=%d Callback=%p"), RepID, Target.PostReplicatedChangeCallback);
					if (Target.PostReplicatedChangeCallback)
					{
						Target.PostReplicatedChangeCallback(DstItemMemory, Target);
					}
				}
			}
		}
	}

	// --- Dynamic state ---

	void FArcIrisReplicatedArrayNetSerializer::CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::CloneDynamicState"));
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
		NewState->Baseline = Source.State->Baseline;
		NewState->RemovedReplicationIDs = Source.State->RemovedReplicationIDs;
		NewState->ChangeVersion = Source.State->ChangeVersion;
		NewState->ReceivedDirtyIDs = Source.State->ReceivedDirtyIDs;
		NewState->ReceivedRemovedIDs = Source.State->ReceivedRemovedIDs;
		Target.State = NewState;
	}

	void FArcIrisReplicatedArrayNetSerializer::FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args)
	{
		UE_LOG(LogTemp, Warning, TEXT("ArcIrisReplicatedArray::FreeDynamicState"));
		QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Source);

		if (Value.State != nullptr)
		{
			const FReplicationStateDescriptor* Descriptor = Value.State->ItemDescriptor.GetReference();
			if (Descriptor != nullptr && EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
			{
				for (TPair<int32, FQuantizedItem>& Pair : Value.State->Baseline)
				{
					FreeQuantizedItemState(Descriptor, Pair.Value.QuantizedState);
				}
			}

			delete Value.State;
			Value.State = nullptr;
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

		const UScriptStruct* ConcreteStruct = nullptr;
		if (Property != nullptr)
		{
			const FStructProperty* StructProp = CastField<FStructProperty>(Property);
			if (StructProp != nullptr)
			{
				ConcreteStruct = StructProp->Struct;
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
		}

		UE_LOG(LogTemp, Warning, TEXT("FArcIrisReplicatedArrayPropertyNetSerializerInfo::BuildNetSerializerConfig Property=%s Struct=%s"),
			Property ? *Property->GetName() : TEXT("null"),
			ConcreteStruct ? *ConcreteStruct->GetName() : TEXT("null"));

		if (ConcreteStruct == nullptr)
		{
			return Config;
		}
		for (TFieldIterator<FArrayProperty> It(ConcreteStruct); It; ++It)
		{
			const FArrayProperty* ArrayProp = *It;
			const FStructProperty* InnerProp = CastField<FStructProperty>(ArrayProp->Inner);
			if (InnerProp == nullptr)
			{
				continue;
			}

			const UScriptStruct* ItemStruct = InnerProp->Struct;
			if (!ItemStruct->IsChildOf(FFastArraySerializerItem::StaticStruct()))
			{
				continue;
			}

			Config->ItemDescriptor = FReplicationStateDescriptorBuilder::CreateDescriptorForStruct(ItemStruct);
			Config->ItemsArrayOffset = static_cast<uint32>(ArrayProp->GetOffset_ForGC());
			Config->ItemStride = static_cast<uint32>(ItemStruct->GetStructureSize());
			break;
		}

		return Config;
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
		FPropertyNetSerializerInfoRegistry::Register(&ArcIrisReplicatedArraySerializerInfo);
	}

	void FArcIrisReplicatedArrayNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
	}

} // namespace ArcMassReplication
