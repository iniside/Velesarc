// Copyright Lukasz Baran. All Rights Reserved.

#include "Debug/ArcRecoilDebugScreen.h"

#include "ArcRecoilApi.h"
#include "ArcRecoilEntityResolve.h"
#include "ArcRecoilImpulse.h"
#include "ArcRecoilModule.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Fragments/ArcRecoilFragments.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Mass/EntityManager.h"
#include "SceneView.h"

namespace ArcRecoilDebugScreenPrivate
{
	constexpr float TraceDistance = 10000.0f;
	constexpr float LookMarkerHalfSize = 7.0f;
	constexpr float ImpactMarkerHalfSize = 5.0f;
	constexpr int32 CircleSegments = 32;
	constexpr float MinCircleRadiusPx = 2.0f;

	const FLinearColor LookColor(0.2f, 1.0f, 0.2f, 1.0f);
	const FLinearColor ImpactHitColor(1.0f, 0.15f, 0.15f, 1.0f);
	const FLinearColor ImpactMissColor(1.0f, 0.0f, 1.0f, 1.0f);
	const FLinearColor SpreadColor(1.0f, 0.6f, 0.1f, 1.0f);
	const FLinearColor TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	FDelegateHandle DrawHandle;

	bool bLoggedCallbackFired = false;
	bool bLoggedEntityResolved = false;
	bool bLoggedResolveFailure = false;

	bool ProjectPoint(const UCanvas& Canvas, const FVector& WorldPoint, FVector2D& OutScreenPos)
	{
		if (Canvas.SceneView == nullptr)
		{
			return false;
		}

		const FPlane Projected = Canvas.SceneView->Project(WorldPoint);
		if (Projected.W <= 0.0f)
		{
			return false;
		}

		const FVector ScreenPoint = Canvas.Project(WorldPoint, false);
		OutScreenPos = FVector2D(ScreenPoint.X, ScreenPoint.Y);

		return true;
	}

	bool TraceShot(const UWorld& World,
		const FVector& Start,
		const FVector& Direction,
		const AActor* IgnoreActor,
		FVector& OutPoint)
	{
		const FVector End = Start + Direction * TraceDistance;

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(IgnoreActor);

		if (World.LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.bBlockingHit)
		{
			OutPoint = Hit.ImpactPoint;
			return true;
		}

		OutPoint = End;

		return false;
	}

	void DrawFilledBox(UCanvas& Canvas, const FVector2D& Center, const float HalfSize, const FLinearColor& Color)
	{
		// Null texture resolves to GWhiteTexture, which is how UCanvas fills a solid rect.
		Canvas.K2_DrawTexture(nullptr,
			FVector2D(Center.X - HalfSize, Center.Y - HalfSize),
			FVector2D(2.0f * HalfSize, 2.0f * HalfSize),
			FVector2D::ZeroVector, FVector2D::UnitVector, Color, BLEND_Opaque);
	}

	void DrawCircle(UCanvas& Canvas, const FVector2D& Center, const float Radius, const FLinearColor& Color)
	{
		FVector2D Previous(Center.X + Radius, Center.Y);
		for (int32 Segment = 1; Segment <= CircleSegments; ++Segment)
		{
			const float Angle = 2.0f * UE_PI * static_cast<float>(Segment) / static_cast<float>(CircleSegments);
			const FVector2D Current(Center.X + Radius * FMath::Cos(Angle), Center.Y + Radius * FMath::Sin(Angle));
			Canvas.K2_DrawLine(Previous, Current, 1.0f, Color);
			Previous = Current;
		}
	}

	void DrawOverlay(UCanvas* Canvas, APlayerController* PlayerController)
	{
		if (CVarArcRecoilDebug.GetValueOnGameThread() == 0)
		{
			bLoggedCallbackFired = false;
			bLoggedEntityResolved = false;
			bLoggedResolveFailure = false;
			return;
		}

		if (!bLoggedCallbackFired)
		{
			bLoggedCallbackFired = true;
			UE_LOGF(LogArcRecoil, Log,"DebugScreen: draw callback fired (Canvas=%ls PlayerController=%ls).",
				Canvas != nullptr ? TEXT("valid") : TEXT("null"),
				PlayerController != nullptr ? TEXT("valid") : TEXT("null"));
		}

		if (Canvas == nullptr || PlayerController == nullptr || !PlayerController->IsLocalController())
		{
			return;
		}

		APawn* Pawn = PlayerController->GetPawn();
		UWorld* World = PlayerController->GetWorld();
		if (Pawn == nullptr || World == nullptr || PlayerController->PlayerCameraManager == nullptr)
		{
			return;
		}

		ArcRecoil::FResolvedRecoilEntity Resolved;
		const ArcRecoil::EEntityResolveResult ResolveResult = ArcRecoil::ResolveRecoilEntity(Pawn, Resolved);
		if (ResolveResult != ArcRecoil::EEntityResolveResult::Resolved)
		{
			if (!bLoggedResolveFailure)
			{
				bLoggedResolveFailure = true;
				UE_LOGF(LogArcRecoil, Log,"DebugScreen: entity resolution failed on pawn '%ls' (%ls) - overlay stops here.",
					*GetNameSafe(Pawn), ArcRecoil::DescribeResolveResult(ResolveResult));
			}
			return;
		}

		if (!bLoggedEntityResolved)
		{
			bLoggedEntityResolved = true;
			UE_LOGF(LogArcRecoil, Log,"DebugScreen: entity %d resolved on pawn '%ls' - markers are being drawn.",
				Resolved.Entity.Index, *GetNameSafe(Pawn));
		}

		FVector2f AngularOffsetDeg = FVector2f::ZeroVector;
		float SpreadHalfAngleDeg = 0.0f;
		float SpreadExponent = 1.0f;
		if (!ArcRecoil::GetAimPerturbation(*Resolved.EntityManager, Resolved.Entity,
			AngularOffsetDeg, SpreadHalfAngleDeg, SpreadExponent))
		{
			return;
		}

		const FArcRecoilStateFragment* State =
			Resolved.EntityManager->GetFragmentDataPtr<FArcRecoilStateFragment>(Resolved.Entity);
		if (State == nullptr)
		{
			return;
		}

		FVector CameraLocation = FVector::ZeroVector;
		FRotator CameraRotation = FRotator::ZeroRotator;
		PlayerController->PlayerCameraManager->GetCameraViewPoint(CameraLocation, CameraRotation);

		const FVector CameraForward = CameraRotation.Vector();
		const FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);

		// Zero spread makes the cone tilt/spin draws inert, so this is the cone centre with no jitter.
		FRandomStream CenterStream(0);
		const FVector ShotDirection = ArcRecoil::PerturbAimDirection(
			CameraForward, AngularOffsetDeg, 0.0f, SpreadExponent, CenterStream);

		FVector LookPoint = FVector::ZeroVector;
		TraceShot(*World, CameraLocation, CameraForward, Pawn, LookPoint);

		FVector ImpactPoint = FVector::ZeroVector;
		const bool bImpactHit = TraceShot(*World, CameraLocation, ShotDirection, Pawn, ImpactPoint);

		FVector2D LookScreenPos = FVector2D::ZeroVector;
		const bool bLookVisible = ProjectPoint(*Canvas, LookPoint, LookScreenPos);

		FVector2D ImpactScreenPos = FVector2D::ZeroVector;
		const bool bImpactVisible = ProjectPoint(*Canvas, ImpactPoint, ImpactScreenPos);

		if (bLookVisible)
		{
			DrawFilledBox(*Canvas, LookScreenPos, LookMarkerHalfSize, LookColor);
		}

		if (bImpactVisible)
		{
			const FLinearColor& ImpactColor = bImpactHit ? ImpactHitColor : ImpactMissColor;
			DrawFilledBox(*Canvas, ImpactScreenPos, ImpactMarkerHalfSize, ImpactColor);

			const float ImpactDistance = static_cast<float>((ImpactPoint - CameraLocation).Size());
			const float SpreadWorldRadius = ImpactDistance * FMath::Tan(FMath::DegreesToRadians(SpreadHalfAngleDeg));

			// Project the rim point instead of deriving pixels from FOV: stays correct under ADS zoom and any aspect.
			FVector2D RimScreenPos = FVector2D::ZeroVector;
			if (SpreadWorldRadius > 0.0f
				&& ProjectPoint(*Canvas, ImpactPoint + CameraRight * SpreadWorldRadius, RimScreenPos))
			{
				const float RadiusPx = static_cast<float>((RimScreenPos - ImpactScreenPos).Size());
				if (RadiusPx >= MinCircleRadiusPx)
				{
					DrawCircle(*Canvas, ImpactScreenPos, RadiusPx, SpreadColor);
				}
			}
		}

		UFont* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
		if (Font == nullptr || !bLookVisible)
		{
			return;
		}

		const FVector2D TextOrigin(LookScreenPos.X + 18.0f, LookScreenPos.Y + 18.0f);
		Canvas->K2_DrawText(Font, FString::Printf(TEXT("Heat %.2f"), State->Heat),
			TextOrigin, FVector2D::UnitVector, TextColor);
		Canvas->K2_DrawText(Font, FString::Printf(TEXT("Offset %.2f deg"), AngularOffsetDeg.Size()),
			FVector2D(TextOrigin.X, TextOrigin.Y + 14.0f), FVector2D::UnitVector, TextColor);
		Canvas->K2_DrawText(Font, FString::Printf(TEXT("Spread %.2f deg"), SpreadHalfAngleDeg),
			FVector2D(TextOrigin.X, TextOrigin.Y + 28.0f), FVector2D::UnitVector, TextColor);
	}
}

const TCHAR* ArcRecoil::DebugScreen::ShowFlagName()
{
	return TEXT("Game");
}

bool ArcRecoil::DebugScreen::Register()
{
	if (ArcRecoilDebugScreenPrivate::DrawHandle.IsValid())
	{
		return true;
	}

	ArcRecoilDebugScreenPrivate::DrawHandle = UDebugDrawService::Register(ShowFlagName(),
		FDebugDrawDelegate::CreateStatic(&ArcRecoilDebugScreenPrivate::DrawOverlay));

	return ArcRecoilDebugScreenPrivate::DrawHandle.IsValid();
}

void ArcRecoil::DebugScreen::Unregister()
{
	if (!ArcRecoilDebugScreenPrivate::DrawHandle.IsValid())
	{
		return;
	}

	UDebugDrawService::Unregister(ArcRecoilDebugScreenPrivate::DrawHandle);
	ArcRecoilDebugScreenPrivate::DrawHandle.Reset();
}
