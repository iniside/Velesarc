#include "ArcGunDebugger.h"

#include "ArcGunRecoilInstance.h"
#include "ArcGunStateComponent.h"
#include "imgui.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

void FArcGunDebugger::Initialize()
{
}

void FArcGunDebugger::Uninitialize()
{
}

void FArcGunDebugger::Draw()
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

	APawn* P = PS->GetPawn();
	if (!P)
	{
		return;
	}

	UArcGunStateComponent* GunState = PS->FindComponentByClass<UArcGunStateComponent>();
	if (!GunState)
	{
		return;
	}

	auto BoolToText = [](bool InBool)
	{
		return InBool ? TEXT("True") : TEXT("False");
	};
	
	ImGui::Begin("Proxy Attribute Sets");

	ImGui::Checkbox("Draw Targeting", &bDrawTargeting);
	if (bDrawTargeting)
	{
		ImGui::InputFloat("Size", &TargetingDrawSize, 0.01f, 1.f);
		ImGui::ColorEdit3("DebugDraw Color", TargetingDrawColor);
		FColor Color(
						FColor::QuantizeUNormFloatTo8(TargetingDrawColor[0])
						, FColor::QuantizeUNormFloatTo8(TargetingDrawColor[1])
						, FColor::QuantizeUNormFloatTo8(TargetingDrawColor[2])
						, 255);

		const FHitResult& Hit = GunState->GetCurrentHitResult();
		DrawDebugLine(World, Hit.TraceStart, Hit.TraceEnd, Color, false, -1, 0, TargetingDrawSize);

		GunState->DrawDebug();
	}

	ImGui::Checkbox("Draw Camera Direction", &bDrawCameraDirection);
	if (bDrawCameraDirection)
	{
		ImGui::InputFloat("Camera DirSize", &CameraDirectionDrawSize, 0.01f, 1.f);
		ImGui::InputFloat("Max Distance", &CameraDirectionDrawDistance, 5.f, 10.f);
		ImGui::ColorEdit3("Camera Dir DebugDraw Color", CameraDirectionDrawColor);
		FColor Color(
						FColor::QuantizeUNormFloatTo8(CameraDirectionDrawColor[0])
						, FColor::QuantizeUNormFloatTo8(CameraDirectionDrawColor[1])
						, FColor::QuantizeUNormFloatTo8(CameraDirectionDrawColor[2])
						, 255);


		FVector EyeLoc;
		FRotator EyeRot;
		P->GetActorEyesViewPoint(EyeLoc, EyeRot);
		DrawDebugLine(World, EyeLoc, EyeLoc + (EyeRot.Vector() * CameraDirectionDrawDistance), Color, false, -1, 0, CameraDirectionDrawSize);
	}

	ImGui::Checkbox("Draw Camera Point", &bDrawCameraPoint);
	if (bDrawCameraPoint)
	{
		ImGui::InputFloat("Camera Point Size", &CameraPointDrawSize, 0.01f, 1.f);
		ImGui::InputFloat("Offset", &CameraPointDrawOffset, 1.f, 5.f);
		ImGui::ColorEdit3("Camera Point DebugDraw Color", CameraPointDrawColor);
		FColor Color(
						FColor::QuantizeUNormFloatTo8(CameraPointDrawColor[0])
						, FColor::QuantizeUNormFloatTo8(CameraPointDrawColor[1])
						, FColor::QuantizeUNormFloatTo8(CameraPointDrawColor[2])
						, 255);

		FVector EyeLoc;
		FRotator EyeRot;
		P->GetActorEyesViewPoint(EyeLoc, EyeRot);
		DrawDebugPoint(World, EyeLoc + (EyeRot.Vector() * CameraPointDrawOffset), CameraPointDrawSize, Color);
	}
	
	const FArcGunRecoilInstance_Base* Recoil = GunState->GetGunRecoil<FArcGunRecoilInstance_Base>();
	if (Recoil)
	{
		ImGui::BeginTable("Gun Recoil", 2);

		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Control Rotation ");
			ImGui::TableNextColumn();
			ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("%s"), *PC->GetControlRotation().ToString())));
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("Spread Heat ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->SpreadHeat);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("RecoilHeat ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->RecoilHeat);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("RecoveredPitch ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->RecoveredPitch);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("LastFireTime ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->LastFireTime);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("ShotsFired ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->ShotsFired);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("TargetBiasDir ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->TargetBiasDir);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("CurrentVelocityLevel ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->CurrentVelocityLevel);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("CurrentRotationLevel ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->CurrentRotationLevel);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("HighestAxisRotation ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->HighestAxisRotation);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("StartYInputValue ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->StartYInputValue);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("EndYInputValue ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->EndYInputValue);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("RecoveredInputValue ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->RecoveredInputValue);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("RecoveryDirection ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->RecoveryDirection);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("LocalSpaceRecoilAccumulatedPhase ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->LocalSpaceRecoilAccumulatedPhase);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("TimeAccumulator ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->TimeAccumulator);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("RotationSpreadGainSpeed ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->RotationSpreadGainSpeed);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("SpreadVelocityGainInterpolation ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->SpreadVelocityGainInterpolation);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("HeatPerShotMultiplier ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->HeatPerShotMultiplier);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("SpreadHeatGainMultiplier ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->SpreadHeatGainMultiplier);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("SpreadHeatRecoveryDelay ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->SpreadHeatRecoveryDelay);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("VelocitySpreadAdditive ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->VelocitySpreadAdditive);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("RotationSpreadAdditive ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->RotationSpreadAdditive);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("SpreadAngleMultiplier ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->SpreadAngleMultiplier);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("SpreadRecoveryMultiplier ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->SpreadRecoveryMultiplier);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("CurrentSpreadAngle ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->CurrentSpreadAngle);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("RecoilHeatRecoveryDelay ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->RecoilHeatRecoveryDelay);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("RecoilRecoveryMultiplier ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->RecoilRecoveryMultiplier);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("RecoilHeatGainMultiplier ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->RecoilHeatGainMultiplier);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("RecoilPitchMultiplier ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->RecoilPitchMultiplier);
		}
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::Text("RecoilYawMultiplier ");
			ImGui::TableNextColumn();
			ImGui::Text("%f", Recoil->RecoilYawMultiplier);
		}
		ImGui::EndTable();
	}
	ImGui::End();
}
