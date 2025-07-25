#include "ArcGlobalAbilityTargetingDebugger.h"

#include "imgui.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#include "TargetingSystem/TargetingPreset.h"

void FArcGlobalAbilityTargetingDebugger::Initialize()
{
	bDrawDebug.AddZeroed(256);
}

void FArcGlobalAbilityTargetingDebugger::Uninitialize()
{
}

void FArcGlobalAbilityTargetingDebugger::Draw()
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

	UArcCoreAbilitySystemComponent* AbilitySystem = PS->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	if (!AbilitySystem)
	{
		return;
	}

	const double WorldTime = AbilitySystem->GetWorld()->GetTimeSeconds();
	auto BoolToText = [](bool InBool)
	{
		return InBool ? TEXT("True") : TEXT("False");
	};

	if (ImGui::TreeNode("Global Ability Targeting"))
	{
		const TMap<FGameplayTag, FArcCoreGlobalTargetingEntry>& TargetingPresets = AbilitySystem->GetTargetingPresets();
		int32 Counter = 0;
		for (const auto& Targeting : TargetingPresets)
		{
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*Targeting.Key.ToString())))
			{
				FString TargetingPreset = FString::Printf(TEXT("Targeting Preset: %s"), *GetNameSafe(Targeting.Value.TargetingPreset));
				ImGui::Text(TCHAR_TO_ANSI(*TargetingPreset));

				FString IsClient = FString::Printf(TEXT("Is Client: %s"), BoolToText(Targeting.Value.bIsClient));
				ImGui::Text(TCHAR_TO_ANSI(*IsClient));

				FString HitActor = FString::Printf(TEXT("Hit Actor %s"), *GetNameSafe(Targeting.Value.HitResult.GetActor()));
				ImGui::Text(TCHAR_TO_ANSI(*HitActor));

				FString HitResult = FString::Printf(TEXT("Hit Location %s"), *Targeting.Value.HitResult.ToString());
				ImGui::Text(TCHAR_TO_ANSI(*HitResult));

				if (ImGui::TreeNode("Hit Results"))
				{
					for (const FHitResult& Hit : Targeting.Value.HitResults)
					{
						FString HitString = FString::Printf(TEXT("Hit Result: %s"), *Hit.ToString());
						ImGui::Text(TCHAR_TO_ANSI(*HitString));
					}
					ImGui::TreePop();
				}
				
				ImGui::PushID(TCHAR_TO_ANSI(*FString::Printf(TEXT("%s %d"),*Targeting.Key.ToString(), Counter)));
				if (ImGui::Checkbox("Draw Debug", &bDrawDebug[Counter]))
				{
					if (bDrawDebug[Counter])
					{
						if (AActor* Actor = Targeting.Value.HitResult.GetActor())
						{
							DrawDebugSphere(World, Actor->GetActorLocation(), 100.0f, 16, FColor::Red, false, 0.0f);	
						}

						for (const FHitResult& Hit : Targeting.Value.HitResults)
						{
							if (AActor* Actor = Hit.GetActor())
							{
								DrawDebugSphere(World, Actor->GetActorLocation(), 100.0f, 16, FColor::Red, false, 0.0f);	
							}		
						}
						
					}
				}
				ImGui::PopID();
				
				ImGui::TreePop();
			}

			Counter++;
		}
		ImGui::TreePop();
	}
}
