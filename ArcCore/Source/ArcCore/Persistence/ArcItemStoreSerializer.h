// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class FArcSaveArchive;
class FArcLoadArchive;
struct FArcItemCopyContainerHelper;
struct FArcAttachedItemHelper;
struct FArcItemSpec;

/**
 * Custom persistence serializer for item store contents.
 * Serializes TArray<FArcItemCopyContainerHelper> — the output of
 * UArcItemsStoreComponent::GetAllInternalItems().
 *
 * Used directly (not through the serializer registry) because the
 * source type is a TArray, not a single UStruct.
 */
struct ARCCORE_API FArcItemStoreSerializer
{
	static constexpr uint32 Version = 2;

	/** Serialize an array of item copy helpers into the archive. */
	static void Save(const TArray<FArcItemCopyContainerHelper>& Source, FArcSaveArchive& Ar);

	/** Deserialize an array of item copy helpers from the archive. */
	static void Load(TArray<FArcItemCopyContainerHelper>& Target, FArcLoadArchive& Ar);

	static void SaveItemSpec(const FArcItemSpec& Spec, const FGameplayTag& SlotId, FArcSaveArchive& Ar);
	static void LoadItemSpec(FArcItemSpec& OutSpec, FGameplayTag& OutSlotId, FArcLoadArchive& Ar);

private:
	static void SavePersistentInstances(const FArcItemSpec& Spec, FArcSaveArchive& Ar);
	static void LoadPersistentInstances(FArcItemSpec& Spec, FArcLoadArchive& Ar);

	static void SaveInstanceData(const FArcItemSpec& Spec, FArcSaveArchive& Ar);
	static void LoadInstanceData(FArcItemSpec& Spec, FArcLoadArchive& Ar);

	static void SaveAttachments(const TArray<FArcAttachedItemHelper>& Attachments, FArcSaveArchive& Ar);
	static void LoadAttachments(TArray<FArcAttachedItemHelper>& OutAttachments, FArcLoadArchive& Ar);
};
