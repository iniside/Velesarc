// Copyright Lukasz Baran. All Rights Reserved.

#include "River/ArcWaterRiverModifier.h"

#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"

#include "ArcTerrainActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "River/ArcWaterRiver.h"
#include "River/ArcWaterRiverChannel.h"
#include "River/ArcWaterRiverComponent.h"
#include "River/ArcWaterRiverSplineModifier.h"

#if WITH_EDITOR
#include "DebugDraw.h"
#include "PrimitiveDrawingUtils.h"
#endif

namespace ArcWaterRiverModifierLocal_4e7c
{
	bool Fail(const FString& Message, FText& OutError)
	{
		OutError = FText::FromString(Message);
		return false;
	}

	bool CheckProgress(TFunctionRef<bool()> ShouldContinue, int64& InOutWorkCount)
	{
		const bool bShouldCheck = (InOutWorkCount++ & 255ll) == 0;
		return !bShouldCheck || ShouldContinue();
	}

	bool DeriveSampleSpacing(const FArcWaterRiverGeneratedData& Data, double TerrainTexelCm,
		double& OutSampleSpacingCm, FText& OutError)
	{
		if (!FMath::IsFinite(TerrainTexelCm) || TerrainTexelCm <= 0.0 ||
			!FMath::IsFinite(Data.MinimumUntaperedHalfWidthCm) ||
			Data.MinimumUntaperedHalfWidthCm <= 0.0f ||
			!FMath::IsFinite(Data.MinimumUntaperedFalloffCm) ||
			Data.MinimumUntaperedFalloffCm <= 0.0f)
		{
			return Fail(TEXT("River field resolution inputs must be finite and positive."), OutError);
		}
		OutSampleSpacingCm = FMath::Min3(
			TerrainTexelCm,
			static_cast<double>(Data.MinimumUntaperedHalfWidthCm) * 0.25,
			static_cast<double>(Data.MinimumUntaperedFalloffCm) * 0.5);
		return FMath::IsFinite(OutSampleSpacingCm) && OutSampleSpacingCm > 0.0;
	}

	bool RecenterGeneratedData(FArcWaterRiverGeneratedData& Data, const FVector& Center,
		TFunctionRef<bool()> ShouldContinue, FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN RecenterGeneratedData t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END RecenterGeneratedData elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		if (!ShouldContinue())
		{
			return Fail(TEXT("River generation was cancelled."), OutError);
		}

		int64 WorkCount = 0;
		for (FArcWaterRiverGeneratedReach& Reach : Data.Reaches)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			for (FVector& Point : Reach.SplinePoints)
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				if (!CheckProgress(ShouldContinue, WorkCount))
				{
					return Fail(TEXT("River generation was cancelled."), OutError);
				}
				Point.X -= Center.X;
				Point.Y -= Center.Y;
				Point.Z -= Center.Z;
			}
			for (FArcWaterRiverChannelSection& Section : Reach.Sections)
			{
				if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
				{
					RiverLoopHeartbeat = FPlatformTime::Seconds();
					UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
				}
				if (!CheckProgress(ShouldContinue, WorkCount))
				{
					return Fail(TEXT("River generation was cancelled."), OutError);
				}
				Section.Position.X -= Center.X;
				Section.Position.Y -= Center.Y;
				Section.Position.Z -= Center.Z;
			}
		}
		for (FVector2D& JunctionPosition : Data.JunctionPositions)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if (!CheckProgress(ShouldContinue, WorkCount))
			{
				return Fail(TEXT("River generation was cancelled."), OutError);
			}
			JunctionPosition -= FVector2D(Center.X, Center.Y);
		}
		Data.CoverageCenterXY = FVector2D::ZeroVector;
		return true;
	}

	bool ValidateFrameTransform(const FTransform& Transform, FText& OutError)
	{
		const FVector Translation = Transform.GetTranslation();
		const FVector Scale = Transform.GetScale3D();
		const FQuat Rotation = Transform.GetRotation();
		if (Translation.ContainsNaN() || Scale.ContainsNaN() || Rotation.ContainsNaN())
		{
			return Fail(TEXT("GeneratedFrameWorldTransform must be finite."), OutError);
		}
		if (Scale.X != 1.0 || Scale.Y != 1.0 || Scale.Z != 1.0)
		{
			return Fail(TEXT("GeneratedFrameWorldTransform scale must be exactly one on every axis."), OutError);
		}
		if (Rotation.SizeSquared() <= UE_DOUBLE_SMALL_NUMBER)
		{
			return Fail(TEXT("GeneratedFrameWorldTransform rotation must be valid."), OutError);
		}
		return true;
	}

	bool ResolveTerrainTexelCm(const UArcWaterRiverModifier& Modifier,
		double& OutTerrainTexelCm, FText& OutError)
	{
		const AArcTerrain* Terrain = Modifier.AffectedTerrain.Get();
		if (Terrain == nullptr)
		{
			return Fail(TEXT("AffectedTerrain must resolve before river generation."), OutError);
		}
		const double LocalTexelCm = static_cast<double>(Terrain->Config.TexelSizeCm());
		const FVector TerrainScale = Terrain->GetTerrainTransform().GetScale3D();
		if (TerrainScale.ContainsNaN() || TerrainScale.X <= 0.0 || TerrainScale.Y <= 0.0)
		{
			return Fail(TEXT("AffectedTerrain XY scale must be finite and positive."), OutError);
		}
		const double MinimumXYScale = FMath::Min(TerrainScale.X, TerrainScale.Y);
		const double TerrainTexelCm = LocalTexelCm * MinimumXYScale;
		if (!FMath::IsFinite(TerrainTexelCm) || TerrainTexelCm <= 0.0)
		{
			return Fail(TEXT("AffectedTerrain has an invalid physical height texel spacing."), OutError);
		}
		OutTerrainTexelCm = TerrainTexelCm;
		return true;
	}

#if WITH_EDITOR
	constexpr int32 RiverPreviewCoreProfileSegments = 8;
	constexpr int32 RiverPreviewBankProfileSegments = 4;

	float PreviewSmoothStep(float Value)
	{
		const float T = FMath::Clamp(Value, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	void SnapshotPreviewCurve(FRuntimeFloatCurve& Curve)
	{
		const FRichCurve CurveData = *Curve.GetRichCurveConst();
		Curve.ExternalCurve = nullptr;
		Curve.EditorCurveData = CurveData;
	}

	bool PreviewCurvesEqual(const FRichCurve& A, const FRichCurve& B)
	{
		return A.GetDefaultValue() == B.GetDefaultValue()
			&& A.PreInfinityExtrap == B.PreInfinityExtrap
			&& A.PostInfinityExtrap == B.PostInfinityExtrap
			&& A.GetConstRefOfKeys() == B.GetConstRefOfKeys();
	}

	bool PreviewSettingsEqual(const FArcWaterRiverModifierSettings& A,
		const FArcWaterRiverModifierSettings& B)
	{
		return A.RiversThreshold == B.RiversThreshold
			&& A.HalfWidthCm == B.HalfWidthCm
			&& PreviewCurvesEqual(*A.WidthScaleBySlope.GetRichCurveConst(), *B.WidthScaleBySlope.GetRichCurveConst())
			&& PreviewCurvesEqual(*A.WidthScaleByElevation.GetRichCurveConst(), *B.WidthScaleByElevation.GetRichCurveConst())
			&& A.DepthCm == B.DepthCm
			&& A.FalloffCm == B.FalloffCm
			&& A.BankFreeboardCm == B.BankFreeboardCm
			&& A.SlopeSampleDistanceCm == B.SlopeSampleDistanceCm
			&& A.SplineToleranceCm == B.SplineToleranceCm
			&& A.MaxStepCm == B.MaxStepCm
			&& A.FlowSpeedCmS == B.FlowSpeedCmS
			&& A.TileCm == B.TileCm
			&& A.WidthSegments == B.WidthSegments;
	}

	void AppendPreviewLine(TArray<FVector>& Lines, const FVector& Start, const FVector& End)
	{
		Lines.Add(Start);
		Lines.Add(End);
	}

	void SubmitPreviewLines(const FSceneInterface* Scene, const TArray<FVector>& Lines,
		const FColor& Color, float Thickness, UE::DebugDraw::EDepthTest Depth)
	{
#if UE_DEBUG_RENDERING_ENABLED
		for (int32 Index = 0; Index + 1 < Lines.Num(); Index += 2)
		{
			UE::DebugDraw::Line(Scene, Lines[Index], Lines[Index + 1], Color, Thickness, Depth);
		}
#endif
	}
#endif
}

UArcWaterRiverModifier::UArcWaterRiverModifier()
{
	Priority = 110.0;
	BlendMode = EArcTerrainBlendMode::Min;
#if WITH_EDITOR
	bTickInEditor = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
#endif
}

int32 UArcWaterRiverModifier::GetGeneratedSplineModifierCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<AArcWaterRiver>& RiverActor : GeneratedRiverActors)
	{
		if (IsValid(RiverActor) && RiverActor->GetTerrainModifier() != nullptr)
		{
			++Count;
		}
	}
	return Count;
}

FIntPoint UArcWaterRiverModifier::GetGeneratedMaxFieldDims() const
{
	FIntPoint MaxDims = FIntPoint::ZeroValue;
	for (const TObjectPtr<AArcWaterRiver>& RiverActor : GeneratedRiverActors)
	{
		const UArcWaterRiverSplineModifier* const Modifier = IsValid(RiverActor)
			? RiverActor->GetTerrainModifier()
			: nullptr;
		if (Modifier != nullptr)
		{
			const FIntPoint Dims = Modifier->GetFieldDims();
			MaxDims.X = FMath::Max(MaxDims.X, Dims.X);
			MaxDims.Y = FMath::Max(MaxDims.Y, Dims.Y);
		}
	}
	return MaxDims;
}

double UArcWaterRiverModifier::GetGeneratedFieldSpacingCm() const
{
	for (const TObjectPtr<AArcWaterRiver>& RiverActor : GeneratedRiverActors)
	{
		const UArcWaterRiverSplineModifier* const Modifier = IsValid(RiverActor)
			? RiverActor->GetTerrainModifier()
			: nullptr;
		if (Modifier != nullptr)
		{
			return Modifier->GetSampleSpacingCm();
		}
	}
	return 0.0;
}

int64 UArcWaterRiverModifier::GetGeneratedEstimatedRasterBytes() const
{
	int64 TotalBytes = 0;
	for (const TObjectPtr<AArcWaterRiver>& RiverActor : GeneratedRiverActors)
	{
		const UArcWaterRiverSplineModifier* const Modifier = IsValid(RiverActor)
			? RiverActor->GetTerrainModifier()
			: nullptr;
		if (Modifier != nullptr)
		{
			TotalBytes += Modifier->GetEstimatedRasterBytes();
		}
	}
	return TotalBytes;
}

void UArcWaterRiverModifier::GetResolvedProfileRanges(float& OutMinHalfWidthCm,
	float& OutMaxHalfWidthCm, float& OutMinFalloffCm, float& OutMaxFalloffCm) const
{
	OutMinHalfWidthCm = TNumericLimits<float>::Max();
	OutMaxHalfWidthCm = 0.0f;
	OutMinFalloffCm = TNumericLimits<float>::Max();
	OutMaxFalloffCm = 0.0f;
	for (const FArcWaterRiverGeneratedReach& Reach : GeneratedData.Reaches)
	{
		for (const FArcWaterRiverChannelSection& Section : Reach.Sections)
		{
			OutMinHalfWidthCm = FMath::Min(OutMinHalfWidthCm, Section.HalfWidthCm);
			OutMaxHalfWidthCm = FMath::Max(OutMaxHalfWidthCm, Section.HalfWidthCm);
			OutMinFalloffCm = FMath::Min(OutMinFalloffCm, Section.FalloffCm);
			OutMaxFalloffCm = FMath::Max(OutMaxFalloffCm, Section.FalloffCm);
		}
	}
	if (OutMinHalfWidthCm == TNumericLimits<float>::Max())
	{
		OutMinHalfWidthCm = 0.0f;
		OutMinFalloffCm = 0.0f;
	}
}

bool UArcWaterRiverModifier::GetGenerationBounds(FBox& OutBounds) const
{
	OutBounds.Init();
	const AActor* const Owner = GetOwner();
	const AArcTerrain* const Terrain = AffectedTerrain.Get();
	if (Owner == nullptr || Terrain == nullptr
		|| !FMath::IsFinite(GenerationBoundsSize.X) || !FMath::IsFinite(GenerationBoundsSize.Y)
		|| GenerationBoundsSize.X <= 0.0 || GenerationBoundsSize.Y <= 0.0)
	{
		return false;
	}

	const FVector TerrainLocalCenter = Terrain->GetTerrainTransform().InverseTransformPosition(
		Owner->GetActorLocation());
	if (!FMath::IsFinite(TerrainLocalCenter.X) || !FMath::IsFinite(TerrainLocalCenter.Y)
		|| !FMath::IsFinite(TerrainLocalCenter.Z))
	{
		return false;
	}

	const FVector2D HalfSize = GenerationBoundsSize * 0.5;
	const FVector2D BoundsMin = FVector2D(TerrainLocalCenter.X, TerrainLocalCenter.Y) - HalfSize;
	const FVector2D BoundsMax = FVector2D(TerrainLocalCenter.X, TerrainLocalCenter.Y) + HalfSize;
	if (!FMath::IsFinite(BoundsMin.X) || !FMath::IsFinite(BoundsMin.Y)
		|| !FMath::IsFinite(BoundsMax.X) || !FMath::IsFinite(BoundsMax.Y)
		|| BoundsMax.X <= BoundsMin.X || BoundsMax.Y <= BoundsMin.Y)
	{
		return false;
	}

	OutBounds = FBox(
		FVector(BoundsMin.X, BoundsMin.Y, TerrainLocalCenter.Z),
		FVector(BoundsMax.X, BoundsMax.Y, TerrainLocalCenter.Z));
	return OutBounds.IsValid != 0;
}

bool UArcWaterRiverModifier::ApplyGeneratedData(FArcWaterRiverGeneratedData InGeneratedData,
	const FTransform& GeneratedFrameWorldTransform, FText& OutError,
	TFunctionRef<bool()> ShouldContinue)
{
	const double RiverLogStart = FPlatformTime::Seconds();
	double RiverLoopHeartbeat = RiverLogStart;
	uint64 RiverLoopVisits = 0;
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN UArcWaterRiverModifier::ApplyGeneratedData t=%.6f", RiverLogStart);
	ON_SCOPE_EXIT
	{
		UE_LOGF(LogTemp, Display,"[RiverGen] END UArcWaterRiverModifier::ApplyGeneratedData elapsed=%.3fs, error=%ls",
			FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
	};
	using namespace ArcWaterRiverModifierLocal_4e7c;

	OutError = FText::GetEmpty();
#if WITH_EDITOR
	ClearRiverPreview();
#endif
	if (!ValidateFrameTransform(GeneratedFrameWorldTransform, OutError))
	{
		return false;
	}
	if (InGeneratedData.Reaches.IsEmpty())
	{
		return Fail(TEXT("GeneratedData must contain reaches."), OutError);
	}
	for (int32 ReachIndex = 0; ReachIndex < InGeneratedData.Reaches.Num(); ++ReachIndex)
	{
		if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
		{
			RiverLoopHeartbeat = FPlatformTime::Seconds();
			UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
		}
		if (!ShouldContinue())
		{
			return Fail(TEXT("River generation was cancelled."), OutError);
		}
		const FArcWaterRiverGeneratedReach& Reach = InGeneratedData.Reaches[ReachIndex];
		if (Reach.SplinePoints.Num() < 2 || Reach.SplinePoints.Num() != Reach.Sections.Num())
		{
			return Fail(FString::Printf(
				TEXT("GeneratedData reach %d must contain matching spline points and channel sections with at least two entries."),
				ReachIndex), OutError);
		}
	}
	if (InGeneratedData.CoverageCenterXY.ContainsNaN())
	{
		return Fail(TEXT("GeneratedData.CoverageCenterXY must be finite."), OutError);
	}
	FTransform CenteredWorldTransform = GeneratedFrameWorldTransform;
	CenteredWorldTransform.SetScale3D(FVector::OneVector);
	FQuat CenteredRotation = CenteredWorldTransform.GetRotation();
	CenteredRotation.Normalize();
	CenteredWorldTransform.SetRotation(CenteredRotation);
	FVector RecenterCenter(InGeneratedData.CoverageCenterXY, 0.0);
	AActor* const Owner = GetOwner();
	const bool bIsActorRoot = Owner != nullptr && Owner->GetRootComponent() == this;
	if (bIsActorRoot)
	{
		const FVector ActorWorldLocation = Owner->GetActorLocation();
		RecenterCenter = CenteredWorldTransform.InverseTransformPosition(ActorWorldLocation);
		if (!FMath::IsFinite(RecenterCenter.X) || !FMath::IsFinite(RecenterCenter.Y)
			|| !FMath::IsFinite(RecenterCenter.Z))
		{
			return Fail(TEXT("The actor position must be finite."), OutError);
		}

		const FVector2D OldHalfCoverage = InGeneratedData.CoverageXY * 0.5;
		const FVector2D OldCoverageMin = InGeneratedData.CoverageCenterXY - OldHalfCoverage;
		const FVector2D OldCoverageMax = InGeneratedData.CoverageCenterXY + OldHalfCoverage;
		InGeneratedData.CoverageXY = FVector2D(
			2.0 * FMath::Max(FMath::Abs(OldCoverageMin.X - RecenterCenter.X),
				FMath::Abs(OldCoverageMax.X - RecenterCenter.X)),
			2.0 * FMath::Max(FMath::Abs(OldCoverageMin.Y - RecenterCenter.Y),
				FMath::Abs(OldCoverageMax.Y - RecenterCenter.Y)));
		CenteredWorldTransform.SetTranslation(ActorWorldLocation);
	}
	else
	{
		const FVector CenterOffset = CenteredWorldTransform.TransformVector(RecenterCenter);
		CenteredWorldTransform.AddToTranslation(CenterOffset);
	}

	if (!RecenterGeneratedData(InGeneratedData, RecenterCenter, ShouldContinue, OutError))
	{
		return false;
	}

	if (!ShouldContinue())
	{
		return Fail(TEXT("River generation was cancelled."), OutError);
	}

	GeneratedData = MoveTemp(InGeneratedData);
	CoverageXY = GeneratedData.CoverageXY;
	GeneratedDataSnapshot = MakeShared<const FArcWaterRiverGeneratedData>(GeneratedData);
	bGeneratedDataNeedsRebuild = false;
	BlendMode = EArcTerrainBlendMode::Min;
	{
		TGuardValue<bool> ApplyingGeneratedState(bApplyingGeneratedState, true);
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN SetWorldTransform(CenteredWorldTransform); t=%.6f", FPlatformTime::Seconds());
		SetWorldTransform(CenteredWorldTransform);
		UE_LOGF(LogTemp, Display,"[RiverGen] END SetWorldTransform(CenteredWorldTransform); t=%.6f", FPlatformTime::Seconds());
	}
	if (!RebuildWaterComponents(OutError))
	{
		ClearGeneratedRivers();
		return false;
	}
	return true;
}

bool UArcWaterRiverModifier::RebuildWaterComponents(FText& OutError)
{
	const double RiverLogStart = FPlatformTime::Seconds();
	double RiverLoopHeartbeat = RiverLogStart;
	uint64 RiverLoopVisits = 0;
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN UArcWaterRiverModifier::RebuildWaterComponents t=%.6f", RiverLogStart);
	ON_SCOPE_EXIT
	{
		UE_LOGF(LogTemp, Display,"[RiverGen] END UArcWaterRiverModifier::RebuildWaterComponents elapsed=%.3fs, error=%ls",
			FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
	};
	using namespace ArcWaterRiverModifierLocal_4e7c;

	OutError = FText::GetEmpty();
	if (bRebuildingOutputs)
	{
		bGeneratedDataNeedsRebuild = true;
		return Fail(TEXT("River water component rebuild is already in progress."), OutError);
	}
	TGuardValue<bool> RebuildingOutputs(bRebuildingOutputs, true);
	DestroyGeneratedRiverActors();
	if (!IsOrdinaryWorldInstance() || bIsDisabled)
	{
		return true;
	}
	if (!GeneratedDataSnapshot.IsValid())
	{
		if (GeneratedData.Reaches.IsEmpty())
		{
			return true;
		}
		bGeneratedDataNeedsRebuild = true;
		return Fail(TEXT("The saved river result could not be prepared for water component rebuild."), OutError);
	}

	AActor* const Owner = GetOwner();
	if (Owner == nullptr)
	{
		bGeneratedDataNeedsRebuild = true;
		return Fail(TEXT("River water components require a valid owning actor."), OutError);
	}
	UWorld* const World = GetWorld();
	if (World == nullptr)
	{
		bGeneratedDataNeedsRebuild = true;
		return Fail(TEXT("River water actors require a valid world."), OutError);
	}
	double TerrainTexelCm = 0.0;
	if (!ResolveTerrainTexelCm(*this, TerrainTexelCm, OutError))
	{
		bGeneratedDataNeedsRebuild = true;
		return false;
	}
	double SampleSpacingCm = 0.0;
	if (!DeriveSampleSpacing(*GeneratedDataSnapshot, TerrainTexelCm, SampleSpacingCm, OutError))
	{
		bGeneratedDataNeedsRebuild = true;
		return false;
	}
	for (int32 ReachIndex = 0; ReachIndex < GeneratedDataSnapshot->Reaches.Num(); ++ReachIndex)
	{
		if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
		{
			RiverLoopHeartbeat = FPlatformTime::Seconds();
			UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
		}
		const FArcWaterRiverGeneratedReach& Reach = GeneratedDataSnapshot->Reaches[ReachIndex];
		UE_LOGF(LogTemp, Display,"[RiverGen] WATER REACH index=%d/%d points=%d sections=%d t=%.6f", ReachIndex, GeneratedDataSnapshot->Reaches.Num(), Reach.SplinePoints.Num(), Reach.Sections.Num(), FPlatformTime::Seconds());
		if (Reach.SplinePoints.Num() < 2 || Reach.SplinePoints.Num() != Reach.Sections.Num())
		{
			DestroyGeneratedRiverActors();
			bGeneratedDataNeedsRebuild = true;
			return Fail(FString::Printf(
				TEXT("Saved river reach %d must contain matching spline points and channel sections with at least two entries."),
				ReachIndex), OutError);
		}

		bool bInitialized = false;
		bool bTerrainInitialized = false;
		UArcWaterRiverComponent* InitializedRiverComponent = nullptr;
		FText TerrainInitializationError;
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = FName(*FString::Printf(TEXT("GeneratedRiver_%d"), ReachIndex));
		SpawnParameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
		SpawnParameters.Owner = Owner;
		SpawnParameters.OverrideLevel = Owner->GetLevel();
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags = static_cast<EObjectFlags>(RF_Transient | RF_DuplicateTransient);
#if WITH_EDITOR
		SpawnParameters.bCreateActorPackage = false;
		SpawnParameters.bHideFromSceneOutliner = false;
#endif
		SpawnParameters.CustomPreSpawnInitialization =
			[this, &Reach, ReachIndex, SampleSpacingCm, &bInitialized, &bTerrainInitialized,
				&InitializedRiverComponent, &TerrainInitializationError](AActor* SpawnedActor)
		{
			AArcWaterRiver* const RiverActor = Cast<AArcWaterRiver>(SpawnedActor);
			UArcWaterRiverComponent* const River = RiverActor != nullptr
				? Cast<UArcWaterRiverComponent>(RiverActor->GetRootComponent())
				: nullptr;
			if (River == nullptr)
			{
				return;
			}

			River->SetupAttachment(this);
			River->SetRelativeTransform(FTransform::Identity);
			River->NewPointHalfWidthCm = Settings.HalfWidthCm;
			River->NewPointFlowSpeedCmS = Settings.FlowSpeedCmS;
			River->MaxStepCm = Settings.MaxStepCm;
			River->WidthSegments = Settings.WidthSegments;
			River->TileCm = Settings.TileCm;
			River->RiverMaterial = RiverMaterial;
			River->UnderwaterMaterial = UnderwaterMaterial;
			River->RiverParameterCollection = RiverParameterCollection;
			River->AbsorptionTint = AbsorptionTint;
			River->ClarityDistanceMeters = ClarityDistanceMeters;
			River->ScatterAlbedo = ScatterAlbedo;
			River->SetSplinePoints(Reach.SplinePoints, ESplineCoordinateSpace::Local, false);
			for (int32 PointIndex = 0; PointIndex < Reach.SplinePoints.Num(); ++PointIndex)
			{
				River->SetSplinePointType(PointIndex, ESplinePointType::Linear, false);
			}
			River->SetClosedLoop(Reach.bClosed, false);

			UArcWaterRiverMetadata* const Metadata = River->GetRiverMetadata();
			if (Metadata == nullptr)
			{
				return;
			}
			Metadata->HalfWidth.Points.Reset(Reach.Sections.Num());
			Metadata->FlowSpeed.Points.Reset(Reach.Sections.Num());
			for (int32 PointIndex = 0; PointIndex < Reach.Sections.Num(); ++PointIndex)
			{
				const FArcWaterRiverChannelSection& Section = Reach.Sections[PointIndex];
				FInterpCurvePoint<float> WidthPoint;
				WidthPoint.InVal = static_cast<float>(PointIndex);
				WidthPoint.OutVal = Section.HalfWidthCm;
				WidthPoint.InterpMode = CIM_Linear;
				Metadata->HalfWidth.Points.Add(WidthPoint);

				FInterpCurvePoint<float> FlowPoint;
				FlowPoint.InVal = static_cast<float>(PointIndex);
				FlowPoint.OutVal = Section.FlowSpeedCmS;
				FlowPoint.InterpMode = CIM_Linear;
				Metadata->FlowSpeed.Points.Add(FlowPoint);
			}
			River->SetGeneratedChannel(GeneratedDataSnapshot.ToSharedRef(), ReachIndex);
			River->UpdateSpline();
			UArcWaterRiverSplineModifier* const TerrainModifier = RiverActor->GetTerrainModifier();
			if (TerrainModifier == nullptr ||
				!TerrainModifier->Initialize(*this, Reach, SampleSpacingCm, TerrainInitializationError))
			{
				return;
			}
			InitializedRiverComponent = River;
			bTerrainInitialized = true;
			bInitialized = true;
		};

		AArcWaterRiver* const RiverActor = World->SpawnActor<AArcWaterRiver>(
			AArcWaterRiver::StaticClass(), GetComponentTransform(), SpawnParameters);
		if (RiverActor != nullptr)
		{
			GeneratedRiverActors.Add(RiverActor);
		}
		if (RiverActor == nullptr || !bInitialized || !bTerrainInitialized ||
			InitializedRiverComponent == nullptr || !InitializedRiverComponent->IsRegistered() ||
			RiverActor->GetTerrainModifier() == nullptr || !RiverActor->GetTerrainModifier()->IsRegistered())
		{
			DestroyGeneratedRiverActors();
			bGeneratedDataNeedsRebuild = true;
			return Fail(TerrainInitializationError.IsEmpty()
				? FString::Printf(TEXT("Failed to initialize the generated river actor for reach %d."), ReachIndex)
				: TerrainInitializationError.ToString(), OutError);
		}
	}
	float MinHalfWidthCm = 0.0f;
	float MaxHalfWidthCm = 0.0f;
	float MinFalloffCm = 0.0f;
	float MaxFalloffCm = 0.0f;
	GetResolvedProfileRanges(MinHalfWidthCm, MaxHalfWidthCm, MinFalloffCm, MaxFalloffCm);
	const FIntPoint MaxFieldDims = GetGeneratedMaxFieldDims();
	UE_LOGF(LogTemp, Display,"[RiverGen] OUTPUT reaches=%d splineModifiers=%d maxFieldDims=%dx%d spacingCm=%.3f estimatedPairBytes=%lld halfWidthCm=%.3f..%.3f falloffCm=%.3f..%.3f",
		GeneratedDataSnapshot->Reaches.Num(), GetGeneratedSplineModifierCount(),
		MaxFieldDims.X, MaxFieldDims.Y, GetGeneratedFieldSpacingCm(),
		GetGeneratedEstimatedRasterBytes(), MinHalfWidthCm, MaxHalfWidthCm,
		MinFalloffCm, MaxFalloffCm);
	return true;
}

void UArcWaterRiverModifier::DestroyGeneratedRiverActors()
{
	const double RiverLogStart = FPlatformTime::Seconds();
	double RiverLoopHeartbeat = RiverLogStart;
	uint64 RiverLoopVisits = 0;
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN UArcWaterRiverModifier::DestroyGeneratedRiverActors t=%.6f", RiverLogStart);
	ON_SCOPE_EXIT
	{
		UE_LOGF(LogTemp, Display,"[RiverGen] END UArcWaterRiverModifier::DestroyGeneratedRiverActors elapsed=%.3fs",
			FPlatformTime::Seconds() - RiverLogStart);
	};
	TArray<TObjectPtr<AArcWaterRiver>> RiverActors = MoveTemp(GeneratedRiverActors);
	GeneratedRiverActors.Reset();
	for (TObjectPtr<AArcWaterRiver>& RiverActor : RiverActors)
	{
		if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
		{
			RiverLoopHeartbeat = FPlatformTime::Seconds();
			UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
		}
		if (IsValid(RiverActor))
		{
			RiverActor->Destroy(false, false);
		}
	}
}

bool UArcWaterRiverModifier::IsOrdinaryWorldInstance() const
{
	const AActor* Owner = GetOwner();
	return IsRegistered() && GetWorld() != nullptr && Owner != nullptr &&
		!IsTemplate() && !Owner->IsTemplate();
}

void UArcWaterRiverModifier::ClearGeneratedData()
{
#if WITH_EDITOR
	ClearRiverPreview();
#endif
	GeneratedData = FArcWaterRiverGeneratedData();
	CoverageXY = FVector2D::ZeroVector;
	GeneratedDataSnapshot.Reset();
	bGeneratedDataNeedsRebuild = true;
}

void UArcWaterRiverModifier::ClearGeneratedRivers()
{
	DestroyGeneratedRiverActors();
	ClearGeneratedData();
	MarkPackageDirty();
}

void UArcWaterRiverModifier::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	ClearRiverPreview();
#endif
	GeneratedDataSnapshot.Reset();
	if (!GeneratedData.Reaches.IsEmpty() && GeneratedData.CoverageCenterXY.IsNearlyZero())
	{
		GeneratedDataSnapshot = MakeShared<const FArcWaterRiverGeneratedData>(GeneratedData);
	}
}

void UArcWaterRiverModifier::OnRegister()
{
	UE_LOGF(LogTemp, Display,"[RiverEdit] OnRegister t=%.6f path=%ls owner=%ls depthCm=%.3f registered=%d template=%d",
		FPlatformTime::Seconds(), *GetPathNameSafe(this), *GetPathNameSafe(GetOwner()),
		Settings.DepthCm, IsRegistered(), IsTemplate());
	{
		TGuardValue<bool> ApplyingGeneratedState(bApplyingGeneratedState, true);
		ConditionalUpdateComponentToWorld();
		const FTransform PreservedWorldTransform = GetComponentTransform();
		SetUsingAbsoluteLocation(false);
		SetUsingAbsoluteRotation(false);
		SetUsingAbsoluteScale(false);
		SetWorldTransform(PreservedWorldTransform);
	}
	BlendMode = EArcTerrainBlendMode::Min;
	Super::OnRegister();
#if WITH_EDITOR
	RefreshExternalCurveBindings();
#endif
	FText IgnoredError;
	if (!RebuildWaterComponents(IgnoredError))
	{
		ClearGeneratedRivers();
	}
}

void UArcWaterRiverModifier::OnUnregister()
{
	UE_LOGF(LogTemp, Display,"[RiverEdit] OnUnregister t=%.6f path=%ls owner=%ls depthCm=%.3f registered=%d valid=%d",
		FPlatformTime::Seconds(), *GetPathNameSafe(this), *GetPathNameSafe(GetOwner()),
		Settings.DepthCm, IsRegistered(), IsValid(this));
#if WITH_EDITOR
	ClearExternalCurveBindings();
	ClearRiverPreview();
#endif
	DestroyGeneratedRiverActors();
	GeneratedDataSnapshot.Reset();
	Super::OnUnregister();
}

void UArcWaterRiverModifier::OnComponentDestroyed(bool bDestroyingHierarchy)
{
#if WITH_EDITOR
	ClearExternalCurveBindings();
	ClearRiverPreview();
#endif
	DestroyGeneratedRiverActors();
	GeneratedDataSnapshot.Reset();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UArcWaterRiverModifier::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags,
	ETeleportType Teleport)
{
	if (bApplyingGeneratedState)
	{
		UPrimitiveComponent::OnUpdateTransform(UpdateTransformFlags, Teleport);
		return;
	}
#if WITH_EDITOR
	ClearRiverPreview();
#endif
	if (IsRegistered() && (!GeneratedData.Reaches.IsEmpty() || !GeneratedRiverActors.IsEmpty()))
	{
		ClearGeneratedRivers();
	}
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
}

FBox UArcWaterRiverModifier::ComputeBounds() const
{
	return FBox(ForceInit);
}

bool UArcWaterRiverModifier::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
	return false;
}

void UArcWaterRiverModifier::OnAffectedTerrainConfigChanged()
{
	ClearGeneratedRivers();
}

#if WITH_EDITOR
void UArcWaterRiverModifier::SetRiverPreviewData(const FArcWaterRiverGeneratedData& InPreviewData,
	const FTransform& InPreviewFrame)
{
	using namespace ArcWaterRiverModifierLocal_4e7c;

	ClearRiverPreview();
	if (InPreviewData.Reaches.IsEmpty() || InPreviewFrame.ContainsNaN())
	{
		return;
	}
	const AActor* const Owner = GetOwner();
	AArcTerrain* const Terrain = AffectedTerrain.Get();
	if (Owner == nullptr || Terrain == nullptr)
	{
		return;
	}

	RiverPreviewData = InPreviewData;
	RiverPreviewFrame = InPreviewFrame;
	RiverPreviewSettingsSnapshot = Settings;
	SnapshotPreviewCurve(RiverPreviewSettingsSnapshot.WidthScaleBySlope);
	SnapshotPreviewCurve(RiverPreviewSettingsSnapshot.WidthScaleByElevation);
	RiverPreviewTerrain = Terrain;
	RiverPreviewTerrainTransform = Terrain->GetTerrainTransform();
	RiverPreviewModifierTransform = GetComponentTransform();
	RiverPreviewOwnerLocation = Owner->GetActorLocation();
	RiverPreviewGenerationBoundsSize = GenerationBoundsSize;
	BuildRiverPreviewGeometry();
	bHasRiverPreviewData = !RiverPreviewChannelIndices.IsEmpty()
		|| !RiverPreviewCenterLines.IsEmpty() || !RiverPreviewFalloffLines.IsEmpty();
	if (bRiverPreviewTruncated)
	{
		UE_LOGF(LogTemp, Warning,"[RiverPreview] %ls drew %d of %lld channel segments and %d of %lld profile sections to stay within DebugDraw capacity.",
			*GetPathName(), RiverPreviewDrawnSegmentCount, RiverPreviewSourceSegmentCount,
			RiverPreviewDrawnCrossSectionCount, RiverPreviewSourceCrossSectionCount);
	}
}

void UArcWaterRiverModifier::RefreshExternalCurveBindings()
{
	UCurveFloat* const SlopeCurve = Settings.WidthScaleBySlope.ExternalCurve;
	UCurveFloat* const ElevationCurve = Settings.WidthScaleByElevation.ExternalCurve;
	if (BoundSlopeWidthCurve.Get() == SlopeCurve
		&& BoundElevationWidthCurve.Get() == (ElevationCurve != SlopeCurve ? ElevationCurve : nullptr))
	{
		return;
	}

	ClearExternalCurveBindings();
	if (!IsRegistered())
	{
		return;
	}
	if (SlopeCurve != nullptr)
	{
		BoundSlopeWidthCurve = SlopeCurve;
		SlopeWidthCurveChangedHandle = SlopeCurve->OnUpdateCurve.AddUObject(
			this, &UArcWaterRiverModifier::HandleExternalCurveChanged);
	}
	if (ElevationCurve != nullptr && ElevationCurve != SlopeCurve)
	{
		BoundElevationWidthCurve = ElevationCurve;
		ElevationWidthCurveChangedHandle = ElevationCurve->OnUpdateCurve.AddUObject(
			this, &UArcWaterRiverModifier::HandleExternalCurveChanged);
	}
}

void UArcWaterRiverModifier::ClearExternalCurveBindings()
{
	if (UCurveFloat* const SlopeCurve = BoundSlopeWidthCurve.Get())
	{
		SlopeCurve->OnUpdateCurve.Remove(SlopeWidthCurveChangedHandle);
	}
	if (UCurveFloat* const ElevationCurve = BoundElevationWidthCurve.Get())
	{
		ElevationCurve->OnUpdateCurve.Remove(ElevationWidthCurveChangedHandle);
	}
	BoundSlopeWidthCurve.Reset();
	BoundElevationWidthCurve.Reset();
	SlopeWidthCurveChangedHandle = FDelegateHandle();
	ElevationWidthCurveChangedHandle = FDelegateHandle();
}

void UArcWaterRiverModifier::HandleExternalCurveChanged(
	UCurveBase* Curve, EPropertyChangeType::Type)
{
	if (Curve == Settings.WidthScaleBySlope.ExternalCurve
		|| Curve == Settings.WidthScaleByElevation.ExternalCurve)
	{
		ClearGeneratedRivers();
	}
}

void UArcWaterRiverModifier::SetRiverPreviewVisible(bool bShouldBeVisible)
{
	if (bShouldBeVisible && !bHasRiverPreviewData && !bGeneratedDataNeedsRebuild && !GeneratedData.Reaches.IsEmpty())
	{
		SetRiverPreviewData(GeneratedData, GetComponentTransform());
	}
	if (bShouldBeVisible && bHasRiverPreviewData && !RiverPreviewInputsStillMatch())
	{
		ClearRiverPreview();
		return;
	}
	bRiverPreviewVisible = bShouldBeVisible;
	SetComponentTickEnabled(bRiverPreviewVisible);
}

void UArcWaterRiverModifier::SetRiverPreviewThroughTerrain(bool bThroughTerrain)
{
	bRiverPreviewThroughTerrain = bThroughTerrain;
}

void UArcWaterRiverModifier::ClearRiverPreview()
{
	bRiverPreviewVisible = false;
	bHasRiverPreviewData = false;
	bRiverPreviewTruncated = false;
	RiverPreviewSourceSegmentCount = 0;
	RiverPreviewDrawnSegmentCount = 0;
	RiverPreviewSourceCrossSectionCount = 0;
	RiverPreviewDrawnCrossSectionCount = 0;
	SetComponentTickEnabled(false);
	RiverPreviewData = FArcWaterRiverGeneratedData();
	RiverPreviewFrame = FTransform::Identity;
	RiverPreviewSettingsSnapshot = FArcWaterRiverModifierSettings();
	RiverPreviewTerrain.Reset();
	RiverPreviewTerrainTransform = FTransform::Identity;
	RiverPreviewModifierTransform = FTransform::Identity;
	RiverPreviewOwnerLocation = FVector::ZeroVector;
	RiverPreviewGenerationBoundsSize = FVector2D::ZeroVector;
	RiverPreviewChannelVertices.Reset();
	RiverPreviewChannelIndices.Reset();
	RiverPreviewCenterLines.Reset();
	RiverPreviewBottomLines.Reset();
	RiverPreviewFalloffLines.Reset();
	RiverPreviewDepthLines.Reset();
}

bool UArcWaterRiverModifier::RiverPreviewInputsStillMatch() const
{
	using namespace ArcWaterRiverModifierLocal_4e7c;

	const AActor* const Owner = GetOwner();
	const AArcTerrain* const Terrain = RiverPreviewTerrain.Get();
	return bHasRiverPreviewData && Owner != nullptr && Terrain != nullptr
		&& AffectedTerrain.Get() == Terrain
		&& Owner->GetActorLocation().Equals(RiverPreviewOwnerLocation, 1.e-6)
		&& GetComponentTransform().Equals(RiverPreviewModifierTransform, 1.e-6)
		&& Terrain->GetTerrainTransform().Equals(RiverPreviewTerrainTransform, 1.e-6)
		&& GenerationBoundsSize == RiverPreviewGenerationBoundsSize
		&& PreviewSettingsEqual(Settings, RiverPreviewSettingsSnapshot);
}

void UArcWaterRiverModifier::BuildRiverPreviewGeometry()
{
	using namespace ArcWaterRiverModifierLocal_4e7c;

	RiverPreviewChannelVertices.Reset();
	RiverPreviewChannelIndices.Reset();
	RiverPreviewCenterLines.Reset();
	RiverPreviewBottomLines.Reset();
	RiverPreviewFalloffLines.Reset();
	RiverPreviewDepthLines.Reset();
	RiverPreviewSourceSegmentCount = 0;
	RiverPreviewDrawnSegmentCount = 0;
	RiverPreviewSourceCrossSectionCount = 0;
	RiverPreviewDrawnCrossSectionCount = 0;
	bRiverPreviewTruncated = false;
	for (const FArcWaterRiverGeneratedReach& Reach : RiverPreviewData.Reaches)
	{
		if (Reach.Sections.Num() >= 2)
		{
			RiverPreviewSourceSegmentCount += Reach.bClosed
				? static_cast<int64>(Reach.Sections.Num())
				: static_cast<int64>(Reach.Sections.Num() - 1);
		}
	}

	for (const FArcWaterRiverGeneratedReach& Reach : RiverPreviewData.Reaches)
	{
		const int32 SectionCount = Reach.Sections.Num();
		if (SectionCount < 2)
		{
			continue;
		}
		const int32 SegmentCount = Reach.bClosed ? SectionCount : SectionCount - 1;
		const int32 DrawnReachSegmentCount = SegmentCount;
		for (int32 SegmentIndex = 0; SegmentIndex < DrawnReachSegmentCount; ++SegmentIndex)
		{
			const int32 NextIndex = (SegmentIndex + 1) % SectionCount;
			const FArcWaterRiverChannelSection& StartSection = Reach.Sections[SegmentIndex];
			const FArcWaterRiverChannelSection& EndSection = Reach.Sections[NextIndex];
			FVector2D StartLateral = StartSection.Lateral;
			FVector2D EndLateral = EndSection.Lateral;
			if (!StartLateral.Normalize() || !EndLateral.Normalize())
			{
				continue;
			}
			const FVector StartCenter = RiverPreviewFrame.TransformPosition(StartSection.Position);
			const FVector EndCenter = RiverPreviewFrame.TransformPosition(EndSection.Position);
			const FVector StartLateralWorld = RiverPreviewFrame.TransformVector(
				FVector(StartLateral.X, StartLateral.Y, 0.0)).GetSafeNormal();
			const FVector EndLateralWorld = RiverPreviewFrame.TransformVector(
				FVector(EndLateral.X, EndLateral.Y, 0.0)).GetSafeNormal();
			if (StartLateralWorld.IsNearlyZero() || EndLateralWorld.IsNearlyZero())
			{
				continue;
			}

			const int32 BaseVertex = RiverPreviewChannelVertices.Num();
			RiverPreviewChannelVertices.Add(StartCenter - StartLateralWorld * StartSection.HalfWidthCm);
			RiverPreviewChannelVertices.Add(EndCenter - EndLateralWorld * EndSection.HalfWidthCm);
			RiverPreviewChannelVertices.Add(EndCenter + EndLateralWorld * EndSection.HalfWidthCm);
			RiverPreviewChannelVertices.Add(StartCenter + StartLateralWorld * StartSection.HalfWidthCm);
			RiverPreviewChannelIndices.Add(BaseVertex);
			RiverPreviewChannelIndices.Add(BaseVertex + 1);
			RiverPreviewChannelIndices.Add(BaseVertex + 2);
			RiverPreviewChannelIndices.Add(BaseVertex);
			RiverPreviewChannelIndices.Add(BaseVertex + 2);
			RiverPreviewChannelIndices.Add(BaseVertex + 3);

			AppendPreviewLine(RiverPreviewCenterLines, StartCenter, EndCenter);
			AppendPreviewLine(RiverPreviewBottomLines,
				StartCenter - FVector::UpVector * StartSection.DepthCm,
				EndCenter - FVector::UpVector * EndSection.DepthCm);
			const float StartOuterWidth = StartSection.HalfWidthCm + StartSection.FalloffCm;
			const float EndOuterWidth = EndSection.HalfWidthCm + EndSection.FalloffCm;
			const FVector StartFalloffCenter = RiverPreviewFrame.TransformPosition(
				StartSection.Position + FVector::UpVector * StartSection.FreeboardCm);
			const FVector EndFalloffCenter = RiverPreviewFrame.TransformPosition(
				EndSection.Position + FVector::UpVector * EndSection.FreeboardCm);
			AppendPreviewLine(RiverPreviewFalloffLines,
				StartFalloffCenter - StartLateralWorld * StartOuterWidth,
				EndFalloffCenter - EndLateralWorld * EndOuterWidth);
			AppendPreviewLine(RiverPreviewFalloffLines,
				StartFalloffCenter + StartLateralWorld * StartOuterWidth,
				EndFalloffCenter + EndLateralWorld * EndOuterWidth);
		}
		RiverPreviewDrawnSegmentCount += DrawnReachSegmentCount;
		if (DrawnReachSegmentCount == 0)
		{
			continue;
		}

		const int32 DrawnSectionCount = FMath::Min(SectionCount, DrawnReachSegmentCount + 1);
		const int32 DepthSectionStride = FMath::Max(1, DrawnSectionCount / 24);
		RiverPreviewSourceCrossSectionCount += FMath::DivideAndRoundUp(
			static_cast<int64>(DrawnSectionCount), static_cast<int64>(DepthSectionStride));
		for (int32 SectionIndex = 0; SectionIndex < DrawnSectionCount; SectionIndex += DepthSectionStride)
		{
			const FArcWaterRiverChannelSection& Section = Reach.Sections[SectionIndex];
			FVector2D Lateral = Section.Lateral;
			if (!Lateral.Normalize())
			{
				continue;
			}
			const FVector LateralLocal(Lateral.X, Lateral.Y, 0.0);
			const float HalfFalloff = Section.FalloffCm * 0.5f;
			FVector PreviousLocal = Section.Position - LateralLocal
				* (Section.HalfWidthCm + Section.FalloffCm);
			PreviousLocal.Z += Section.FreeboardCm;
			const FVector LeftCrestLocal = Section.Position - LateralLocal
				* (Section.HalfWidthCm + HalfFalloff) + FVector::UpVector * Section.FreeboardCm;
			AppendPreviewLine(RiverPreviewDepthLines,
				RiverPreviewFrame.TransformPosition(PreviousLocal),
				RiverPreviewFrame.TransformPosition(LeftCrestLocal));
			PreviousLocal = LeftCrestLocal;
			for (int32 BankIndex = 1; BankIndex <= RiverPreviewBankProfileSegments; ++BankIndex)
			{
				const float Alpha = static_cast<float>(BankIndex)
					/ static_cast<float>(RiverPreviewBankProfileSegments);
				const float DistanceToShore = HalfFalloff * (1.0f - Alpha);
				FVector BankLocal = Section.Position - LateralLocal
					* (Section.HalfWidthCm + DistanceToShore);
				BankLocal.Z += Section.FreeboardCm * PreviewSmoothStep(
					DistanceToShore / FMath::Max(HalfFalloff, UE_SMALL_NUMBER));
				AppendPreviewLine(RiverPreviewDepthLines,
					RiverPreviewFrame.TransformPosition(PreviousLocal),
					RiverPreviewFrame.TransformPosition(BankLocal));
				PreviousLocal = BankLocal;
			}
			for (int32 ProfileIndex = 1; ProfileIndex <= RiverPreviewCoreProfileSegments; ++ProfileIndex)
			{
				const float Across = -1.0f + 2.0f * static_cast<float>(ProfileIndex)
					/ static_cast<float>(RiverPreviewCoreProfileSegments);
				FVector ProfileLocal = Section.Position + LateralLocal * (Section.HalfWidthCm * Across);
				ProfileLocal.Z -= Section.DepthCm * (1.0f - PreviewSmoothStep(FMath::Abs(Across)));
				AppendPreviewLine(RiverPreviewDepthLines,
					RiverPreviewFrame.TransformPosition(PreviousLocal),
					RiverPreviewFrame.TransformPosition(ProfileLocal));
				PreviousLocal = ProfileLocal;
			}
			for (int32 BankIndex = 1; BankIndex <= RiverPreviewBankProfileSegments; ++BankIndex)
			{
				const float Alpha = static_cast<float>(BankIndex)
					/ static_cast<float>(RiverPreviewBankProfileSegments);
				const float DistanceToShore = HalfFalloff * Alpha;
				FVector BankLocal = Section.Position + LateralLocal
					* (Section.HalfWidthCm + DistanceToShore);
				BankLocal.Z += Section.FreeboardCm * PreviewSmoothStep(
					DistanceToShore / FMath::Max(HalfFalloff, UE_SMALL_NUMBER));
				AppendPreviewLine(RiverPreviewDepthLines,
					RiverPreviewFrame.TransformPosition(PreviousLocal),
					RiverPreviewFrame.TransformPosition(BankLocal));
				PreviousLocal = BankLocal;
			}
			const FVector RightOuterLocal = Section.Position + LateralLocal
				* (Section.HalfWidthCm + Section.FalloffCm) + FVector::UpVector * Section.FreeboardCm;
			AppendPreviewLine(RiverPreviewDepthLines,
				RiverPreviewFrame.TransformPosition(PreviousLocal),
				RiverPreviewFrame.TransformPosition(RightOuterLocal));
			++RiverPreviewDrawnCrossSectionCount;
		}
	}
	bRiverPreviewTruncated = RiverPreviewDrawnSegmentCount < RiverPreviewSourceSegmentCount
		|| RiverPreviewDrawnCrossSectionCount < RiverPreviewSourceCrossSectionCount;
}

void UArcWaterRiverModifier::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	using namespace ArcWaterRiverModifierLocal_4e7c;

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bRiverPreviewVisible || !RiverPreviewInputsStillMatch())
	{
		if (bRiverPreviewVisible)
		{
			ClearRiverPreview();
		}
		return;
	}

#if UE_DEBUG_RENDERING_ENABLED
	const FSceneInterface* const Scene = UE::DebugDraw::GetSceneKey(GetWorld());
	if (Scene == nullptr)
	{
		return;
	}
	const UE::DebugDraw::EDepthTest Depth = bRiverPreviewThroughTerrain
		? UE::DebugDraw::EDepthTest::AlwaysOnTop
		: UE::DebugDraw::EDepthTest::WorldDepth;
	if (!RiverPreviewChannelIndices.IsEmpty())
	{
		UE::DebugDraw::Mesh(Scene, RiverPreviewChannelVertices, RiverPreviewChannelIndices,
			FColor(20, 135, 255, 72), Depth);
	}
	SubmitPreviewLines(Scene, RiverPreviewFalloffLines, FColor(255, 150, 30, 255), 4.0f, Depth);
	SubmitPreviewLines(Scene, RiverPreviewCenterLines, FColor(230, 245, 255, 255), 3.0f, Depth);
	SubmitPreviewLines(Scene, RiverPreviewBottomLines, FColor(180, 70, 255, 255), 3.0f, Depth);
	SubmitPreviewLines(Scene, RiverPreviewDepthLines, FColor(255, 225, 70, 255), 2.0f, Depth);
#endif
}

void UArcWaterRiverModifier::DrawStampVisualization(
	const FSceneView* View, FPrimitiveDrawInterface* PDI) const
{
	Super::DrawStampVisualization(View, PDI);

	const AArcTerrain* Terrain = AffectedTerrain.Get();
	FBox GenerationBounds;
	if (Terrain == nullptr || !GetGenerationBounds(GenerationBounds))
	{
		return;
	}

	const FMatrix TerrainToWorld = Terrain->GetTerrainTransform().ToMatrixWithScale();
	DrawWireBox(PDI, TerrainToWorld, GenerationBounds,
		FLinearColor(1.0f, 0.0f, 0.0f), SDPG_Foreground, 8.0f);
}

bool UArcWaterRiverModifier::CanEditChange(const FProperty* InProperty) const
{
	if (InProperty != nullptr)
	{
		const FName PropertyName = InProperty->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, CoverageXY) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, BlendMode) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, MinMaxSoftness))
		{
			return false;
		}
	}
	return Super::CanEditChange(InProperty);
}

void UArcWaterRiverModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();
	const TWeakObjectPtr<AActor> EditedOwner = GetOwner();
	const FName EditedComponentName = GetFName();
	UE_LOGF(LogTemp, Display,"[RiverEdit] PostEdit entry t=%.6f path=%ls owner=%ls component=%ls property=%ls member=%ls changeType=0x%x depthCm=%.3f valid=%d",
		FPlatformTime::Seconds(), *GetPathNameSafe(this), *GetPathNameSafe(GetOwner()),
		*EditedComponentName.ToString(), *PropertyName.ToString(), *MemberName.ToString(),
		static_cast<uint32>(PropertyChangedEvent.ChangeType), Settings.DepthCm, IsValid(this));
	const bool bTerrainChanged =
		PropertyName == GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, AffectedTerrain);
	const bool bDisabledChanged =
		PropertyName == GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, bIsDisabled);
	const bool bPriorityChanged =
		PropertyName == GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, Priority);
	const bool bSettingsChanged =
		PropertyName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, Settings) ||
		MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, Settings);
	const bool bGenerationBoundsChanged =
		PropertyName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, GenerationBoundsSize) ||
		MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, GenerationBoundsSize);
	const bool bOutputSettingsChanged =
		PropertyName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, RiverMaterial) ||
		MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, RiverMaterial) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, UnderwaterMaterial) ||
		MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, UnderwaterMaterial) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, RiverParameterCollection) ||
		MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, RiverParameterCollection) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, AbsorptionTint) ||
		MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, AbsorptionTint) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, ClarityDistanceMeters) ||
		MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, ClarityDistanceMeters) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, ScatterAlbedo) ||
		MemberName == GET_MEMBER_NAME_CHECKED(UArcWaterRiverModifier, ScatterAlbedo);

	const bool bClearsGeneratedData = bTerrainChanged || bDisabledChanged || bPriorityChanged
		|| bSettingsChanged || bGenerationBoundsChanged;
	if (bClearsGeneratedData)
	{
		ClearGeneratedRivers();
		UE_LOGF(LogTemp, Display,"[RiverEdit] PostEdit after-clear t=%.6f path=%ls depthCm=%.3f valid=%d",
			FPlatformTime::Seconds(), *GetPathNameSafe(this), Settings.DepthCm, IsValid(this));
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
	AActor* const OwnerAfterSuper = EditedOwner.Get();
	UArcWaterRiverModifier* const CurrentModifier = OwnerAfterSuper != nullptr
		? FindObjectFast<UArcWaterRiverModifier>(OwnerAfterSuper, EditedComponentName)
		: nullptr;
	UE_LOGF(LogTemp, Display,"[RiverEdit] PostEdit after-super t=%.6f oldPath=%ls oldDepthCm=%.3f oldValid=%d currentPath=%ls currentDepthCm=%.3f currentValid=%d same=%d",
		FPlatformTime::Seconds(), *GetPathNameSafe(this), Settings.DepthCm, IsValid(this),
		*GetPathNameSafe(CurrentModifier), CurrentModifier != nullptr ? CurrentModifier->Settings.DepthCm : -1.0f,
		IsValid(CurrentModifier), CurrentModifier == this);
	if (bSettingsChanged)
	{
		RefreshExternalCurveBindings();
	}
	if (!bClearsGeneratedData && bOutputSettingsChanged)
	{
		FText IgnoredError;
		RebuildWaterComponents(IgnoredError);
	}
}
#endif
