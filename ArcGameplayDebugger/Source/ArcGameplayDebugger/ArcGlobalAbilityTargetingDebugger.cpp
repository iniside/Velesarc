#include "ArcGlobalAbilityTargetingDebugger.h"

#include "ArcGlobalAbilityTargetingDebuggerDrawComponent.h"
#include "imgui.h"
#include "StateTreePropertyBindings.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

#include "TargetingSystem/TargetingPreset.h"
#include "TargetingSystem/TargetingSubsystem.h"
#include "Tasks/TargetingTask.h"

void FArcGlobalAbilityTargetingDebugger::Initialize()
{
	DebugDraw.AddZeroed(256);
	for (int32 i = 0; i < DebugDraw.Num(); i++)
	{
		DebugDraw[i].bDrawTargetingTask.AddZeroed(256);
	}
}

void FArcGlobalAbilityTargetingDebugger::Uninitialize()
{
	DestroyDrawActor();
}

void HitResultToTable(const FHitResult& InHIt)
{
	if (ImGui::BeginTable("HitResultTable", 2))
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("bBlockingHit:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text(InHIt.bBlockingHit == true ? "True" : "False");
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("bStartPenetrating:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text(InHIt.bStartPenetrating == true ? "True" : "False");
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("Time:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text("%f", InHIt.Time);
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("Location:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text(TCHAR_TO_ANSI(*InHIt.Location.ToString()));
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("ImpactPoint:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text(TCHAR_TO_ANSI(*InHIt.ImpactPoint.ToString()));
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("ImpactNormal:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text(TCHAR_TO_ANSI(*InHIt.ImpactNormal.ToString()));
			


		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("Normal:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text(TCHAR_TO_ANSI(*InHIt.Normal.ToString()));
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("TraceStart:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text(TCHAR_TO_ANSI(*InHIt.TraceStart.ToString()));
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("TraceEnd:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text(TCHAR_TO_ANSI(*InHIt.TraceEnd.ToString()));
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("PenetrationDepth:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text("%f", InHIt.PenetrationDepth);
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("Item:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text("%d", InHIt.Item);
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("HitObjectHandle:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("%s"), InHIt.HitObjectHandle.IsValid() ? *InHIt.HitObjectHandle.GetName() : TEXT("None"))));
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("Component:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text(TCHAR_TO_ANSI(*GetNameSafe(InHIt.Component.Get())));
			

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("Time:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text(TCHAR_TO_ANSI(*InHIt.BoneName.ToString()));

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
			ImGui::Text("FaceIndex:");
		ImGui::TableSetColumnIndex(1);
			ImGui::Text("%d", InHIt.FaceIndex);
		
		ImGui::EndTable();
	}
}
void FArcGlobalAbilityTargetingDebugger::Draw()
{
	if (!GEngine)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	if (!GEngine->GameViewport)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	if (!PC->PlayerState)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	APlayerState* PS = PC->PlayerState;

	UTargetingSubsystem* TargetingSubsystem = PS->GetGameInstance()->GetSubsystem<UTargetingSubsystem>();
	
	UArcCoreAbilitySystemComponent* AbilitySystem = PS->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	if (!AbilitySystem)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearShapes();
		}
		return;
	}

	EnsureDrawActor(World);
	if (!DrawComponent.IsValid())
	{
		return;
	}

	const double WorldTime = AbilitySystem->GetWorld()->GetTimeSeconds();
	auto BoolToText = [](bool InBool)
	{
		return InBool ? TEXT("True") : TEXT("False");
	};
	ImGui::Begin("Global Targeting");
	
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

			if (ImGui::TreeNode("Hit Result"))
			{
				HitResultToTable(Targeting.Value.HitResult);
				ImGui::TreePop();
			}
			

			if (ImGui::TreeNode("Hit Results"))
			{
				for (const FHitResult& Hit : Targeting.Value.HitResults)
				{
					ImGui::PushID(TCHAR_TO_ANSI(*FString::Printf(TEXT("HitResult %d"), Counter)));
					if (ImGui::TreeNode("Hit Result"))
					{
						HitResultToTable(Hit);
						ImGui::TreePop();
					}
					ImGui::PopID();	
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Debug Drawing"))
			{
				ImGui::PushID(TCHAR_TO_ANSI(*FString::Printf(TEXT("%s %d"),*Targeting.Key.ToString(), Counter)));
				ImGui::Checkbox("Draw Debug", &DebugDraw[Counter].bShow);
				ImGui::PopID();

				ImGui::SliderFloat("Size", &DebugDraw[Counter].Size, 1.0f, 500.0f);
				ImGui::ColorEdit3("DebugDraw Color", DebugDraw[Counter].Color);
				if (DebugDraw[Counter].bShow)
				{
					FColor Color(
						FColor::QuantizeUNormFloatTo8(DebugDraw[Counter].Color[0])
						, FColor::QuantizeUNormFloatTo8(DebugDraw[Counter].Color[1])
						, FColor::QuantizeUNormFloatTo8(DebugDraw[Counter].Color[2])
						, 255);
					
					TArray<FArcTargetingDebugSphere> Spheres;

					if (AActor* Actor = Targeting.Value.HitResult.GetActor())
					{
						FArcTargetingDebugSphere& Sphere = Spheres.AddDefaulted_GetRef();
						Sphere.Location = Actor->GetActorLocation();
						Sphere.Radius = DebugDraw[Counter].Size;
						Sphere.Color = Color;
					}

					for (const FHitResult& Hit : Targeting.Value.HitResults)
					{
						if (AActor* Actor = Hit.GetActor())
						{
							FArcTargetingDebugSphere& Sphere = Spheres.AddDefaulted_GetRef();
							Sphere.Location = Actor->GetActorLocation();
							Sphere.Radius = DebugDraw[Counter].Size;
							Sphere.Color = Color;
						}
					}

					DrawComponent->UpdateTargetingSpheres(Spheres);

					FTargetingDebugInfo TargetingDebugInfo;
					const FTargetingTaskSet* TaskSet = Targeting.Value.TargetingPreset->GetTargetingTaskSet();
					if (TaskSet)
					{
						for (int32 Idx = 0; Idx < TaskSet->Tasks.Num(); Idx++)
						{
							if (!TaskSet->Tasks[Idx])
							{
								continue;
							}

							ImGui::PushID(TCHAR_TO_ANSI(*FString::Printf(TEXT("%s %d"), *GetNameSafe(TaskSet->Tasks[Idx]), Counter)));
							ImGui::Checkbox(TCHAR_TO_ANSI(*FString::Printf(TEXT("Draw %s"), *GetNameSafe(TaskSet->Tasks[Idx]))), &DebugDraw[Counter].bDrawTargetingTask[Idx]);
							ImGui::PopID();
							if (DebugDraw[Counter].bDrawTargetingTask[Idx])
							{
								TaskSet->Tasks[Idx]->DrawDebug(TargetingSubsystem, TargetingDebugInfo, Targeting.Value.TargetingHandle, 0,0,0);
							}
						}
					}
				}
				else
				{
					DrawComponent->ClearShapes();
				}
				ImGui::TreePop();
			}
			
			
			
			ImGui::TreePop();
		}

		Counter++;
	}

	ImGui::End();
}

void FArcGlobalAbilityTargetingDebugger::EnsureDrawActor(UWorld* World)
{
	if (DrawActor.IsValid())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transient;
	AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!NewActor)
	{
		return;
	}

#if WITH_EDITOR
	NewActor->SetActorLabel(TEXT("GlobalAbilityTargetingDebuggerDraw"));
#endif

	UArcGlobalAbilityTargetingDebuggerDrawComponent* NewComponent = NewObject<UArcGlobalAbilityTargetingDebuggerDrawComponent>(NewActor, UArcGlobalAbilityTargetingDebuggerDrawComponent::StaticClass());
	NewComponent->RegisterComponent();
	NewActor->AddInstanceComponent(NewComponent);

	DrawActor = NewActor;
	DrawComponent = NewComponent;
}

void FArcGlobalAbilityTargetingDebugger::DestroyDrawActor()
{
	if (DrawActor.IsValid())
	{
		DrawActor->Destroy();
	}
	DrawActor.Reset();
	DrawComponent.Reset();
}
