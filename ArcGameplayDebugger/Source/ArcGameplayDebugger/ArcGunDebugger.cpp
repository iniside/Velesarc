#include "ArcGunDebugger.h"

#include "ArcGunDebuggerDrawComponent.h"
#include "ArcGunRecoilInstance.h"
#include "ArcGunStateComponent.h"
#include "imgui.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

void FArcGunDebugger::Initialize()
{
}

void FArcGunDebugger::Uninitialize()
{
	DestroyDrawActor();
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

	ImGui::Begin("Proxy Attribute Sets");

	FArcGunDebugDrawData DebugData;

	ImGui::Checkbox("Draw Targeting", &bDrawTargeting);
	if (bDrawTargeting)
	{
		ImGui::InputFloat("Size", &TargetingDrawSize, 0.01f, 1.f);
		ImGui::ColorEdit3("DebugDraw Color", TargetingDrawColor);

		const FHitResult& Hit = GunState->GetCurrentHitResult();
		DebugData.bDrawTargeting = true;
		DebugData.TraceStart = Hit.TraceStart;
		DebugData.TraceEnd = Hit.TraceEnd;
		DebugData.TargetingColor = FColor(
			FColor::QuantizeUNormFloatTo8(TargetingDrawColor[0]),
			FColor::QuantizeUNormFloatTo8(TargetingDrawColor[1]),
			FColor::QuantizeUNormFloatTo8(TargetingDrawColor[2]),
			255);
		DebugData.TargetingSize = TargetingDrawSize;

		GunState->DrawDebug();
	}

	ImGui::Checkbox("Draw Camera Direction", &bDrawCameraDirection);
	if (bDrawCameraDirection)
	{
		ImGui::InputFloat("Camera DirSize", &CameraDirectionDrawSize, 0.01f, 1.f);
		ImGui::InputFloat("Max Distance", &CameraDirectionDrawDistance, 5.f, 10.f);
		ImGui::ColorEdit3("Camera Dir DebugDraw Color", CameraDirectionDrawColor);

		FVector EyeLoc;
		FRotator EyeRot;
		P->GetActorEyesViewPoint(EyeLoc, EyeRot);

		DebugData.bDrawCameraDirection = true;
		DebugData.EyeLocation = EyeLoc;
		DebugData.EyeDirection = EyeRot.Vector();
		DebugData.CameraDirectionColor = FColor(
			FColor::QuantizeUNormFloatTo8(CameraDirectionDrawColor[0]),
			FColor::QuantizeUNormFloatTo8(CameraDirectionDrawColor[1]),
			FColor::QuantizeUNormFloatTo8(CameraDirectionDrawColor[2]),
			255);
		DebugData.CameraDirectionDrawDistance = CameraDirectionDrawDistance;
		DebugData.CameraDirectionDrawSize = CameraDirectionDrawSize;
	}

	ImGui::Checkbox("Draw Camera Point", &bDrawCameraPoint);
	if (bDrawCameraPoint)
	{
		ImGui::InputFloat("Camera Point Size", &CameraPointDrawSize, 0.01f, 1.f);
		ImGui::InputFloat("Offset", &CameraPointDrawOffset, 1.f, 5.f);
		ImGui::ColorEdit3("Camera Point DebugDraw Color", CameraPointDrawColor);

		FVector EyeLoc;
		FRotator EyeRot;
		P->GetActorEyesViewPoint(EyeLoc, EyeRot);

		DebugData.bDrawCameraPoint = true;
		DebugData.CameraPointLocation = EyeLoc + (EyeRot.Vector() * CameraPointDrawOffset);
		DebugData.CameraPointDrawSize = CameraPointDrawSize;
		DebugData.CameraPointColor = FColor(
			FColor::QuantizeUNormFloatTo8(CameraPointDrawColor[0]),
			FColor::QuantizeUNormFloatTo8(CameraPointDrawColor[1]),
			FColor::QuantizeUNormFloatTo8(CameraPointDrawColor[2]),
			255);
	}

	const FArcGunRecoilInstance_Base* Recoil = GunState->GetGunRecoil<FArcGunRecoilInstance_Base>();
	if (Recoil)
	{

	}
	ImGui::End();

	EnsureDrawActor(World);
	if (DrawComponent.IsValid())
	{
		DrawComponent->UpdateGunDebug(DebugData);
	}
}

void FArcGunDebugger::EnsureDrawActor(UWorld* World)
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
	NewActor->SetActorLabel(TEXT("GunDebuggerDraw"));
#endif

	UArcGunDebuggerDrawComponent* NewComponent = NewObject<UArcGunDebuggerDrawComponent>(NewActor, UArcGunDebuggerDrawComponent::StaticClass());
	NewComponent->RegisterComponent();
	NewActor->AddInstanceComponent(NewComponent);

	DrawActor = NewActor;
	DrawComponent = NewComponent;
}

void FArcGunDebugger::DestroyDrawActor()
{
	if (DrawActor.IsValid())
	{
		DrawActor->Destroy();
	}
	DrawActor.Reset();
	DrawComponent.Reset();
}
