// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassItemsDebuggerShared.h"

#include "Core/ArcCoreAssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "GameFramework/PlayerController.h"
#include "Items/ArcItemData.h"
#include "MassEntitySubsystem.h"
#include "Misc/Paths.h"
#include "Mass/EntityView.h"
#include "imgui.h"

namespace ArcMassItemsDebugger
{
	UWorld* GetDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	FMassEntityManager* GetEntityManager()
	{
		UWorld* World = GetDebugWorld();
		if (!World)
		{
			return nullptr;
		}

		UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
		if (!MassSub)
		{
			return nullptr;
		}

		return &MassSub->GetMutableEntityManager();
	}

	FArcMassItemStoreFragment* GetStore(FMassEntityManager& Manager, FMassEntityHandle Entity)
	{
		FMassEntityView EntityView(Manager, Entity);
		return EntityView.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
	}

	FString FormatGameplayTag(const FGameplayTag& Tag)
	{
		return Tag.IsValid() ? Tag.ToString() : TEXT("-");
	}

	FString FormatItemId(const FArcItemId& ItemId)
	{
		return ItemId.IsValid() ? ItemId.ToString() : TEXT("-");
	}

	FString FormatItemDefinition(const FPrimaryAssetId& ItemDefinitionId)
	{
		if (!ItemDefinitionId.IsValid())
		{
			return TEXT("-");
		}
		const FString Filename = UArcCoreAssetManager::Get().GetJsonRecordFilename(ItemDefinitionId);
		return Filename.IsEmpty() ? ItemDefinitionId.ToString() : FPaths::GetBaseFilename(Filename);
	}

	const FArcItemData* GetItemAt(const FArcMassItemStoreFragment& Store, int32 Index)
	{
		if (!Store.ReplicatedItems.Items.IsValidIndex(Index))
		{
			return nullptr;
		}
		return Store.ReplicatedItems.Items[Index].ToItem();
	}

	void DrawStructProperties(const UScriptStruct* StructType, const void* StructData, const char* TableIdPrefix)
	{
		if (!StructType || !StructData)
		{
			ImGui::TextDisabled("(invalid struct)");
			return;
		}

		const FString TableId = FString::Printf(TEXT("%s_%s"), ANSI_TO_TCHAR(TableIdPrefix), *StructType->GetName());
		if (ImGui::BeginTable(TCHAR_TO_ANSI(*TableId), 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 220.f);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			for (TFieldIterator<FProperty> PropIt(StructType); PropIt; ++PropIt)
			{
				const FProperty* Property = *PropIt;
				FString Value;
				const void* PropertyData = Property->ContainerPtrToValuePtr<void>(StructData);
				Property->ExportTextItem_Direct(Value, PropertyData, nullptr, nullptr, PPF_None);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("%s", TCHAR_TO_ANSI(*Property->GetName()));
				ImGui::TableSetColumnIndex(1);
				ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*Value));
			}

			ImGui::EndTable();
		}
	}

	void DrawTags(const char* Header, const FGameplayTagContainer& Tags)
	{
		if (Tags.Num() == 0)
		{
			return;
		}

		ImGui::Spacing();
		ImGui::SeparatorText(Header);
		TArray<FGameplayTag> TagArray;
		Tags.GetGameplayTagArray(TagArray);
		for (const FGameplayTag& Tag : TagArray)
		{
			ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
		}
	}
}
