// Copyright Lukasz Baran. All Rights Reserved.

#include "Persistence/ArcItemStoreSerializer.h"

#include "Items/ArcItemsArray.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemInstance.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "Serialization/ArcSaveArchive.h"
#include "Serialization/ArcLoadArchive.h"
#include "Serialization/ArcReflectionSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcItemStoreSerializer, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Save
// ─────────────────────────────────────────────────────────────────────────────

void FArcItemStoreSerializer::Save(const TArray<FArcItemCopyContainerHelper>& Source, FArcSaveArchive& Ar)
{
	Ar.SetVersion(Version);
	Ar.WriteProperty(FName(TEXT("Version")), Version);

	Ar.BeginArray(FName(TEXT("Items")), Source.Num());
	for (int32 i = 0; i < Source.Num(); ++i)
	{
		const FArcItemCopyContainerHelper& Helper = Source[i];

		Ar.BeginArrayElement(i);

		SaveItemSpec(Helper.Item, Helper.SlotId, Ar);
		SaveAttachments(Helper.ItemAttachments, Ar);

		Ar.EndArrayElement();
	}
	Ar.EndArray();
}

void FArcItemStoreSerializer::SaveItemSpec(const FArcItemSpec& Spec, const FGameplayTag& SlotId, FArcSaveArchive& Ar)
{
	Ar.WriteProperty(FName(TEXT("ItemId")), Spec.ItemId.GetGuid());
	Ar.WriteProperty(FName(TEXT("ItemDefinitionId")), Spec.GetItemDefinitionId().ToString());
	Ar.WriteProperty(FName(TEXT("Amount")), static_cast<uint16>(Spec.Amount));
	Ar.WriteProperty(FName(TEXT("Level")), static_cast<uint8>(Spec.Level));
	Ar.WriteProperty(FName(TEXT("SlotId")), SlotId);

	SavePersistentInstances(Spec, Ar);
	SaveInstanceData(Spec, Ar);
}

void FArcItemStoreSerializer::SavePersistentInstances(const FArcItemSpec& Spec, FArcSaveArchive& Ar)
{
	// Collect all valid persistent instances first to get an accurate count
	TArray<const FArcItemInstance*> ToWrite;
	for (const TSharedPtr<FArcItemInstance>& Instance : Spec.InitialInstanceData)
	{
		if (Instance.IsValid() && Instance->ShouldPersist() && Instance->GetScriptStruct())
		{
			ToWrite.Add(Instance.Get());
		}
	}

	Ar.BeginArray(FName(TEXT("Instances")), ToWrite.Num());
	for (int32 i = 0; i < ToWrite.Num(); ++i)
	{
		const FArcItemInstance* Instance = ToWrite[i];
		UScriptStruct* StructType = Instance->GetScriptStruct();

		Ar.BeginArrayElement(i);
		Ar.WriteProperty(FName(TEXT("StructType")), StructType->GetPathName());

		Ar.BeginStruct(FName(TEXT("Data")));
		FArcReflectionSerializer::Save(StructType, Instance, Ar);
		Ar.EndStruct();

		Ar.EndArrayElement();
	}
	Ar.EndArray();
}

void FArcItemStoreSerializer::SaveInstanceData(const FArcItemSpec& Spec, FArcSaveArchive& Ar)
{
	TArray<const FArcItemFragment_ItemInstanceBase*> Fragments;
	for (int32 i = 0; i < Spec.InstanceData.Num(); ++i)
	{
		const FArcItemFragment_ItemInstanceBase* Fragment = Spec.InstanceData.Get(i);
		if (Fragment && Fragment->GetScriptStruct())
		{
			Fragments.Add(Fragment);
		}
	}

	Ar.BeginArray(FName(TEXT("InstanceData")), Fragments.Num());
	for (int32 i = 0; i < Fragments.Num(); ++i)
	{
		const FArcItemFragment_ItemInstanceBase* Fragment = Fragments[i];
		UScriptStruct* StructType = Fragment->GetScriptStruct();

		Ar.BeginArrayElement(i);
		Ar.WriteProperty(FName(TEXT("StructType")), StructType->GetPathName());

		Ar.BeginStruct(FName(TEXT("Data")));
		FArcReflectionSerializer::Save(StructType, Fragment, Ar);
		Ar.EndStruct();

		Ar.EndArrayElement();
	}
	Ar.EndArray();
}

void FArcItemStoreSerializer::SaveAttachments(const TArray<FArcAttachedItemHelper>& Attachments, FArcSaveArchive& Ar)
{
	Ar.BeginArray(FName(TEXT("Attachments")), Attachments.Num());
	for (int32 i = 0; i < Attachments.Num(); ++i)
	{
		const FArcAttachedItemHelper& Attached = Attachments[i];

		Ar.BeginArrayElement(i);

		SaveItemSpec(Attached.Item, Attached.SlotId, Ar);
		// Attachments are flat (one level deep), no recursive attachments.

		Ar.EndArrayElement();
	}
	Ar.EndArray();
}

// ─────────────────────────────────────────────────────────────────────────────
// Load
// ─────────────────────────────────────────────────────────────────────────────

void FArcItemStoreSerializer::Load(TArray<FArcItemCopyContainerHelper>& Target, FArcLoadArchive& Ar)
{
	uint32 FileVersion = 0;
	Ar.ReadProperty(FName(TEXT("Version")), FileVersion);

	int32 ItemCount = 0;
	if (!Ar.BeginArray(FName(TEXT("Items")), ItemCount))
	{
		return;
	}

	Target.Reserve(ItemCount);
	for (int32 i = 0; i < ItemCount; ++i)
	{
		if (!Ar.BeginArrayElement(i))
		{
			continue;
		}

		FArcItemCopyContainerHelper Helper;
		LoadItemSpec(Helper.Item, Helper.SlotId, Ar);
		Helper.ItemId = Helper.Item.ItemId;
		LoadAttachments(Helper.ItemAttachments, Ar);

		Target.Add(MoveTemp(Helper));
		Ar.EndArrayElement();
	}
	Ar.EndArray();
}

void FArcItemStoreSerializer::LoadItemSpec(FArcItemSpec& OutSpec, FGameplayTag& OutSlotId, FArcLoadArchive& Ar)
{
	FGuid ItemGuid;
	if (Ar.ReadProperty(FName(TEXT("ItemId")), ItemGuid))
	{
		OutSpec.ItemId = FArcItemId(ItemGuid);
	}

	FString DefinitionStr;
	if (Ar.ReadProperty(FName(TEXT("ItemDefinitionId")), DefinitionStr))
	{
		OutSpec.ItemDefinitionId = FPrimaryAssetId::FromString(DefinitionStr);
	}

	uint16 Amount = 1;
	if (Ar.ReadProperty(FName(TEXT("Amount")), Amount))
	{
		OutSpec.Amount = Amount;
	}

	uint8 Level = 1;
	if (Ar.ReadProperty(FName(TEXT("Level")), Level))
	{
		OutSpec.Level = Level;
	}

	Ar.ReadProperty(FName(TEXT("SlotId")), OutSlotId);

	LoadPersistentInstances(OutSpec, Ar);
	LoadInstanceData(OutSpec, Ar);
}

void FArcItemStoreSerializer::LoadPersistentInstances(FArcItemSpec& Spec, FArcLoadArchive& Ar)
{
	int32 InstanceCount = 0;
	if (!Ar.BeginArray(FName(TEXT("Instances")), InstanceCount))
	{
		return;
	}

	for (int32 i = 0; i < InstanceCount; ++i)
	{
		if (!Ar.BeginArrayElement(i))
		{
			continue;
		}

		FString StructTypePath;
		if (!Ar.ReadProperty(FName(TEXT("StructType")), StructTypePath))
		{
			UE_LOG(LogArcItemStoreSerializer, Warning, TEXT("Missing StructType for instance %d, skipping"), i);
			Ar.EndArrayElement();
			continue;
		}

		UScriptStruct* StructType = FindObject<UScriptStruct>(nullptr, *StructTypePath);
		if (!StructType)
		{
			UE_LOG(LogArcItemStoreSerializer, Warning, TEXT("Could not resolve struct type '%s', skipping instance %d"), *StructTypePath, i);
			Ar.EndArrayElement();
			continue;
		}

		TSharedPtr<FArcItemInstance> Instance = ArcItems::AllocateInstance(StructType);
		if (Instance.IsValid())
		{
			if (Ar.BeginStruct(FName(TEXT("Data"))))
			{
				FArcReflectionSerializer::Load(StructType, Instance.Get(), Ar);
				Ar.EndStruct();
			}

			Spec.InitialInstanceData.Add(MoveTemp(Instance));
		}

		Ar.EndArrayElement();
	}
	Ar.EndArray();
}

void FArcItemStoreSerializer::LoadInstanceData(FArcItemSpec& Spec, FArcLoadArchive& Ar)
{
	int32 FragmentCount = 0;
	if (!Ar.BeginArray(FName(TEXT("InstanceData")), FragmentCount))
	{
		return;
	}

	for (int32 i = 0; i < FragmentCount; ++i)
	{
		if (!Ar.BeginArrayElement(i))
		{
			continue;
		}

		FString StructTypePath;
		if (!Ar.ReadProperty(FName(TEXT("StructType")), StructTypePath))
		{
			UE_LOG(LogArcItemStoreSerializer, Warning, TEXT("Missing StructType for InstanceData %d, skipping"), i);
			Ar.EndArrayElement();
			continue;
		}

		UScriptStruct* StructType = FindObject<UScriptStruct>(nullptr, *StructTypePath);
		if (!StructType)
		{
			UE_LOG(LogArcItemStoreSerializer, Warning, TEXT("Could not resolve struct type '%s', skipping InstanceData %d"), *StructTypePath, i);
			Ar.EndArrayElement();
			continue;
		}

		void* Mem = FMemory::Malloc(StructType->GetCppStructOps()->GetSize(), StructType->GetCppStructOps()->GetAlignment());
		StructType->GetCppStructOps()->Construct(Mem);
		auto* Fragment = static_cast<FArcItemFragment_ItemInstanceBase*>(Mem);

		if (Ar.BeginStruct(FName(TEXT("Data"))))
		{
			FArcReflectionSerializer::Load(StructType, Fragment, Ar);
			Ar.EndStruct();
		}

		TSharedPtr<FArcItemFragment_ItemInstanceBase> SharedPtr(Fragment, [StructType](FArcItemFragment_ItemInstanceBase* Ptr)
		{
			StructType->GetCppStructOps()->Destruct(Ptr);
			FMemory::Free(Ptr);
		});
		Spec.InstanceData.Add(MoveTemp(SharedPtr));

		Ar.EndArrayElement();
	}
	Ar.EndArray();
}

void FArcItemStoreSerializer::LoadAttachments(TArray<FArcAttachedItemHelper>& OutAttachments, FArcLoadArchive& Ar)
{
	int32 AttachmentCount = 0;
	if (!Ar.BeginArray(FName(TEXT("Attachments")), AttachmentCount))
	{
		return;
	}

	OutAttachments.Reserve(AttachmentCount);
	for (int32 i = 0; i < AttachmentCount; ++i)
	{
		if (!Ar.BeginArrayElement(i))
		{
			continue;
		}

		FArcAttachedItemHelper Attached;
		LoadItemSpec(Attached.Item, Attached.SlotId, Ar);
		Attached.ItemId = Attached.Item.ItemId;

		OutAttachments.Add(MoveTemp(Attached));
		Ar.EndArrayElement();
	}
	Ar.EndArray();
}
