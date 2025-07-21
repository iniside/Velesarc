#include "ArcItemAttachmentDebugger.h"

#include "imgui.h"
#include "Engine/AssetManager.h"
#include "Equipment/ArcItemAttachmentComponent.h"
#include "GameFramework/PlayerState.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Kismet/GameplayStatics.h"

void FArcItemAttachmentDebugger::Initialize()
{
}

void FArcItemAttachmentDebugger::Uninitialize()
{
}

void FArcItemAttachmentDebugger::Draw()
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

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return;
	}

	APlayerState* PS = PC->PlayerState;

	
	TArray<UArcItemsStoreComponent*> ItemsStores;
	PS->GetComponents<UArcItemsStoreComponent>(ItemsStores);

	UArcItemAttachmentComponent* ItemAttachmentComponent = PS->FindComponentByClass<UArcItemAttachmentComponent>();
	if (!ItemAttachmentComponent)
	{
		return;
	}

	const TMap<FArcItemId, FArcItemId>& PendingAttachments = ItemAttachmentComponent->GetPendingAttachments();
	const FArcItemAttachmentContainer& Attachments = ItemAttachmentComponent->GetReplicatedAttachments();

	for (int32 Idx = 0; Idx < Attachments.Items.Num(); ++Idx)	
	{
		const FArcItemAttachment& Attachment = Attachments.Items[Idx];
		if (!Attachment.ItemDefinition)
		{
			continue;
		}

		FString Slot = Attachment.SlotId.ToString();
		FString ItemName = FString::Printf(TEXT("%s %s"),*Slot, *GetNameSafe(Attachment.ItemDefinition));
		if (ImGui::TreeNode(TCHAR_TO_ANSI(*ItemName)))
		{
			ImGui::Text("ItemId: %s", *Attachment.ItemId.ToString());
			ImGui::Text("OwnerId: %s", *Attachment.OwnerId.ToString());

			ImGui::Text("Socket: %s", TCHAR_TO_ANSI(*Attachment.SocketName.ToString()));
			ImGui::Text("SocketComponentTag: %s", TCHAR_TO_ANSI(*Attachment.SocketComponentTag.ToString()));
			ImGui::Text("ChangeSocket: %s", TCHAR_TO_ANSI(*Attachment.ChangedSocket.ToString()));
			ImGui::Text("ChangeSocketComponentTag: %s", TCHAR_TO_ANSI(*Attachment.ChangeSceneComponentTag.ToString()));

			ImGui::Text("SlotId: %s", TCHAR_TO_ANSI(*Attachment.SlotId.ToString()));
			ImGui::Text("OwnerSlotId: %s", TCHAR_TO_ANSI(*Attachment.OwnerSlotId.ToString()));
			
			ImGui::Text("Relative Transform: %s", *Attachment.RelativeTransform.ToString());
			ImGui::Text("Visual Item: %s", *GetNameSafe(Attachment.VisualItemDefinition));
			ImGui::Text("Old Visual Item: %s", *GetNameSafe(Attachment.OldVisualItemDefinition));

			ImGui::Text("Item Definition: %s", *GetNameSafe(Attachment.ItemDefinition));
			ImGui::Text("Owner Item: %s", *GetNameSafe(Attachment.OwnerItemDefinition));
			
			ImGui::TreePop();
		}
	}
}
