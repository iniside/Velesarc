#include "ArcCraftGameplayDebugger.h"

#include "EngineUtils.h"
#include "ArcCraft/ArcCraftComponent.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Kismet/GameplayStatics.h"
#include "imgui.h"
#include "GameFramework/PlayerState.h"

void FArcCraftGameplayDebugger::Initialize()
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

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		UArcCraftComponent* CraftComponent = Actor->FindComponentByClass<UArcCraftComponent>();
		
		if (CraftComponent)
		{
			CraftStations Stations;
			Stations.CraftComponent = CraftComponent;
			CraftStationsList.Add(Stations);
		}
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	
	IAssetRegistry& AR = AssetManager->GetAssetRegistry();

	CraftDataAssetList.Reset(1024);
	
	AR.GetAssetsByClass(UArcCraftData::StaticClass()->GetClassPathName(), CraftDataAssetList);
}

void FArcCraftGameplayDebugger::Uninitialize()
{

}

void FArcCraftGameplayDebugger::Draw()
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

	
	FString CurrentStationName;
	if (CraftStationsList.Num() > 0 && SelectedCraftStationIndex > -1)
	{
		CurrentStationName = FString::Printf(TEXT("%s %s"), *CraftStationsList[SelectedCraftStationIndex].CraftComponent->GetOwner()->GetName(), *CraftStationsList[SelectedCraftStationIndex].CraftComponent->GetName());
	}
	else
	{
		CurrentStationName = TEXT("No Station");
	}
	
	if (ImGui::BeginCombo("Slect Station", TCHAR_TO_ANSI(*CurrentStationName)))
	{
		for (int32 Idx = 0; Idx < CraftStationsList.Num(); Idx++)
		{
			FString Name = FString::Printf(TEXT("%s %s"), *CraftStationsList[Idx].CraftComponent->GetOwner()->GetName(), *CraftStationsList[Idx].CraftComponent->GetName());
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Name), false))
			{
				SelectedCraftStationIndex = Idx;
			}
		}
		ImGui::EndCombo();
	}

	if (!CraftStationsList.IsValidIndex(SelectedCraftStationIndex))
	{
		ImGui::Text("No Craft Station Selected");
		return;
	}

	FString CurrentCraftData;
	if (CraftDataAssetList.Num() > 0 && SelectedCratDataAssetIndex > -1)
	{
		CurrentCraftData = FString::Printf(TEXT("%s"), *CraftDataAssetList[SelectedCratDataAssetIndex].AssetName.ToString());
	}
	else
	{
		CurrentCraftData = TEXT("No Craft Data");
	}

	const CraftStations& Station = CraftStationsList[SelectedCraftStationIndex];
	
	if (ImGui::BeginCombo("Select Craft Data", TCHAR_TO_ANSI(*CurrentCraftData)))
	{
		for (int32 Idx = 0; Idx < CraftDataAssetList.Num(); Idx++)
		{
			FString Name = FString::Printf(TEXT("%s"), *CraftDataAssetList[Idx].AssetName.ToString());
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Name), false))
			{
				SelectedCratDataAssetIndex = Idx;
			}
		}
		ImGui::EndCombo();
	}
	
	ImGui::InputInt("Amount", &CraftAmount);
	if (ImGui::Button("Create Item"))
	{
		if (CraftDataAssetList.IsValidIndex(SelectedCratDataAssetIndex))
		{
			UArcCraftData* CraftData = Cast<UArcCraftData>(CraftDataAssetList[SelectedCratDataAssetIndex].GetAsset());
			if (CraftData)
			{
				Station.CraftComponent->CraftItem(CraftData, PS, CraftAmount);
			}
		}
	}
	
	const TArray<FArcCraftItem>& CurrentCraftedItems = Station.CraftComponent->GetCraftedItemList();

	if (ImGui::BeginTable("Crafted Items Stable", 3))
	{
		for (int32 Idx = 0; Idx < CurrentCraftedItems.Num(); Idx++)
		{
			FString RecipeName = FString::Printf(TEXT("%s"), *CurrentCraftedItems[Idx].Recipe->GetName());
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
				ImGui::Text(TCHAR_TO_ANSI(*RecipeName));
			ImGui::TableNextColumn();
				ImGui::Text("%.2f", CurrentCraftedItems[Idx].CurrentTime);
			ImGui::TableNextColumn();
				ImGui::Text("%d", CurrentCraftedItems[Idx].CurrentAmount);
		}
		ImGui::EndTable();	
	}

	const TArray<FArcItemSpec>& Items = Station.CraftComponent->GetCraftedItemSpecList().Items;

	if (ImGui::BeginTable("Stored Items", 3))
	{
		for (int32 Idx = 0; Idx < Items.Num(); Idx++)
		{
			const FArcItemSpec& ItemData = Items[Idx];
			
			FString ItemName = FString::Printf(TEXT("%s"), *ItemData.GetItemDefinition()->GetName());
			FString ItemId = FString::Printf(TEXT("%s"), *ItemData.ItemId.ToString());
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
				ImGui::Text(TCHAR_TO_ANSI(*ItemName));
			ImGui::TableNextColumn();
				ImGui::Text(TCHAR_TO_ANSI(*ItemId));
			ImGui::TableNextColumn();
				ImGui::Text("%d", ItemData.Amount);
		}
		ImGui::EndTable();	
	}
}
