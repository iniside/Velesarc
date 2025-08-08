#include "ArcBuilderDebugger.h"

#include "ArcBuilderComponent.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "imgui.h"

void FArcBuilderDebugger::Initialize()
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

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	
	IAssetRegistry& AR = AssetManager->GetAssetRegistry();

	BuildDataAssetList.Reset(1024);
	
	AR.GetAssetsByClass(UArcBuilderData::StaticClass()->GetClassPathName(), BuildDataAssetList);
}

void FArcBuilderDebugger::Uninitialize()
{
}

void FArcBuilderDebugger::Draw()
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

	UArcBuilderComponent* BuilderComponent = PS->FindComponentByClass<UArcBuilderComponent>();
	if (!BuilderComponent)
	{
		return;
	}

	FString SelectedBuildDataAssetName = "Select Build Data";
	if (BuildDataAssetList.IsValidIndex(SelectedBuildDataAssetIndex))
	{
		SelectedBuildDataAssetName = FString::Printf(TEXT("%s"), *BuildDataAssetList[SelectedBuildDataAssetIndex].AssetName.ToString());
	}
	
	if (ImGui::BeginCombo("Select Build Data", TCHAR_TO_ANSI(*SelectedBuildDataAssetName)))
	{
		for (int32 Idx = 0; Idx < BuildDataAssetList.Num(); Idx++)
		{
			FString Name = FString::Printf(TEXT("%s"), *BuildDataAssetList[Idx].AssetName.ToString());
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Name), false))
			{
				SelectedBuildDataAssetIndex = Idx;
			}
		}
		ImGui::EndCombo();
	}

	if (!BuildDataAssetList.IsValidIndex(SelectedBuildDataAssetIndex))
	{
		return;
	}

	UArcBuilderData* BuildData = Cast<UArcBuilderData>(BuildDataAssetList[SelectedBuildDataAssetIndex].GetAsset());
	if (!BuildData)
	{
		return;
	}

	if (ImGui::Button("Begin Placment"))
	{
		BuilderComponent->BeginPlacement(BuildData);
	}
	if (ImGui::Button("Begin Placment"))
	{
		BuilderComponent->EndPlacement();
	}
}
