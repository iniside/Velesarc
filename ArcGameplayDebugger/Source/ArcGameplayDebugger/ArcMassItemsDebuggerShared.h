// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "UObject/PrimaryAssetId.h"

class UScriptStruct;
struct FArcItemData;
struct FArcItemId;
struct FArcMassItemStoreFragment;
struct FMassEntityManager;

namespace ArcMassItemsDebugger
{
	UWorld* GetDebugWorld();
	FMassEntityManager* GetEntityManager();

	FArcMassItemStoreFragment* GetStore(FMassEntityManager& Manager, FMassEntityHandle Entity);

	FString FormatGameplayTag(const FGameplayTag& Tag);
	FString FormatItemId(const FArcItemId& ItemId);
	FString FormatItemDefinition(const FPrimaryAssetId& ItemDefinitionId);
	const FArcItemData* GetItemAt(const FArcMassItemStoreFragment& Store, int32 Index);

	void DrawStructProperties(const UScriptStruct* StructType, const void* StructData, const char* TableIdPrefix);
	void DrawTags(const char* Header, const FGameplayTagContainer& Tags);
}

// Cross-tab navigation request, written by one tab and consumed at the start of the next Draw().
enum class EArcMassItemsDebuggerJumpTarget : uint8
{
	None,
	ItemsTab
};
