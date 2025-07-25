#include "ArcEquipmentDebugger.h"

#include "imgui.h"
#include "Commands/ArcEquipItemCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Commands/ArcUnequipItemCommand.h"
#include "Equipment/ArcEquipmentComponent.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Kismet/GameplayStatics.h"

void FArcEquipmentDebugger::Initialize()
{
}

void FArcEquipmentDebugger::Uninitialize()
{
}

void FArcEquipmentDebugger::Draw()
{
	if (!GEngine)
	{
		return;
	}
	
	if (!GEngine->GameViewport)
	{
		return;
	}

	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}

	if (!PC->PlayerState)
	{
		return;
	}

	APlayerState* PS = PC->PlayerState;
	
	TArray<UArcItemsStoreComponent*> ItemsStores;
	PS->GetComponents<UArcItemsStoreComponent>(ItemsStores);

	TArray<FString> ItemStoreNames;
	Algo::Transform(ItemsStores, ItemStoreNames, [](const UArcItemsStoreComponent* Store){
		return Store ? FString::Printf(TEXT("Add to %s"), *Store->GetName()) : FString(TEXT("Invalid Store"));
	});

	UArcEquipmentComponent* EquipmentComponent = PS->FindComponentByClass<UArcEquipmentComponent>();
	if (!EquipmentComponent)
	{
		return;
	}

	const TArray<FArcEquipmentSlot>& EquipmentSlots = EquipmentComponent->GetEquipmentSlots();
	if (EquipmentSlots.Num() == 0)
	{
		return;
	}

	ImGui::Begin("Equipment");
	for (int32 Idx = 0; Idx < EquipmentSlots.Num(); ++Idx)
	{
		const FArcEquipmentSlot& EquipmentSlot = EquipmentSlots[Idx];
		if (!EquipmentSlot.SlotId.IsValid())
		{
			continue;
		}
		
		if (ImGui::TreeNode(TCHAR_TO_ANSI(*EquipmentSlot.SlotId.ToString())))
		{
			UArcItemsStoreComponent* EquipmentItemsStore = EquipmentComponent->GetItemsStore();
			const bool bIsSlotLocked = EquipmentItemsStore->IsSlotLocked(EquipmentSlot.SlotId);
			FString SlotLocked = bIsSlotLocked ? TEXT("Slot (Locked)") : TEXT("Slot (Unlocked)");

			ImGui::Text(TCHAR_TO_ANSI(*SlotLocked));
			if (ImGui::Button("Unequip"))
			{
				Arcx::SendServerCommand<FArcUnequipItemCommand>(PC, EquipmentComponent, EquipmentSlot.SlotId);
			}

			const FArcItemData* ExistingItem = EquipmentItemsStore->GetItemFromSlot(EquipmentSlot.SlotId);
			FString PreviewText = "Select Item to Equip";
			if (ExistingItem)
			{
				PreviewText = GetNameSafe(ExistingItem->GetItemDefinition());
			}
			if (ImGui::BeginCombo("EquipmentTable", TCHAR_TO_ANSI(*PreviewText)))
			{
				TArray<const FArcItemData*> Items = EquipmentComponent->GetItemsFromStoreForSlot(EquipmentSlot.SlotId);
				for (int32 ItemIdx = 0; ItemIdx < Items.Num(); ++ItemIdx)
				{
					const FArcItemData* ItemData = Items[ItemIdx];
					if (!ItemData || !ItemData->GetItemDefinition())
					{
						continue;
					}

					FString ItemName = GetNameSafe(ItemData->GetItemDefinition());
					
					ImGui::PushID(TCHAR_TO_ANSI(*ItemData->GetItemId().ToString()));
					FString ItemDisplayName = FString::Printf(TEXT("Equip (%s)"), *ItemName);
					if (ImGui::Selectable(TCHAR_TO_ANSI(*ItemDisplayName)))
					{
						Arcx::SendServerCommand<FArcEquipItemCommand>(PC,
							EquipmentComponent->GetItemsStore(), EquipmentComponent, ItemData->GetItemId(), EquipmentSlot.SlotId);
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}

			if (ExistingItem)
			{
				FString ItemId = FString::Printf(TEXT("ItemId: %s"), *ExistingItem->GetItemId().ToString());
				ImGui::Text(TCHAR_TO_ANSI(*ItemId));

				FString ItemsStore = FString::Printf(TEXT("ItemsStore: %s"), *GetNameSafe(ExistingItem->GetItemsStoreComponent()));
				ImGui::Text(TCHAR_TO_ANSI(*ItemsStore));
			}
			
			ImGui::TreePop();
		}
	}

	ImGui::End();
}
