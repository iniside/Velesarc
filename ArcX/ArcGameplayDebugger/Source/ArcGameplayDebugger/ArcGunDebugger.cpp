#include "ArcGunDebugger.h"

#include "ArcGunRecoilInstance.h"
#include "ArcGunStateComponent.h"
#include "imgui.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Pawn.h"
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

	}
	ImGui::End();
}
