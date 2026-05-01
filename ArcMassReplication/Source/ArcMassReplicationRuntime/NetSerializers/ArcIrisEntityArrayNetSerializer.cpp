// Copyright Lukasz Baran. All Rights Reserved.

#include "NetSerializers/ArcIrisEntityArrayNetSerializer.h"
#include "NetSerializers/ArcMassEntityNetDataStore.h"
#include "Replication/ArcMassEntityReplicationProxy.h"
#include "Subsystem/ArcMassEntityReplicationSubsystem.h"
#include "Fragments/ArcMassNetId.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamUtil.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetSerializerDelegates.h"
#include "Iris/Serialization/NetSerializers.h"

namespace ArcMassReplication
{
	using namespace UE::Net;

	namespace EntityArrayPrivate
	{
		UArcMassEntityReplicationSubsystem* GetSubsystem(FNetSerializationContext& Context)
		{
			FNetTokenStore* TokenStore = Context.GetNetTokenStore();
			if (TokenStore == nullptr)
			{
				return nullptr;
			}

			FArcMassEntityNetDataStore* DataStore = TokenStore->GetDataStore<FArcMassEntityNetDataStore>();
			if (DataStore == nullptr)
			{
				return nullptr;
			}

			return DataStore->GetSubsystem();
		}

		FArcIrisEntityArrayNetSerializer::FDynamicState* EnsureState(FArcIrisEntityArrayNetSerializer::QuantizedType& Target)
		{
			if (Target.State == nullptr)
			{
				Target.State = new FArcIrisEntityArrayNetSerializer::FDynamicState();
			}
			return Target.State;
		}
	} // namespace EntityArrayPrivate

	// --- Helpers ---

	const FArcMassReplicationDescriptorSet* FArcIrisEntityArrayNetSerializer::GetDescriptorSetFromContext(FNetSerializationContext& Context, uint32 Hash)
	{
		if (Hash == 0)
		{
			return nullptr;
		}

		UArcMassEntityReplicationSubsystem* Subsystem = EntityArrayPrivate::GetSubsystem(Context);
		if (Subsystem == nullptr)
		{
			return nullptr;
		}

		return Subsystem->FindDescriptorSetByHash(Hash);
	}

	bool FArcIrisEntityArrayNetSerializer::ShouldFilterEntityForConnection(FNetSerializationContext& Context, uint32 NetIdValue)
	{
		if (NetIdValue == 0)
		{
			return false;
		}

		uint32 ConnectionId = Context.GetLocalConnectionId();
		if (ConnectionId == 0)
		{
			return false;
		}

		UArcMassEntityReplicationSubsystem* Subsystem = EntityArrayPrivate::GetSubsystem(Context);
		if (Subsystem == nullptr)
		{
			return false;
		}

		FArcMassNetId NetId(NetIdValue);
		return !Subsystem->IsEntityRelevantToConnection(NetId, ConnectionId);
	}

	uint32 FArcIrisEntityArrayNetSerializer::FilterFragmentMaskForConnection(
		FNetSerializationContext& Context, uint32 NetIdValue, uint32 FragmentMask,
		const FArcMassReplicationDescriptorSet* DescSet)
	{
		if (DescSet == nullptr || FragmentMask == 0)
		{
			return FragmentMask;
		}

		uint32 ConnectionId = Context.GetLocalConnectionId();
		if (ConnectionId == 0)
		{
			return FragmentMask;
		}

		UArcMassEntityReplicationSubsystem* Subsystem = EntityArrayPrivate::GetSubsystem(Context);
		if (Subsystem == nullptr)
		{
			return FragmentMask;
		}

		FArcMassNetId NetId(NetIdValue);
		bool bIsOwner = Subsystem->IsConnectionOwnerOf(NetId, ConnectionId);
		if (bIsOwner)
		{
			return FragmentMask;
		}

		uint32 FilteredMask = FragmentMask;
		for (int32 SlotIdx = 0; SlotIdx < DescSet->FragmentFilters.Num(); ++SlotIdx)
		{
			if (DescSet->FragmentFilters[SlotIdx] == EArcMassReplicationFilter::OwnerOnly)
			{
				FilteredMask &= ~(1U << SlotIdx);
			}
		}

		return FilteredMask;
	}

	void FArcIrisEntityArrayNetSerializer::SerializeFragmentIris(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		const FInstancedStruct& Slot)
	{
		const uint8* SrcMemory = Slot.GetMemory();
		if (SrcMemory == nullptr || Descriptor == nullptr)
		{
			return;
		}

		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		uint32 InternalSize = Descriptor->InternalSize;
		uint32 InternalAlignment = Descriptor->InternalAlignment > 16U ? Descriptor->InternalAlignment : 16U;
		uint8* InternalBuffer = static_cast<uint8*>(FMemory::Malloc(InternalSize, InternalAlignment));
		FMemory::Memzero(InternalBuffer, InternalSize);

		FNetQuantizeArgs QuantizeArgs;
		QuantizeArgs.Version = 0;
		QuantizeArgs.NetSerializerConfig = &StructConfig;
		QuantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(SrcMemory);
		QuantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		StructSerializer->Quantize(Context, QuantizeArgs);

		FNetSerializeArgs SerializeArgs;
		SerializeArgs.Version = 0;
		SerializeArgs.NetSerializerConfig = &StructConfig;
		SerializeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		StructSerializer->Serialize(Context, SerializeArgs);

		if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
		{
			FNetFreeDynamicStateArgs FreeArgs;
			FreeArgs.Version = 0;
			FreeArgs.NetSerializerConfig = &StructConfig;
			FreeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
			StructSerializer->FreeDynamicState(Context, FreeArgs);
		}

		FMemory::Free(InternalBuffer);
	}

	void FArcIrisEntityArrayNetSerializer::DeserializeFragmentIris(
		FNetSerializationContext& Context,
		const FReplicationStateDescriptor* Descriptor,
		FInstancedStruct& Slot)
	{
		if (Descriptor == nullptr)
		{
			return;
		}

		const FNetSerializer* StructSerializer = &UE_NET_GET_SERIALIZER(FStructNetSerializer);
		FStructNetSerializerConfig StructConfig;
		StructConfig.StateDescriptor = Descriptor;

		uint32 InternalSize = Descriptor->InternalSize;
		uint32 InternalAlignment = Descriptor->InternalAlignment > 16U ? Descriptor->InternalAlignment : 16U;
		uint8* InternalBuffer = static_cast<uint8*>(FMemory::Malloc(InternalSize, InternalAlignment));
		FMemory::Memzero(InternalBuffer, InternalSize);

		FNetDeserializeArgs DeserializeArgs;
		DeserializeArgs.Version = 0;
		DeserializeArgs.NetSerializerConfig = &StructConfig;
		DeserializeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		StructSerializer->Deserialize(Context, DeserializeArgs);

		if (Context.HasErrorOrOverflow())
		{
			if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
			{
				FNetFreeDynamicStateArgs FreeArgs;
				FreeArgs.Version = 0;
				FreeArgs.NetSerializerConfig = &StructConfig;
				FreeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
				StructSerializer->FreeDynamicState(Context, FreeArgs);
			}
			FMemory::Free(InternalBuffer);
			return;
		}

		FNetDequantizeArgs DequantizeArgs;
		DequantizeArgs.Version = 0;
		DequantizeArgs.NetSerializerConfig = &StructConfig;
		DequantizeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
		DequantizeArgs.Target = reinterpret_cast<NetSerializerValuePointer>(Slot.GetMutableMemory());
		StructSerializer->Dequantize(Context, DequantizeArgs);

		if (EnumHasAnyFlags(Descriptor->Traits, EReplicationStateTraits::HasDynamicState))
		{
			FNetFreeDynamicStateArgs FreeArgs;
			FreeArgs.Version = 0;
			FreeArgs.NetSerializerConfig = &StructConfig;
			FreeArgs.Source = reinterpret_cast<NetSerializerValuePointer>(InternalBuffer);
			StructSerializer->FreeDynamicState(Context, FreeArgs);
		}

		FMemory::Free(InternalBuffer);
	}

	// --- Quantize: update baseline with dirty entities ---

	void FArcIrisEntityArrayNetSerializer::Quantize(FNetSerializationContext& Context, const FNetQuantizeArgs& Args)
	{
		const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		FDynamicState* State = EntityArrayPrivate::EnsureState(Target);

		const FArcMassReplicationDescriptorSet* DescSet = nullptr;
		if (Source.OwnerProxy)
		{
			DescSet = &Source.OwnerProxy->GetDescriptorSet();
		}

		State->DescriptorSetHash = DescSet ? DescSet->Hash : 0;

		bool bHasChanges = false;

		State->RemovedNetIds.Reset();
		for (const FArcMassNetId& RemovedId : Source.PendingRemovals)
		{
			uint32 NetIdValue = RemovedId.GetValue();
			State->Baseline.Remove(NetIdValue);
			State->RemovedNetIds.Add(NetIdValue);
			bHasChanges = true;
		}

		for (int32 EntityIdx = 0; EntityIdx < Source.Entities.Num(); ++EntityIdx)
		{
			if (!Source.DirtyEntities.IsValidIndex(EntityIdx) || !Source.DirtyEntities[EntityIdx])
			{
				continue;
			}

			const FArcIrisReplicatedEntity& SrcEntity = Source.Entities[EntityIdx];
			uint32 NetIdValue = SrcEntity.NetId.GetValue();

			FQuantizedEntity& QEntity = State->Baseline.FindOrAdd(NetIdValue);
			QEntity.NetIdValue = NetIdValue;
			QEntity.ReplicationKey = SrcEntity.ReplicationKey;
			QEntity.FragmentCount = static_cast<uint8>(SrcEntity.FragmentSlots.Num());
			QEntity.FragmentSlots.SetNum(QEntity.FragmentCount);

			for (int32 SlotIdx = 0; SlotIdx < SrcEntity.FragmentSlots.Num(); ++SlotIdx)
			{
				if ((SrcEntity.FragmentDirtyMask & (1U << SlotIdx)) == 0)
				{
					continue;
				}

				const FInstancedStruct& SrcSlot = SrcEntity.FragmentSlots[SlotIdx];
				if (!SrcSlot.IsValid())
				{
					continue;
				}

				const UScriptStruct* FragType = SrcSlot.GetScriptStruct();
				FInstancedStruct& DstSlot = QEntity.FragmentSlots[SlotIdx];
				if (!DstSlot.IsValid() || DstSlot.GetScriptStruct() != FragType)
				{
					DstSlot.InitializeAs(FragType);
				}
				FragType->CopyScriptStruct(DstSlot.GetMutableMemory(), SrcSlot.GetMemory());
			}

			bHasChanges = true;
		}

		if (bHasChanges)
		{
			++State->ChangeVersion;
		}

		const_cast<SourceType&>(Source).ClearDirtyState();
	}

	// --- IsEqual ---

	bool FArcIrisEntityArrayNetSerializer::IsEqual(FNetSerializationContext& Context, const FNetIsEqualArgs& Args)
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

	// --- Dequantize: convert received delta into source representation for Apply ---

	void FArcIrisEntityArrayNetSerializer::Dequantize(FNetSerializationContext& Context, const FNetDequantizeArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);

		if (Source.State == nullptr)
		{
			return;
		}

		const FDynamicState& State = *Source.State;

		Target.PendingRemovals.Reset();
		for (uint32 RemovedNetIdValue : State.ReceivedRemovedNetIds)
		{
			Target.PendingRemovals.Add(FArcMassNetId(RemovedNetIdValue));
		}

		Target.Entities.Reset();
		for (uint32 DirtyNetId : State.ReceivedDirtyNetIds)
		{
			const FQuantizedEntity* QEntity = State.Baseline.Find(DirtyNetId);
			if (QEntity == nullptr)
			{
				continue;
			}

			FArcIrisReplicatedEntity& NewEntity = Target.Entities.AddDefaulted_GetRef();
			NewEntity.NetId = FArcMassNetId(QEntity->NetIdValue);
			NewEntity.ReplicationKey = QEntity->ReplicationKey;
			NewEntity.FragmentDirtyMask = (1U << QEntity->FragmentCount) - 1U;
			NewEntity.FragmentSlots = QEntity->FragmentSlots;
		}

		Target.DirtyEntities.Init(true, Target.Entities.Num());
	}

	// --- Serialize: full state (all baseline entities) ---

	void FArcIrisEntityArrayNetSerializer::Serialize(FNetSerializationContext& Context, const FNetSerializeArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

		if (Source.State == nullptr)
		{
			Writer->WriteBits(0U, 32U);
			Writer->WriteBits(0U, 8U);
			Writer->WriteBits(0U, 16U);
			return;
		}

		const FDynamicState& State = *Source.State;
		const FArcMassReplicationDescriptorSet* DescSet = GetDescriptorSetFromContext(Context, State.DescriptorSetHash);

		Writer->WriteBits(State.DescriptorSetHash, 32U);

		Writer->WriteBits(0U, 8U);

		TArray<const FQuantizedEntity*> PassingEntities;
		for (const TPair<uint32, FQuantizedEntity>& Pair : State.Baseline)
		{
			if (!ShouldFilterEntityForConnection(Context, Pair.Key))
			{
				PassingEntities.Add(&Pair.Value);
			}
		}

		uint16 EntityCount = static_cast<uint16>(PassingEntities.Num());
		Writer->WriteBits(static_cast<uint32>(EntityCount), 16U);

		for (const FQuantizedEntity* QEntity : PassingEntities)
		{
			uint32 FilteredMask = FilterFragmentMaskForConnection(Context, QEntity->NetIdValue,
				(1U << QEntity->FragmentCount) - 1U, DescSet);

			Writer->WriteBits(QEntity->NetIdValue, 32U);
			Writer->WriteBits(QEntity->ReplicationKey, 32U);
			Writer->WriteBits(FilteredMask, 32U);

			for (int32 SlotIdx = 0; SlotIdx < QEntity->FragmentCount; ++SlotIdx)
			{
				if ((FilteredMask & (1U << SlotIdx)) == 0)
				{
					continue;
				}

				if (DescSet && DescSet->Descriptors.IsValidIndex(SlotIdx) && DescSet->Descriptors[SlotIdx].IsValid()
					&& QEntity->FragmentSlots.IsValidIndex(SlotIdx) && QEntity->FragmentSlots[SlotIdx].IsValid())
				{
					SerializeFragmentIris(Context, DescSet->Descriptors[SlotIdx].GetReference(), QEntity->FragmentSlots[SlotIdx]);
				}
			}
		}
	}

	// --- Deserialize (full state) ---

	void FArcIrisEntityArrayNetSerializer::Deserialize(FNetSerializationContext& Context, const FNetDeserializeArgs& Args)
	{
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		FDynamicState* State = EntityArrayPrivate::EnsureState(Target);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		State->DescriptorSetHash = Reader->ReadBits(32U);
		State->Baseline.Reset();
		State->ReceivedDirtyNetIds.Reset();
		State->ReceivedRemovedNetIds.Reset();

		const FArcMassReplicationDescriptorSet* DescSet = GetDescriptorSetFromContext(Context, State->DescriptorSetHash);

		uint32 RemovedCount = Reader->ReadBits(8U);
		for (uint32 Idx = 0; Idx < RemovedCount; ++Idx)
		{
			State->ReceivedRemovedNetIds.Add(Reader->ReadBits(32U));
		}

		uint32 EntityCount = Reader->ReadBits(16U);
		for (uint32 EntityIdx = 0; EntityIdx < EntityCount; ++EntityIdx)
		{
			uint32 NetIdValue = Reader->ReadBits(32U);
			uint32 RepKey = Reader->ReadBits(32U);
			uint32 FragmentMask = Reader->ReadBits(32U);

			FQuantizedEntity& QEntity = State->Baseline.Add(NetIdValue);
			QEntity.NetIdValue = NetIdValue;
			QEntity.ReplicationKey = RepKey;

			int32 MaxSlot = 0;
			uint32 Mask = FragmentMask;
			while (Mask != 0)
			{
				int32 Bit = FMath::CountTrailingZeros(Mask);
				MaxSlot = FMath::Max(MaxSlot, Bit + 1);
				Mask &= ~(1U << Bit);
			}

			QEntity.FragmentCount = static_cast<uint8>(MaxSlot);
			QEntity.FragmentSlots.SetNum(MaxSlot);

			for (int32 SlotIdx = 0; SlotIdx < MaxSlot; ++SlotIdx)
			{
				if ((FragmentMask & (1U << SlotIdx)) == 0)
				{
					continue;
				}

				if (DescSet && DescSet->Descriptors.IsValidIndex(SlotIdx) && DescSet->Descriptors[SlotIdx].IsValid()
					&& DescSet->FragmentTypes.IsValidIndex(SlotIdx))
				{
					FInstancedStruct& DstSlot = QEntity.FragmentSlots[SlotIdx];
					if (!DstSlot.IsValid() || DstSlot.GetScriptStruct() != DescSet->FragmentTypes[SlotIdx])
					{
						DstSlot.InitializeAs(DescSet->FragmentTypes[SlotIdx]);
					}
					DeserializeFragmentIris(Context, DescSet->Descriptors[SlotIdx].GetReference(), DstSlot);

					if (Context.HasErrorOrOverflow())
					{
						return;
					}
				}
			}

			State->ReceivedDirtyNetIds.Add(NetIdValue);
		}

		++State->ChangeVersion;
	}

	// --- SerializeDelta: diff current vs prev baseline ---

	void FArcIrisEntityArrayNetSerializer::SerializeDelta(FNetSerializationContext& Context, const FNetSerializeDeltaArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		const QuantizedType& Prev = *reinterpret_cast<const QuantizedType*>(Args.Prev);
		FNetBitStreamWriter* Writer = Context.GetBitStreamWriter();

		const FDynamicState* CurrentState = Source.State;
		const FDynamicState* PrevState = Prev.State;

		uint32 DescHash = CurrentState ? CurrentState->DescriptorSetHash : 0;
		const FArcMassReplicationDescriptorSet* DescSet = GetDescriptorSetFromContext(Context, DescHash);

		Writer->WriteBits(DescHash, 32U);

		uint8 RemovedCount = CurrentState ? static_cast<uint8>(FMath::Min(CurrentState->RemovedNetIds.Num(), 255)) : 0;
		Writer->WriteBits(static_cast<uint32>(RemovedCount), 8U);
		if (CurrentState)
		{
			for (int32 Idx = 0; Idx < RemovedCount; ++Idx)
			{
				Writer->WriteBits(CurrentState->RemovedNetIds[Idx], 32U);
			}
		}

		TArray<const FQuantizedEntity*> DirtyEntities;
		if (CurrentState)
		{
			for (const TPair<uint32, FQuantizedEntity>& CurPair : CurrentState->Baseline)
			{
				if (ShouldFilterEntityForConnection(Context, CurPair.Key))
				{
					continue;
				}

				bool bIsDirty = false;
				if (!PrevState || !PrevState->Baseline.Contains(CurPair.Key))
				{
					bIsDirty = true;
				}
				else
				{
					const FQuantizedEntity& PrevEntity = PrevState->Baseline.FindChecked(CurPair.Key);
					if (PrevEntity.ReplicationKey != CurPair.Value.ReplicationKey)
					{
						bIsDirty = true;
					}
				}

				if (bIsDirty)
				{
					DirtyEntities.Add(&CurPair.Value);
				}
			}
		}

		uint16 DirtyCount = static_cast<uint16>(DirtyEntities.Num());
		Writer->WriteBits(static_cast<uint32>(DirtyCount), 16U);

		for (const FQuantizedEntity* QEntity : DirtyEntities)
		{
			uint32 FilteredMask = FilterFragmentMaskForConnection(Context, QEntity->NetIdValue,
				(1U << QEntity->FragmentCount) - 1U, DescSet);

			Writer->WriteBits(QEntity->NetIdValue, 32U);
			Writer->WriteBits(QEntity->ReplicationKey, 32U);
			Writer->WriteBits(FilteredMask, 32U);

			for (int32 SlotIdx = 0; SlotIdx < QEntity->FragmentCount; ++SlotIdx)
			{
				if ((FilteredMask & (1U << SlotIdx)) == 0)
				{
					continue;
				}

				if (DescSet && DescSet->Descriptors.IsValidIndex(SlotIdx) && DescSet->Descriptors[SlotIdx].IsValid()
					&& QEntity->FragmentSlots.IsValidIndex(SlotIdx) && QEntity->FragmentSlots[SlotIdx].IsValid())
				{
					SerializeFragmentIris(Context, DescSet->Descriptors[SlotIdx].GetReference(), QEntity->FragmentSlots[SlotIdx]);
				}
			}
		}
	}

	// --- DeserializeDelta: merge incoming delta into baseline ---

	void FArcIrisEntityArrayNetSerializer::DeserializeDelta(FNetSerializationContext& Context, const FNetDeserializeDeltaArgs& Args)
	{
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);
		const QuantizedType& Prev = *reinterpret_cast<const QuantizedType*>(Args.Prev);

		FDynamicState* State = EntityArrayPrivate::EnsureState(Target);
		FNetBitStreamReader* Reader = Context.GetBitStreamReader();

		if (Prev.State)
		{
			State->Baseline = Prev.State->Baseline;
		}

		State->DescriptorSetHash = Reader->ReadBits(32U);
		State->ReceivedDirtyNetIds.Reset();
		State->ReceivedRemovedNetIds.Reset();

		const FArcMassReplicationDescriptorSet* DescSet = GetDescriptorSetFromContext(Context, State->DescriptorSetHash);

		uint32 RemovedCount = Reader->ReadBits(8U);
		for (uint32 Idx = 0; Idx < RemovedCount; ++Idx)
		{
			uint32 RemovedNetId = Reader->ReadBits(32U);
			State->Baseline.Remove(RemovedNetId);
			State->ReceivedRemovedNetIds.Add(RemovedNetId);
		}

		uint32 DirtyCount = Reader->ReadBits(16U);
		for (uint32 EntityIdx = 0; EntityIdx < DirtyCount; ++EntityIdx)
		{
			uint32 NetIdValue = Reader->ReadBits(32U);
			uint32 RepKey = Reader->ReadBits(32U);
			uint32 FragmentMask = Reader->ReadBits(32U);

			FQuantizedEntity& QEntity = State->Baseline.FindOrAdd(NetIdValue);
			QEntity.NetIdValue = NetIdValue;
			QEntity.ReplicationKey = RepKey;

			int32 MaxSlot = 0;
			uint32 Mask = FragmentMask;
			while (Mask != 0)
			{
				int32 Bit = FMath::CountTrailingZeros(Mask);
				MaxSlot = FMath::Max(MaxSlot, Bit + 1);
				Mask &= ~(1U << Bit);
			}

			if (QEntity.FragmentCount < static_cast<uint8>(MaxSlot))
			{
				QEntity.FragmentCount = static_cast<uint8>(MaxSlot);
			}
			QEntity.FragmentSlots.SetNum(FMath::Max(QEntity.FragmentSlots.Num(), MaxSlot));

			for (int32 SlotIdx = 0; SlotIdx < MaxSlot; ++SlotIdx)
			{
				if ((FragmentMask & (1U << SlotIdx)) == 0)
				{
					continue;
				}

				if (DescSet && DescSet->Descriptors.IsValidIndex(SlotIdx) && DescSet->Descriptors[SlotIdx].IsValid()
					&& DescSet->FragmentTypes.IsValidIndex(SlotIdx))
				{
					FInstancedStruct& DstSlot = QEntity.FragmentSlots[SlotIdx];
					if (!DstSlot.IsValid() || DstSlot.GetScriptStruct() != DescSet->FragmentTypes[SlotIdx])
					{
						DstSlot.InitializeAs(DescSet->FragmentTypes[SlotIdx]);
					}
					DeserializeFragmentIris(Context, DescSet->Descriptors[SlotIdx].GetReference(), DstSlot);

					if (Context.HasErrorOrOverflow())
					{
						return;
					}
				}
			}

			State->ReceivedDirtyNetIds.Add(NetIdValue);
		}

		++State->ChangeVersion;
	}

	// --- Validate ---

	bool FArcIrisEntityArrayNetSerializer::Validate(FNetSerializationContext& Context, const FNetValidateArgs& Args)
	{
		return true;
	}

	// --- Apply: fires callbacks on the actual actor property ---

	void FArcIrisEntityArrayNetSerializer::Apply(FNetSerializationContext& Context, const FNetApplyArgs& Args)
	{
		const SourceType& Source = *reinterpret_cast<const SourceType*>(Args.Source);
		SourceType& Target = *reinterpret_cast<SourceType*>(Args.Target);

		const FArcMassReplicationDescriptorSet* DescSet = nullptr;
		if (Target.OwnerProxy)
		{
			DescSet = &Target.OwnerProxy->GetDescriptorSet();
		}

		for (const FArcMassNetId& RemovedNetId : Source.PendingRemovals)
		{
			Target.PreReplicatedRemove(RemovedNetId);
			int32 Idx = Target.FindIndexByNetId(RemovedNetId);
			if (Idx != INDEX_NONE)
			{
				Target.Entities.RemoveAtSwap(Idx);
			}
		}

		for (int32 TargetIdx = Target.Entities.Num() - 1; TargetIdx >= 0; --TargetIdx)
		{
			FArcMassNetId NetId = Target.Entities[TargetIdx].NetId;
			if (Source.FindIndexByNetId(NetId) == INDEX_NONE
				&& Source.PendingRemovals.FindByPredicate([NetId](const FArcMassNetId& R) { return R == NetId; }) == nullptr)
			{
				Target.PreReplicatedRemove(NetId);
				Target.Entities.RemoveAtSwap(TargetIdx);
			}
		}

		for (const FArcIrisReplicatedEntity& SrcEntity : Source.Entities)
		{
			int32 TargetIdx = Target.FindIndexByNetId(SrcEntity.NetId);
			bool bIsNew = (TargetIdx == INDEX_NONE);

			if (bIsNew && DescSet)
			{
				TargetIdx = Target.AddEntity(SrcEntity.NetId, DescSet->FragmentTypes);
			}

			if (TargetIdx == INDEX_NONE)
			{
				continue;
			}

			FArcIrisReplicatedEntity& TargetEntity = Target.Entities[TargetIdx];

			if (!bIsNew && TargetEntity.ReplicationKey == SrcEntity.ReplicationKey)
			{
				continue;
			}

			TargetEntity.ReplicationKey = SrcEntity.ReplicationKey;
			uint32 AppliedMask = 0;

			for (int32 SlotIdx = 0; SlotIdx < SrcEntity.FragmentSlots.Num(); ++SlotIdx)
			{
				if ((SrcEntity.FragmentDirtyMask & (1U << SlotIdx)) == 0)
				{
					continue;
				}

				const FInstancedStruct& SrcSlot = SrcEntity.FragmentSlots[SlotIdx];
				if (!SrcSlot.IsValid())
				{
					continue;
				}

				if (TargetEntity.FragmentSlots.IsValidIndex(SlotIdx))
				{
					FInstancedStruct& DstSlot = TargetEntity.FragmentSlots[SlotIdx];
					if (DstSlot.IsValid() && DstSlot.GetScriptStruct() == SrcSlot.GetScriptStruct())
					{
						SrcSlot.GetScriptStruct()->CopyScriptStruct(DstSlot.GetMutableMemory(), SrcSlot.GetMemory());
						AppliedMask |= (1U << SlotIdx);
					}
				}
			}

			if (bIsNew)
			{
				Target.PostReplicatedAdd(SrcEntity.NetId);
			}
			else if (AppliedMask != 0)
			{
				Target.PostReplicatedChange(SrcEntity.NetId, AppliedMask);
			}
		}
	}

	// --- Dynamic state ---

	void FArcIrisEntityArrayNetSerializer::CloneDynamicState(FNetSerializationContext& Context, const FNetCloneDynamicStateArgs& Args)
	{
		const QuantizedType& Source = *reinterpret_cast<const QuantizedType*>(Args.Source);
		QuantizedType& Target = *reinterpret_cast<QuantizedType*>(Args.Target);

		if (Source.State == nullptr)
		{
			Target.State = nullptr;
			return;
		}

		FDynamicState* NewState = new FDynamicState();
		NewState->DescriptorSetHash = Source.State->DescriptorSetHash;
		NewState->Baseline = Source.State->Baseline;
		NewState->RemovedNetIds = Source.State->RemovedNetIds;
		NewState->ChangeVersion = Source.State->ChangeVersion;
		NewState->ReceivedDirtyNetIds = Source.State->ReceivedDirtyNetIds;
		NewState->ReceivedRemovedNetIds = Source.State->ReceivedRemovedNetIds;
		Target.State = NewState;
	}

	void FArcIrisEntityArrayNetSerializer::FreeDynamicState(FNetSerializationContext& Context, const FNetFreeDynamicStateArgs& Args)
	{
		QuantizedType& Value = *reinterpret_cast<QuantizedType*>(Args.Source);
		delete Value.State;
		Value.State = nullptr;
	}

	// --- Registration ---

	UE_NET_IMPLEMENT_SERIALIZER(FArcIrisEntityArrayNetSerializer);
	FArcIrisEntityArrayNetSerializer::ConfigType FArcIrisEntityArrayNetSerializer::DefaultConfig = FArcIrisEntityArrayNetSerializer::ConfigType();

	FArcIrisEntityArrayNetSerializer::FNetSerializerRegistryDelegates FArcIrisEntityArrayNetSerializer::NetSerializerRegistryDelegates;
	static const FName PropertyNetSerializerRegistry_NAME_ArcIrisEntityArray(TEXT("ArcIrisEntityArray"));
	UE_NET_IMPLEMENT_NAMED_STRUCT_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcIrisEntityArray, FArcIrisEntityArrayNetSerializer);

	FArcIrisEntityArrayNetSerializer::FNetSerializerRegistryDelegates::~FNetSerializerRegistryDelegates()
	{
		UE_NET_UNREGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcIrisEntityArray);
	}

	void FArcIrisEntityArrayNetSerializer::FNetSerializerRegistryDelegates::OnPreFreezeNetSerializerRegistry()
	{
		UE_NET_REGISTER_NETSERIALIZER_INFO(PropertyNetSerializerRegistry_NAME_ArcIrisEntityArray);
	}

	void FArcIrisEntityArrayNetSerializer::FNetSerializerRegistryDelegates::OnPostFreezeNetSerializerRegistry()
	{
	}
} // namespace ArcMassReplication
