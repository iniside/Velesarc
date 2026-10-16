#include "Nodes/ArcTargetingSort_AimScreen.h"

#include "ArcTargetingContext.h"
#include "ArcTargetingDebug.h"
#include "DebugDraw.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace ArcTargetingSortAimScreenPrivate
{
	APlayerController* ResolvePlayerController(const FArcTargetingContext& Ctx)
	{
		AActor* Source = Ctx.SourceActor.Get();
		if (Source == nullptr) { return nullptr; }
		if (APawn* Pawn = Cast<APawn>(Source))
		{
			return Cast<APlayerController>(Pawn->GetController());
		}
		return Cast<APlayerController>(Source);
	}
}

void FArcTargetingSort_AimScreen::Execute(FArcTargetingContext& Ctx, TArray<FArcTargetCandidate>& Candidates) const
{
	using namespace ArcTargetingSortAimScreenPrivate;

	APlayerController* PC = ResolvePlayerController(Ctx);

	float Wd = DistanceWeight;
	float Wa = AngleWeight;
	float Ws = (PC != nullptr) ? ScreenWeight : 0.f;

	int32 ViewW = 0;
	int32 ViewH = 0;
	if (PC != nullptr)
	{
		PC->GetViewportSize(ViewW, ViewH);
		if (ViewW <= 0 || ViewH <= 0)
		{
			Ws = 0.f;
		}
	}

	const float MaxAngleRad = FMath::DegreesToRadians(FMath::Max(MaxAngleDegrees, 1.f));
	const FVector AimDir = Ctx.SourceDirection.GetSafeNormal();

	for (FArcTargetCandidate& C : Candidates)
	{
		const FVector ToCand = C.HitResult.ImpactPoint - Ctx.SourceLocation;
		const float Dist = ToCand.Size();

		const float DistanceTerm = 1.f - FMath::Clamp(Dist / MaxDistance, 0.f, 1.f);

		float AngleTerm = 0.f;
		if (!AimDir.IsZero() && Dist > KINDA_SMALL_NUMBER)
		{
			const float CosA = FVector::DotProduct(AimDir, ToCand / Dist);
			const float Angle = FMath::Acos(FMath::Clamp(CosA, -1.f, 1.f));
			AngleTerm = 1.f - FMath::Clamp(Angle / MaxAngleRad, 0.f, 1.f);
		}

		float ScreenTerm = 0.f;
		if (Ws > 0.f)
		{
			FVector2D ScreenPos;
			const bool bProjected = UGameplayStatics::ProjectWorldToScreen(PC, C.HitResult.ImpactPoint, ScreenPos);
			if (bProjected)
			{
				const float Nx = (ScreenPos.X / float(ViewW)) - 0.5f;
				const float Ny = (ScreenPos.Y / float(ViewH)) - 0.5f;
				const float NormDist = FMath::Sqrt(Nx * Nx + Ny * Ny);
				ScreenTerm = 1.f - FMath::Clamp(NormDist / MaxScreenRadius, 0.f, 1.f);
			}
		}

		const float SumW = Wd + Wa + Ws;
		if (SumW > KINDA_SMALL_NUMBER)
		{
			C.Score = (Wd * DistanceTerm + Wa * AngleTerm + Ws * ScreenTerm) / SumW;
		}
	}

	Candidates.StableSort([](const FArcTargetCandidate& A, const FArcTargetCandidate& B)
	{
		return A.Score > B.Score;
	});
}

#if UE_DEBUG_RENDERING_ENABLED

void FArcTargetingSort_AimScreen::DrawDebug(
	const FArcTargetingContext& Ctx,
	TConstArrayView<FArcTargetCandidate> CandidatesBefore,
	TConstArrayView<FArcTargetCandidate> CandidatesAfter) const
{
	const FSceneInterface* Scene = GetTargetingDebugScene(Ctx);

	const FColor Color = (DebugColor == FColor()) ? FColor::Magenta : DebugColor;

	UE::DebugDraw::Arrow(Scene,
		Ctx.SourceLocation,
		Ctx.SourceLocation + Ctx.SourceDirection * 200.f,
		12.f,
		Color);

	for (const FArcTargetCandidate& C : CandidatesAfter)
	{
		UE::DebugDraw::Sphere(Scene, C.HitResult.ImpactPoint, 10.f, 12, Color);
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
