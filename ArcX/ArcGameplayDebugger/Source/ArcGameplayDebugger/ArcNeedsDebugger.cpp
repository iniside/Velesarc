#include "ArcNeedsDebugger.h"

#include "EngineUtils.h"
#include "imgui.h"
#include "MassAgentComponent.h"
#include "MassEntitySubsystem.h"
#include "MassSubsystemBase.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "NeedsSystem/ArcNeedsFragment.h"

void FArcNeedsDebugger::Initialize()
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
	
	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	Entities.Reset(128);

	TArray<AActor*> Actors;
	for (TActorIterator<AActor> It(PC->GetWorld()); It; ++It)
	{
		Actors.Add(*It);
		UMassAgentComponent* AgentComponent = It->FindComponentByClass<UMassAgentComponent>();
		if (AgentComponent)
		{
			Entities.Add(AgentComponent->GetEntityHandle());
		}
	}

	if (Actors.Num() > 0)
	{
		SelectedEntityIdx = -1;
	}
}

void FArcNeedsDebugger::Uninitialize()
{

}

void FArcNeedsDebugger::Draw()
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
	
	ImGui::Begin("Needs Debugger");

	if (ImGui::BeginCombo("Select Entity", "None"))
	{
		for (int32 EntityIdx = 0; EntityIdx < Entities.Num(); EntityIdx++)
		{
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Entities[EntityIdx].DebugGetDescription())))
			{
				SelectedEntityIdx = EntityIdx;
			}
		}
		ImGui::EndCombo();
	}

	if (SelectedEntityIdx == INDEX_NONE)
	{
		ImGui::End();
		return;
	}

	UMassEntitySubsystem* MassSub = PC->GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	const FMassEntityManager& Manager =  MassSub->GetEntityManager();
	const FArcNeedsFragment* NeedsFragment = Manager.GetFragmentDataPtr<FArcNeedsFragment>(Entities[SelectedEntityIdx]);

	if (NeedsFragment)
	{
		for (const FArcNeedItem& NeedItem : NeedsFragment->Needs)
		{
			FString Need = FString::Printf(TEXT("%s Current: %.3f ChangeRate: %.3f"), *NeedItem.NeedName.ToString(), NeedItem.CurrentValue, NeedItem.ChangeRate);
			ImGui::Text(TCHAR_TO_ANSI(*Need));
		}
	}
	ImGui::End();
}
