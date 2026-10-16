// Copyright Lukasz Baran. All Rights Reserved.

#include "Generation/ArcWaterRiverGeneration.h"

#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileFormat.h"
#include "DataMaps/ArcTerrainDataMapTypes.h"
#include "Engine/World.h"
#include "Extraction/ArcWaterRiverNetworkExtraction.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopedSlowTask.h"
#include "River/ArcWaterRiverChannel.h"
#include "River/ArcWaterRiverModifier.h"

#define LOCTEXT_NAMESPACE "ArcWaterRiverGeneration"

namespace ArcWaterRiverGenerationPrivate_7a3d
{
	constexpr int32 HeightPageIntervalsPerSide = 256;
	constexpr int32 HeightPageSamplesPerSide = HeightPageIntervalsPerSide + 1;
	constexpr int32 RiverReadBlockSamplesPerSide = 1024;

	bool Fail(const FText& Error, FText& OutError)
	{
		OutError = Error;
		return false;
	}

	bool IsFinitePositiveScale(const FVector& Scale)
	{
		return !Scale.ContainsNaN() && Scale.X > 0.0 && Scale.Y > 0.0 && Scale.Z > 0.0;
	}

	bool HasUnsupportedTilt(const FQuat& Rotation)
	{
		const FVector Up = Rotation.RotateVector(FVector::UpVector);
		return Up.ContainsNaN() || !Up.Equals(FVector::UpVector, 1.e-4);
	}

	void SnapshotCurve(FRuntimeFloatCurve& Curve)
	{
		const FRichCurve CurveData = *Curve.GetRichCurveConst();
		Curve.ExternalCurve = nullptr;
		Curve.EditorCurveData = CurveData;
	}

	bool CurvesEqual(const FRichCurve& A, const FRichCurve& B)
	{
		return A.GetDefaultValue() == B.GetDefaultValue()
			&& A.PreInfinityExtrap == B.PreInfinityExtrap
			&& A.PostInfinityExtrap == B.PostInfinityExtrap
			&& A.GetConstRefOfKeys() == B.GetConstRefOfKeys();
	}

	bool SettingsEqual(const FArcWaterRiverModifierSettings& A,
		const FArcWaterRiverModifierSettings& B)
	{
		return A.RiversThreshold == B.RiversThreshold
			&& A.HalfWidthCm == B.HalfWidthCm
			&& CurvesEqual(*A.WidthScaleBySlope.GetRichCurveConst(), *B.WidthScaleBySlope.GetRichCurveConst())
			&& CurvesEqual(*A.WidthScaleByElevation.GetRichCurveConst(), *B.WidthScaleByElevation.GetRichCurveConst())
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

	bool ValidateSettings(const FArcWaterRiverModifierSettings& Settings, FText& OutError)
	{
		if (!FMath::IsFinite(Settings.RiversThreshold)
			|| Settings.RiversThreshold < 0.0f || Settings.RiversThreshold > 1.0f)
		{
			return Fail(LOCTEXT("InvalidThreshold", "Rivers Threshold must be finite and in [0, 1]."), OutError);
		}
		if (!FMath::IsFinite(Settings.HalfWidthCm) || Settings.HalfWidthCm <= 0.0f)
		{
			return Fail(LOCTEXT("InvalidHalfWidth", "Half Width must be finite and positive."), OutError);
		}
		if (!FMath::IsFinite(Settings.DepthCm) || Settings.DepthCm < 0.0f)
		{
			return Fail(LOCTEXT("InvalidDepth", "Depth must be finite and non-negative."), OutError);
		}
		if (!FMath::IsFinite(Settings.FalloffCm) || Settings.FalloffCm <= 0.0f)
		{
			return Fail(LOCTEXT("InvalidFalloff", "Falloff must be finite and positive."), OutError);
		}
		if (!FMath::IsFinite(Settings.BankFreeboardCm) || Settings.BankFreeboardCm < 0.0f)
		{
			return Fail(LOCTEXT("InvalidFreeboard", "Bank Freeboard must be finite and non-negative."), OutError);
		}
		if (!FMath::IsFinite(Settings.SlopeSampleDistanceCm) || Settings.SlopeSampleDistanceCm <= 0.0f)
		{
			return Fail(LOCTEXT("InvalidSlopeDistance", "Slope Sample Distance must be finite and positive."), OutError);
		}
		if (!FMath::IsFinite(Settings.SplineToleranceCm) || Settings.SplineToleranceCm < 0.0f)
		{
			return Fail(LOCTEXT("InvalidSplineTolerance", "Spline Tolerance must be finite and non-negative."), OutError);
		}
		if (!FMath::IsFinite(Settings.MaxStepCm) || Settings.MaxStepCm <= 0.0f)
		{
			return Fail(LOCTEXT("InvalidMaxStep", "Max Step must be finite and positive."), OutError);
		}
		if (!FMath::IsFinite(Settings.FlowSpeedCmS))
		{
			return Fail(LOCTEXT("InvalidFlowSpeed", "Flow Speed must be finite."), OutError);
		}
		if (!FMath::IsFinite(Settings.TileCm) || Settings.TileCm <= 0.0f)
		{
			return Fail(LOCTEXT("InvalidTileSize", "Tile size must be finite and positive."), OutError);
		}
		if (Settings.WidthSegments < 1)
		{
			return Fail(LOCTEXT("InvalidWidthSegments", "Width Segments must be positive."), OutError);
		}
		return true;
	}

	bool SampleRivers(UArcTerrainSubsystem& Subsystem, const AArcTerrain& Terrain,
		FIntPoint MinTexel, FIntPoint MaxTexel, float Threshold,
		ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid& OutMask,
		FText& OutError, TFunctionRef<bool()> ShouldContinue)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN SampleRivers t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END SampleRivers elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		const int64 SpanX = static_cast<int64>(MaxTexel.X) - MinTexel.X + 1;
		const int64 SpanY = static_cast<int64>(MaxTexel.Y) - MinTexel.Y + 1;
		if (SpanX < 2 || SpanY < 2 || SpanX > MAX_int32 || SpanY > MAX_int32
			|| MaxTexel.X == MAX_int32 || MaxTexel.Y == MAX_int32)
		{
			return Fail(LOCTEXT("RiversGridTooLarge", "The native Rivers source rectangle exceeds the supported sparse-grid coordinate range."), OutError);
		}
		const FIntPoint ReadDims(static_cast<int32>(SpanX), static_cast<int32>(SpanY));
		ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid Mask;
		Mask.SourceRect = FIntRect(MinTexel, MaxTexel + FIntPoint(1, 1));
		int64 BlocksRead = 0;
		int64 NonemptyBlocks = 0;
		for (int32 OffsetY = 0; OffsetY < ReadDims.Y;)
		{
			const int32 BlockHeight = FMath::Min(RiverReadBlockSamplesPerSide, ReadDims.Y - OffsetY);
			for (int32 OffsetX = 0; OffsetX < ReadDims.X;)
			{
				if (!ShouldContinue())
				{
					return Fail(LOCTEXT("CancelledRiversRead", "River generation was cancelled."), OutError);
				}
				const int32 BlockWidth = FMath::Min(RiverReadBlockSamplesPerSide, ReadDims.X - OffsetX);
				const FIntPoint BlockMin = MinTexel + FIntPoint(OffsetX, OffsetY);
				const FIntPoint BlockMax = BlockMin + FIntPoint(BlockWidth - 1, BlockHeight - 1);
				UE_LOGF(LogTemp, Display,"[RiverGen] RIVERS BLOCK begin min=%ls max=%ls t=%.6f", *BlockMin.ToString(), *BlockMax.ToString(), FPlatformTime::Seconds());
				const double RiverBlockStart = FPlatformTime::Seconds();
				ON_SCOPE_EXIT
				{
					UE_LOGF(LogTemp, Display,"[RiverGen] RIVERS BLOCK end min=%ls max=%ls elapsed=%.3fs", *BlockMin.ToString(), *BlockMax.ToString(), FPlatformTime::Seconds() - RiverBlockStart);
				};
				TArray<float> BlockValues;
				FIntPoint BlockDims = FIntPoint::ZeroValue;
				bool bComplete = false;
				if (!Subsystem.SampleDataMapTexelsFromSources(Terrain, EArcTerrainDataMap::Rivers,
					BlockMin, BlockMax, BlockValues, BlockDims, bComplete))
				{
					return Fail(LOCTEXT("RiversReadFailed", "Failed to read the complete native Rivers source rectangle."), OutError);
				}
				if (!bComplete)
				{
					return Fail(LOCTEXT("RiversReadIncomplete", "The Rivers source rectangle is incomplete; resolve all contributing sources and rebuild."), OutError);
				}
				if (BlockDims != FIntPoint(BlockWidth, BlockHeight)
					|| BlockValues.Num() != static_cast<int64>(BlockWidth) * BlockHeight)
				{
					return Fail(LOCTEXT("RiversBlockShape", "The Rivers source reader returned an unexpected block shape."), OutError);
				}
				++BlocksRead;
				bool bBlockNonempty = false;
				for (int32 Y = 0; Y < BlockHeight; ++Y)
				{
					if (!ShouldContinue())
					{
						return Fail(LOCTEXT("CancelledRiversCopy", "River generation was cancelled."), OutError);
					}
					for (int32 X = 0; X < BlockWidth; ++X)
					{
						const float Value = BlockValues[Y * BlockWidth + X];
						if (!FMath::IsFinite(Value))
						{
							const FIntPoint InvalidTexel = BlockMin + FIntPoint(X, Y);
							return Fail(FText::Format(
								LOCTEXT("InvalidMaskValue", "The Rivers mask contains a non-finite value at texel ({0}, {1})."),
								FText::AsNumber(InvalidTexel.X), FText::AsNumber(InvalidTexel.Y)), OutError);
						}
						if (Value > 0.0f && Value >= Threshold)
						{
							if (!Mask.AddActivePixel(BlockMin + FIntPoint(X, Y)))
							{
								return Fail(LOCTEXT("RiversActiveCountOverflow", "The active Rivers pixel count exceeds the supported range."), OutError);
							}
							bBlockNonempty = true;
						}
					}
				}
				NonemptyBlocks += bBlockNonempty ? 1 : 0;
				OffsetX += BlockWidth;
			}
			OffsetY += BlockHeight;
		}
		UE_LOGF(LogTemp, Display,"[RiverGen] RIVERS READ blocks=%lld nonemptyBlocks=%lld tiles=%d active=%lld bytes=%lld",
			BlocksRead, NonemptyBlocks, Mask.Tiles.Num(), Mask.ActivePixelCount, Mask.GetAllocatedBytes());
		OutMask = MoveTemp(Mask);
		return true;
	}

	bool ValidateTransforms(const UArcWaterRiverModifier& Modifier, const AArcTerrain& Terrain,
		FTransform& OutTerrainTransform, FTransform& OutGeneratedFrame, FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN ValidateTransforms t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END ValidateTransforms elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		const FTransform ModifierTransform = Modifier.GetComponentTransform();
		const FTransform TerrainTransform = Terrain.GetTerrainTransform();
		if (ModifierTransform.ContainsNaN() || TerrainTransform.ContainsNaN()
			|| !IsFinitePositiveScale(ModifierTransform.GetScale3D())
			|| !IsFinitePositiveScale(TerrainTransform.GetScale3D()))
		{
			return Fail(LOCTEXT("InvalidTransform", "The river modifier and terrain require finite, positive transforms."), OutError);
		}
		if (HasUnsupportedTilt(ModifierTransform.GetRotation()) || HasUnsupportedTilt(TerrainTransform.GetRotation()))
		{
			return Fail(LOCTEXT("UnsupportedTilt", "River generation supports yaw rotation only; remove pitch and roll from the modifier and terrain."), OutError);
		}

		const double TerrainYaw = TerrainTransform.Rotator().Yaw;
		OutTerrainTransform = TerrainTransform;
		OutGeneratedFrame = FTransform(
			FRotator(0.0, TerrainYaw, 0.0), TerrainTransform.GetTranslation(), FVector::OneVector);
		return true;
	}

	bool BoundsToTexelsInward(const FBox& Bounds, double TexelCm, FIntPoint& OutMin,
		FIntPoint& OutMax, FText& OutError)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN BoundsToTexelsInward t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END BoundsToTexelsInward elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		if (!Bounds.IsValid || Bounds.Min.ContainsNaN() || Bounds.Max.ContainsNaN()
			|| !FMath::IsFinite(TexelCm) || TexelCm <= 0.0)
		{
			return Fail(LOCTEXT("InvalidSourceBounds", "The Rivers source region is invalid."), OutError);
		}
		const double MinTexelX = Bounds.Min.X / TexelCm;
		const double MinTexelY = Bounds.Min.Y / TexelCm;
		const double MaxTexelX = Bounds.Max.X / TexelCm;
		const double MaxTexelY = Bounds.Max.Y / TexelCm;
		if (!FMath::IsFinite(MinTexelX) || !FMath::IsFinite(MinTexelY)
			|| !FMath::IsFinite(MaxTexelX) || !FMath::IsFinite(MaxTexelY)
			|| MinTexelX < static_cast<double>(MIN_int32) || MinTexelY < static_cast<double>(MIN_int32)
			|| MaxTexelX > static_cast<double>(MAX_int32) || MaxTexelY > static_cast<double>(MAX_int32)
			|| MaxTexelX < MinTexelX || MaxTexelY < MinTexelY)
		{
			return Fail(LOCTEXT("SourceBoundsOutOfRange", "The Rivers source region exceeds the supported texel range."), OutError);
		}
		const int64 MinX = FMath::CeilToInt64(MinTexelX);
		const int64 MinY = FMath::CeilToInt64(MinTexelY);
		const int64 MaxX = FMath::FloorToInt64(MaxTexelX);
		const int64 MaxY = FMath::FloorToInt64(MaxTexelY);
		if (MaxX - MinX + 1 < 2 || MaxY - MinY + 1 < 2)
		{
			return Fail(LOCTEXT("GenerationBoundsTooSmall", "Generation Bounds must contain at least two native Rivers texels on each axis after inward alignment."), OutError);
		}
		OutMin = FIntPoint(static_cast<int32>(MinX), static_cast<int32>(MinY));
		OutMax = FIntPoint(static_cast<int32>(MaxX), static_cast<int32>(MaxY));
		return true;
	}

	FVector FramePosition(const FTransform& TerrainTransform, const FTransform& FrameTransform,
		const FVector& TerrainLocalPosition)
	{
		return FrameTransform.InverseTransformPosition(
			TerrainTransform.TransformPosition(TerrainLocalPosition));
	}

	bool BuildNetworkInput(const TArray<ArcWaterRiverNetworkExtraction::FArcWaterRiverPath>& Paths,
		const TArray<FIntPoint>& JunctionTexels, double DataTexelCm,
		const FTransform& TerrainTransform, const FTransform& GeneratedFrame,
		FArcWaterRiverNetworkInput& OutInput, FText& OutError, TFunctionRef<bool()> ShouldContinue)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN BuildNetworkInput t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END BuildNetworkInput elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		const FVector SourceOrigin = FramePosition(TerrainTransform, GeneratedFrame,
			FVector::ZeroVector);
		const FVector DataStepX3 = FramePosition(TerrainTransform, GeneratedFrame,
			FVector(DataTexelCm, 0.0, 0.0)) - SourceOrigin;
		const FVector DataStepY3 = FramePosition(TerrainTransform, GeneratedFrame,
			FVector(0.0, DataTexelCm, 0.0)) - SourceOrigin;
		const FVector2D DataStepX(DataStepX3.X, DataStepX3.Y);
		const FVector2D DataStepY(DataStepY3.X, DataStepY3.Y);
		if (FMath::Abs(DataStepX.Y) > 1.e-3 || FMath::Abs(DataStepY.X) > 1.e-3
			|| DataStepX.X <= 0.0 || DataStepY.Y <= 0.0)
		{
			return Fail(LOCTEXT("UnsupportedDataAxes", "The terrain transform cannot produce an axis-aligned river data lattice."), OutError);
		}

		OutInput.Reaches.Reserve(Paths.Num());
		for (const ArcWaterRiverNetworkExtraction::FArcWaterRiverPath& Path : Paths)
		{
			if (!ShouldContinue())
			{
				return Fail(LOCTEXT("CancelledNetwork", "River generation was cancelled."), OutError);
			}
			FArcWaterRiverNetworkReachInput& Reach = OutInput.Reaches.AddDefaulted_GetRef();
			Reach.bClosed = Path.bClosed;
			Reach.bStartIsOriginalEndpoint = Path.bStartIsOriginalEndpoint;
			Reach.bEndIsOriginalEndpoint = Path.bEndIsOriginalEndpoint;
			Reach.Points.Reserve(Path.Points.Num());
			for (const FVector2D& Point : Path.Points)
			{
				if (!ShouldContinue())
				{
					return Fail(LOCTEXT("CancelledNetworkPoints", "River generation was cancelled."), OutError);
				}
				Reach.Points.Add(FVector2D(SourceOrigin.X + Point.X, SourceOrigin.Y + Point.Y));
			}
			Reach.Junctions.Reserve(Path.Junctions.Num());
			for (const ArcWaterRiverNetworkExtraction::FArcWaterRiverJunctionRef& Junction : Path.Junctions)
			{
				if (!ShouldContinue())
				{
					return Fail(LOCTEXT("CancelledNetworkJunctions", "River generation was cancelled."), OutError);
				}
				FArcWaterRiverNetworkJunctionRef& Target = Reach.Junctions.AddDefaulted_GetRef();
				Target.JunctionId = Junction.JunctionId;
				Target.PointIndex = Junction.PointIndex;
			}
		}

		OutInput.JunctionPositions.Reserve(JunctionTexels.Num());
		for (const FIntPoint JunctionTexel : JunctionTexels)
		{
			if (!ShouldContinue())
			{
				return Fail(LOCTEXT("CancelledJunctionPositions", "River generation was cancelled."), OutError);
			}
			OutInput.JunctionPositions.Add(FVector2D(
				SourceOrigin.X + JunctionTexel.X * DataStepX.X,
				SourceOrigin.Y + JunctionTexel.Y * DataStepY.Y));
		}
		return true;
	}

	int64 FloorDivide(int64 Value, int64 Divisor)
	{
		int64 Quotient = Value / Divisor;
		if (Value % Divisor < 0)
		{
			--Quotient;
		}
		return Quotient;
	}

	class FArcWaterRiverHeightPageSampler
	{
	public:
		FArcWaterRiverHeightPageSampler(UArcTerrainSubsystem& InSubsystem, const AArcTerrain& InTerrain,
			const UArcWaterRiverModifier& InModifier, const FTransform& InTerrainTransform,
			const FTransform& InGeneratedFrame, TFunctionRef<bool()> InShouldContinue)
			: Subsystem(InSubsystem)
			, Terrain(InTerrain)
			, Modifier(InModifier)
			, TerrainTransform(InTerrainTransform)
			, GeneratedFrame(InGeneratedFrame)
			, ShouldContinue(InShouldContinue)
			, HeightTexelCm(static_cast<double>(InTerrain.Config.TexelSizeCm()))
		{
		}

		bool IsValid(FText& OutError) const
		{
			if (!FMath::IsFinite(HeightTexelCm) || HeightTexelCm <= 0.0)
			{
				return Fail(LOCTEXT("InvalidHeightTexel", "The terrain height texel size is invalid."), OutError);
			}
			return true;
		}

		bool Sample(const FVector2D& GeneratedPosition, float& OutHeight)
		{
			const double SampleStart = FPlatformTime::Seconds();
			ON_SCOPE_EXIT
			{
				SampleSeconds += FPlatformTime::Seconds() - SampleStart;
			};
			++SampleRequests;
			if (!FMath::IsFinite(GeneratedPosition.X) || !FMath::IsFinite(GeneratedPosition.Y))
			{
				return SetFailure(LOCTEXT("HeightPositionNonFinite", "A terrain height request contains a non-finite generated position."));
			}

			const FVector TerrainLocalPosition = TerrainTransform.InverseTransformPosition(
				GeneratedFrame.TransformPosition(FVector(GeneratedPosition.X, GeneratedPosition.Y, 0.0)));
			const double TexelX = TerrainLocalPosition.X / HeightTexelCm;
			const double TexelY = TerrainLocalPosition.Y / HeightTexelCm;
			if (!FMath::IsFinite(TexelX) || !FMath::IsFinite(TexelY)
				|| TexelX < static_cast<double>(MIN_int32) || TexelY < static_cast<double>(MIN_int32)
				|| TexelX > static_cast<double>(MAX_int32) || TexelY > static_cast<double>(MAX_int32))
			{
				return SetFailure(LOCTEXT("HeightPositionOutOfRange", "A terrain height request exceeds the supported texel coordinate range."));
			}

			const int64 TexelX0 = FMath::FloorToInt64(TexelX);
			const int64 TexelY0 = FMath::FloorToInt64(TexelY);
			const int64 PageX = FloorDivide(TexelX0, HeightPageIntervalsPerSide);
			const int64 PageY = FloorDivide(TexelY0, HeightPageIntervalsPerSide);
			const int64 PageOriginX = PageX * HeightPageIntervalsPerSide;
			const int64 PageOriginY = PageY * HeightPageIntervalsPerSide;
			if (PageOriginX < MIN_int32 || PageOriginY < MIN_int32
				|| PageOriginX > static_cast<int64>(MAX_int32) - HeightPageIntervalsPerSide
				|| PageOriginY > static_cast<int64>(MAX_int32) - HeightPageIntervalsPerSide)
			{
				return SetFailure(LOCTEXT("HeightPageOutOfRange", "A terrain height page exceeds the supported texel coordinate range."));
			}

			const FIntPoint PageCoordinate(static_cast<int32>(PageX), static_cast<int32>(PageY));
			++PageRequests;
			FHeightPage* Page = Pages.Find(PageCoordinate);
			if (Page != nullptr)
			{
				++ReusedPageRequests;
			}
			else
			{
				FHeightPage NewPage;
				if (!LoadPage(FIntPoint(static_cast<int32>(PageOriginX), static_cast<int32>(PageOriginY)), NewPage))
				{
					return false;
				}
				Page = &Pages.Add(PageCoordinate, MoveTemp(NewPage));
				++LoadedPages;
				ResidentHeightBytes += Page->Heights.GetAllocatedSize();
				PeakPageBytes = ResidentHeightBytes + Pages.GetAllocatedSize();
			}

			const int32 LocalX = static_cast<int32>(TexelX0 - PageOriginX);
			const int32 LocalY = static_cast<int32>(TexelY0 - PageOriginY);
			const float AlphaX = static_cast<float>(TexelX - static_cast<double>(TexelX0));
			const float AlphaY = static_cast<float>(TexelY - static_cast<double>(TexelY0));
			const float H00 = Page->Heights[LocalY * HeightPageSamplesPerSide + LocalX];
			const float H10 = Page->Heights[LocalY * HeightPageSamplesPerSide + LocalX + 1];
			const float H01 = Page->Heights[(LocalY + 1) * HeightPageSamplesPerSide + LocalX];
			const float H11 = Page->Heights[(LocalY + 1) * HeightPageSamplesPerSide + LocalX + 1];
			OutHeight = FMath::Lerp(FMath::Lerp(H00, H10, AlphaX), FMath::Lerp(H01, H11, AlphaX), AlphaY);
			if (!FMath::IsFinite(OutHeight))
			{
				return SetFailure(LOCTEXT("HeightInterpolationNonFinite", "Terrain height interpolation produced a non-finite value."));
			}
			return true;
		}

		const FText& GetError() const
		{
			return LastError;
		}

		void LogStats() const
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] HEIGHT PAGES sampleRequests=%llu pageRequests=%llu loadAttempts=%llu loaded=%llu reused=%llu residentBytes=%llu peakBytes=%llu sampleTime=%.3fs loadTime=%.3fs",
				SampleRequests, PageRequests, PageLoadAttempts, LoadedPages, ReusedPageRequests,
				GetAllocatedBytes(), PeakPageBytes, SampleSeconds, LoadSeconds);
		}

		void ReleasePages()
		{
			Pages.Empty();
			ResidentHeightBytes = 0;
		}

	private:
		struct FHeightPage
		{
			TArray<float> Heights;
		};

		bool SetFailure(const FText& Error)
		{
			if (LastError.IsEmpty())
			{
				LastError = Error;
			}
			return false;
		}

		bool LoadPage(const FIntPoint& PageOrigin, FHeightPage& OutPage)
		{
			const FIntPoint PageMax = PageOrigin + FIntPoint(HeightPageIntervalsPerSide, HeightPageIntervalsPerSide);
			FIntPoint Dims = FIntPoint::ZeroValue;
			bool bComplete = false;
			const double LoadStart = FPlatformTime::Seconds();
			UE_LOGF(LogTemp, Display,"[RiverGen] HEIGHT PAGE BEGIN min=%ls max=%ls t=%.6f",
				*PageOrigin.ToString(), *PageMax.ToString(), LoadStart);
			ON_SCOPE_EXIT
			{
				LoadSeconds += FPlatformTime::Seconds() - LoadStart;
				UE_LOGF(LogTemp, Display,"[RiverGen] HEIGHT PAGE END min=%ls max=%ls dims=%ls complete=%d elapsed=%.3fs error=%ls",
					*PageOrigin.ToString(), *PageMax.ToString(), *Dims.ToString(), bComplete ? 1 : 0,
					FPlatformTime::Seconds() - LoadStart, *LastError.ToString());
			};
			++PageLoadAttempts;
			if (!ShouldContinue())
			{
				return SetFailure(LOCTEXT("CancelledHeightPage", "River generation was cancelled while loading terrain height pages."));
			}

			TArray<float> TerrainHeights;
			if (!Subsystem.SampleTerrainHeightTexelsBeforeStamp(
				Terrain, Modifier, PageOrigin, PageMax, TerrainHeights, Dims, bComplete))
			{
				return SetFailure(LOCTEXT("HeightPageReadFailed", "Failed to read a terrain height page before the river stamp."));
			}
			if (!bComplete)
			{
				return SetFailure(LOCTEXT("HeightPageReadIncomplete", "A terrain height page before the river stamp is incomplete; resolve all contributing sources and rebuild."));
			}
			const int32 ExpectedSampleCount = HeightPageSamplesPerSide * HeightPageSamplesPerSide;
			if (Dims != FIntPoint(HeightPageSamplesPerSide, HeightPageSamplesPerSide)
				|| TerrainHeights.Num() != ExpectedSampleCount)
			{
				return SetFailure(LOCTEXT("HeightPageReadShape", "The terrain height reader returned an unexpected page shape."));
			}

			OutPage.Heights.SetNumUninitialized(ExpectedSampleCount);
			for (int32 Y = 0; Y < HeightPageSamplesPerSide; ++Y)
			{
				if (!ShouldContinue())
				{
					return SetFailure(LOCTEXT("CancelledHeightPageConversion", "River generation was cancelled while converting a terrain height page."));
				}
				for (int32 X = 0; X < HeightPageSamplesPerSide; ++X)
				{
					const float TerrainLocalHeight = TerrainHeights[Y * HeightPageSamplesPerSide + X];
					if (!FMath::IsFinite(TerrainLocalHeight))
					{
						return SetFailure(LOCTEXT("HeightPageNonFinite", "A terrain height page contains a non-finite sample."));
					}
					const FVector TerrainLocalSample(
						static_cast<double>(PageOrigin.X + X) * HeightTexelCm,
						static_cast<double>(PageOrigin.Y + Y) * HeightTexelCm,
						TerrainLocalHeight);
					const float GeneratedHeight = static_cast<float>(
						FramePosition(TerrainTransform, GeneratedFrame, TerrainLocalSample).Z);
					if (!FMath::IsFinite(GeneratedHeight))
					{
						return SetFailure(LOCTEXT("HeightPageTransformNonFinite", "A terrain height page produced a non-finite generated height."));
					}
					OutPage.Heights[Y * HeightPageSamplesPerSide + X] = GeneratedHeight;
				}
			}
			return true;
		}

		uint64 GetAllocatedBytes() const
		{
			return ResidentHeightBytes + Pages.GetAllocatedSize();
		}

		UArcTerrainSubsystem& Subsystem;
		const AArcTerrain& Terrain;
		const UArcWaterRiverModifier& Modifier;
		FTransform TerrainTransform;
		FTransform GeneratedFrame;
		TFunctionRef<bool()> ShouldContinue;
		double HeightTexelCm = 0.0;
		TMap<FIntPoint, FHeightPage> Pages;
		FText LastError;
		uint64 SampleRequests = 0;
		uint64 PageRequests = 0;
		uint64 PageLoadAttempts = 0;
		uint64 LoadedPages = 0;
		uint64 ReusedPageRequests = 0;
		uint64 ResidentHeightBytes = 0;
		uint64 PeakPageBytes = 0;
		double SampleSeconds = 0.0;
		double LoadSeconds = 0.0;
	};
}

bool ArcWaterRiverGeneration::Compute(UArcWaterRiverModifier& Modifier,
	FArcWaterRiverGeneratedData& OutGeneratedData, FTransform& OutGeneratedFrame, FText& OutError)
{
	const double RiverLogStart = FPlatformTime::Seconds();
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN ArcWaterRiverGeneration::Compute t=%.6f", RiverLogStart);
	ON_SCOPE_EXIT
	{
		UE_LOGF(LogTemp, Display,"[RiverGen] END ArcWaterRiverGeneration::Compute elapsed=%.3fs, error=%ls",
			FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
	};
	using namespace ArcWaterRiverGenerationPrivate_7a3d;

	OutError = FText::GetEmpty();
	OutGeneratedData = FArcWaterRiverGeneratedData();
	OutGeneratedFrame = FTransform::Identity;
	if (!IsInGameThread())
	{
		return Fail(LOCTEXT("GameThreadOnly", "River generation must run on the game thread."), OutError);
	}
	AActor* const Owner = Modifier.GetOwner();
	if (Modifier.IsTemplate() || Owner == nullptr || Owner->IsTemplate() || Modifier.GetWorld() == nullptr)
	{
		return Fail(LOCTEXT("InstanceOnly", "River computation is available only on a level instance component."), OutError);
	}
	AArcTerrain* const Terrain = Modifier.AffectedTerrain.Get();
	if (Terrain == nullptr)
	{
		return Fail(LOCTEXT("MissingTerrain", "Assign a loaded affected terrain before rebuilding rivers."), OutError);
	}
	UArcTerrainSubsystem* const Subsystem = Modifier.GetWorld()->GetSubsystem<UArcTerrainSubsystem>();
	if (Subsystem == nullptr)
	{
		return Fail(LOCTEXT("MissingSubsystem", "The affected world's ArcTerrain subsystem is unavailable."), OutError);
	}

	FArcWaterRiverModifierSettings Settings = Modifier.Settings;
	const FVector2D GenerationBoundsSize = Modifier.GenerationBoundsSize;
	const FVector OwnerWorldLocation = Owner->GetActorLocation();
	UE_LOGF(LogTemp, Display,"[RiverGen] INPUT modifier=%ls terrain=%ls actorWorldLocation=%ls boundsSize=%ls halfWidth=%.3f depth=%.3f falloff=%.3f slopeWindow=%.3f maxStep=%.3f threshold=%.6f",
		*Modifier.GetPathName(), *Terrain->GetPathName(), *OwnerWorldLocation.ToString(),
		*GenerationBoundsSize.ToString(), Settings.HalfWidthCm, Settings.DepthCm,
		Settings.FalloffCm, Settings.SlopeSampleDistanceCm, Settings.MaxStepCm, Settings.RiversThreshold);
	UCurveFloat* const SlopeCurveSource = Settings.WidthScaleBySlope.ExternalCurve;
	UCurveFloat* const ElevationCurveSource = Settings.WidthScaleByElevation.ExternalCurve;
	SnapshotCurve(Settings.WidthScaleBySlope);
	SnapshotCurve(Settings.WidthScaleByElevation);
	if (!ValidateSettings(Settings, OutError))
	{
		return false;
	}
	FBox GenerationBounds;
	if (!Modifier.GetGenerationBounds(GenerationBounds))
	{
		return Fail(LOCTEXT("InvalidGenerationBounds", "The actor position and full Generation Bounds Size must define a finite, positive terrain-local XY rectangle."), OutError);
	}
	FTransform TerrainTransform;
	FTransform GeneratedFrame;
	if (!ValidateTransforms(Modifier, *Terrain, TerrainTransform, GeneratedFrame, OutError))
	{
		return false;
	}
	const FTransform InitialModifierTransform = Modifier.GetComponentTransform();
	const TWeakObjectPtr<AActor> OwnerGuard(Owner);
	const TWeakObjectPtr<AArcTerrain> TerrainGuard(Terrain);
	const TWeakObjectPtr<UCurveFloat> SlopeCurveGuard(SlopeCurveSource);
	const TWeakObjectPtr<UCurveFloat> ElevationCurveGuard(ElevationCurveSource);
	const bool bHadSlopeCurve = SlopeCurveSource != nullptr;
	const bool bHadElevationCurve = ElevationCurveSource != nullptr;
	const TFunction<bool()> InputsStillMatch = [&Modifier, OwnerGuard, TerrainGuard, TerrainTransform,
		InitialModifierTransform, OwnerWorldLocation, Settings, GenerationBoundsSize,
		SlopeCurveGuard, ElevationCurveGuard,
		bHadSlopeCurve, bHadElevationCurve]() -> bool
	{
		AActor* const CurrentOwner = OwnerGuard.Get();
		AArcTerrain* const CurrentTerrain = TerrainGuard.Get();
		const UCurveFloat* const CurrentSlopeCurve = Modifier.Settings.WidthScaleBySlope.ExternalCurve;
		const UCurveFloat* const CurrentElevationCurve = Modifier.Settings.WidthScaleByElevation.ExternalCurve;
		return CurrentOwner != nullptr
			&& Modifier.GetOwner() == CurrentOwner
			&& CurrentOwner->GetActorLocation().Equals(OwnerWorldLocation, 1.e-6)
			&& CurrentTerrain != nullptr
			&& Modifier.AffectedTerrain.Get() == CurrentTerrain
			&& CurrentTerrain->GetTerrainTransform().Equals(TerrainTransform, 1.e-6)
			&& Modifier.GetComponentTransform().Equals(InitialModifierTransform, 1.e-6)
			&& (CurrentSlopeCurve != nullptr) == bHadSlopeCurve
			&& (CurrentElevationCurve != nullptr) == bHadElevationCurve
			&& (!bHadSlopeCurve || CurrentSlopeCurve == SlopeCurveGuard.Get())
			&& (!bHadElevationCurve || CurrentElevationCurve == ElevationCurveGuard.Get())
			&& Modifier.GenerationBoundsSize == GenerationBoundsSize
			&& SettingsEqual(Modifier.Settings, Settings);
	};

	FScopedSlowTask SlowTask(90.0f, LOCTEXT("ComputeProgress", "Computing river channels"));
	SlowTask.MakeDialog(true);
	double RiverLastHeartbeat = FPlatformTime::Seconds();
	const TFunction<bool()> ShouldContinue = [&SlowTask, &InputsStillMatch, &RiverLastHeartbeat, RiverLogStart]() -> bool
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - RiverLastHeartbeat >= 2.0)
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] HEARTBEAT phase=%ls progress=%.2f/%.2f elapsed=%.3fs",
				*SlowTask.GetCurrentMessage().ToString(), SlowTask.CompletedWork,
				SlowTask.TotalAmountOfWork, Now - RiverLogStart);
			RiverLastHeartbeat = Now;
		}
		return !SlowTask.ShouldCancel() && InputsStillMatch();
	};
	SlowTask.EnterProgressFrame(5.0f, LOCTEXT("ValidateProgress", "Validating river inputs"));
	UE_LOGF(LogTemp, Display,"[RiverGen] PHASE Validating river inputs t=%.6f", FPlatformTime::Seconds());
	if (!ShouldContinue())
	{
		return Fail(InputsStillMatch()
			? LOCTEXT("CancelledEarly", "River generation was cancelled.")
			: LOCTEXT("InputsChangedEarly", "River generation inputs changed while rebuilding; run Rebuild Rivers again."), OutError);
	}

	SlowTask.EnterProgressFrame(15.0f, LOCTEXT("ReadRiversProgress", "Reading native Rivers sources"));
	UE_LOGF(LogTemp, Display,"[RiverGen] PHASE Reading native Rivers sources t=%.6f", FPlatformTime::Seconds());
	FBox AvailableSourceBounds(ForceInit);
	FArcTerrainTileGrid DataGrid;
	if (!Subsystem->GetDataMapSourceRegion(*Terrain, EArcTerrainDataMap::Rivers, AvailableSourceBounds, DataGrid))
	{
		return Fail(LOCTEXT("MissingRiversSource", "No valid Rivers source exists on the affected terrain."), OutError);
	}
	if (!DataGrid.IsValid())
	{
		return Fail(LOCTEXT("InvalidRiversGrid", "The native Rivers source grid is invalid."), OutError);
	}
	const double DataTexelCm = static_cast<double>(DataGrid.TileExtentM) * 100.0
		/ static_cast<double>(DataGrid.TileQuads);
	if (!AvailableSourceBounds.IsValid || AvailableSourceBounds.Min.ContainsNaN()
		|| AvailableSourceBounds.Max.ContainsNaN())
	{
		return Fail(LOCTEXT("InvalidRiversSourceBounds", "The available Rivers source region is invalid."), OutError);
	}
	const FVector2D SelectedMin(
		FMath::Max(GenerationBounds.Min.X, AvailableSourceBounds.Min.X),
		FMath::Max(GenerationBounds.Min.Y, AvailableSourceBounds.Min.Y));
	const FVector2D SelectedMax(
		FMath::Min(GenerationBounds.Max.X, AvailableSourceBounds.Max.X),
		FMath::Min(GenerationBounds.Max.Y, AvailableSourceBounds.Max.Y));
	if (SelectedMax.X <= SelectedMin.X || SelectedMax.Y <= SelectedMin.Y)
	{
		return Fail(LOCTEXT("GenerationBoundsOutsideSources", "Generation Bounds do not overlap the available Rivers source region."), OutError);
	}
	const FBox SelectedSourceBounds(
		FVector(SelectedMin.X, SelectedMin.Y, 0.0),
		FVector(SelectedMax.X, SelectedMax.Y, 0.0));
	FIntPoint SourceMinTexel;
	FIntPoint SourceMaxTexel;
	if (!BoundsToTexelsInward(SelectedSourceBounds, DataTexelCm, SourceMinTexel, SourceMaxTexel, OutError))
	{
		return false;
	}
	UE_LOGF(LogTemp, Display,"[RiverGen] SOURCE selectedTexels=%ls..%ls texelCm=%.6f terrainTransform=%ls",
		*SourceMinTexel.ToString(), *SourceMaxTexel.ToString(), DataTexelCm, *TerrainTransform.ToString());
	ArcWaterRiverNetworkExtraction::FArcWaterRiverMaskGrid RiversMask;
	if (!SampleRivers(*Subsystem, *Terrain, SourceMinTexel, SourceMaxTexel,
		Settings.RiversThreshold, RiversMask, OutError, ShouldContinue))
	{
		return false;
	}
	UE_LOGF(LogTemp, Display,"[RiverGen] SOURCE READ COMPLETE rect=%ls..%ls tiles=%d active=%lld bytes=%lld",
		*RiversMask.SourceRect.Min.ToString(), *RiversMask.SourceRect.Max.ToString(),
		RiversMask.Tiles.Num(), RiversMask.ActivePixelCount, RiversMask.GetAllocatedBytes());
	if (!ShouldContinue())
	{
		return Fail(InputsStillMatch()
			? LOCTEXT("CancelledAfterRead", "River generation was cancelled.")
			: LOCTEXT("InputsChangedAfterRead", "River generation inputs changed while rebuilding; run Rebuild Rivers again."), OutError);
	}

	SlowTask.EnterProgressFrame(25.0f, LOCTEXT("ExtractProgress", "Extracting the complete river network"));
	UE_LOGF(LogTemp, Display,"[RiverGen] PHASE Extracting the complete river network t=%.6f", FPlatformTime::Seconds());
	const FVector TerrainScale = TerrainTransform.GetScale3D();
	const FVector2D DataCmPerTexel(DataTexelCm * TerrainScale.X, DataTexelCm * TerrainScale.Y);
	TArray<ArcWaterRiverNetworkExtraction::FArcWaterRiverPath> Paths;
	TArray<FIntPoint> JunctionTexels;
	const float ExtractionStart = SlowTask.CompletedWork;
	const float ExtractionScope = SlowTask.CurrentFrameScope;
	const TFunction<bool(float)> ExtractionShouldContinue =
		[&ShouldContinue, &SlowTask, ExtractionStart, ExtractionScope](float Progress) -> bool
	{
		SlowTask.CompletedWork = FMath::Max(SlowTask.CompletedWork,
			ExtractionStart + ExtractionScope * FMath::Clamp(Progress, 0.0f, 1.0f));
		SlowTask.CurrentFrameScope = FMath::Max(0.0f,
			ExtractionStart + ExtractionScope - SlowTask.CompletedWork);
		SlowTask.TickProgress();
		return ShouldContinue();
	};
	if (!ArcWaterRiverNetworkExtraction::ExtractNetwork(MoveTemp(RiversMask),
		DataCmPerTexel, Settings.SplineToleranceCm, Paths, JunctionTexels, OutError,
		ExtractionShouldContinue))
	{
		if (!InputsStillMatch())
		{
			OutError = LOCTEXT("InputsChangedDuringExtraction", "River generation inputs changed while rebuilding; run Rebuild Rivers again.");
		}
		return false;
	}
	if (Paths.IsEmpty())
	{
		return Fail(LOCTEXT("NoRivers", "The Rivers sources contain no active river network at the current threshold."), OutError);
	}

	FArcWaterRiverNetworkInput NetworkInput;
	UE_LOGF(LogTemp, Display,"[RiverGen] EXTRACTION COMPLETE paths=%d junctions=%d elapsed=%.3fs",
		Paths.Num(), JunctionTexels.Num(), FPlatformTime::Seconds() - RiverLogStart);
	if (!BuildNetworkInput(Paths, JunctionTexels, DataTexelCm,
		TerrainTransform, GeneratedFrame, NetworkInput, OutError, ShouldContinue))
	{
		return false;
	}
	Paths.Empty();
	JunctionTexels.Empty();

	FArcWaterRiverHeightPageSampler HeightSampler(
		*Subsystem, *Terrain, Modifier, TerrainTransform, GeneratedFrame, ShouldContinue);
	if (!HeightSampler.IsValid(OutError))
	{
		return false;
	}
	NetworkInput.Terrain.SampleHeight = [&HeightSampler](const FVector2D& Position, float& OutHeight) -> bool
	{
		return HeightSampler.Sample(Position, OutHeight);
	};
	NetworkInput.Terrain.WorldZOffsetCm = static_cast<float>(GeneratedFrame.GetTranslation().Z);
	NetworkInput.Terrain.WorldZScale = 1.0f;

	SlowTask.EnterProgressFrame(45.0f, LOCTEXT("ChannelsProgress", "Building river channels from terrain height pages"));
	UE_LOGF(LogTemp, Display,"[RiverGen] PHASE Building river channels from terrain height pages t=%.6f", FPlatformTime::Seconds());
	FArcWaterRiverGeneratedData GeneratedData;
	const bool bChannelsBuilt = ArcWaterRiverChannel::BuildChannels(
		NetworkInput, Settings, GeneratedData, OutError, ShouldContinue);
	const FText HeightSamplerError = HeightSampler.GetError();
	NetworkInput.Terrain.SampleHeight = TFunction<bool(const FVector2D&, float&)>();
	HeightSampler.LogStats();
	HeightSampler.ReleasePages();
	if (!bChannelsBuilt)
	{
		if (!InputsStillMatch())
		{
			OutError = LOCTEXT("InputsChangedDuringChannels", "River generation inputs changed while rebuilding; run Rebuild Rivers again.");
		}
		else if (!HeightSamplerError.IsEmpty())
		{
			OutError = HeightSamplerError;
		}
		return false;
	}
	NetworkInput = FArcWaterRiverNetworkInput();
	if (!ShouldContinue())
	{
		return Fail(InputsStillMatch()
			? LOCTEXT("CancelledBeforePublish", "River generation was cancelled.")
			: LOCTEXT("InputsChangedBeforePublish", "River generation inputs changed before publication; run Rebuild Rivers again."), OutError);
	}

	OutGeneratedData = MoveTemp(GeneratedData);
	OutGeneratedFrame = GeneratedFrame;
	return true;
}

bool ArcWaterRiverGeneration::Rebuild(UArcWaterRiverModifier& Modifier, FText& OutError)
{
	const double RiverLogStart = FPlatformTime::Seconds();
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN ArcWaterRiverGeneration::Rebuild t=%.6f", RiverLogStart);
	ON_SCOPE_EXIT
	{
		UE_LOGF(LogTemp, Display,"[RiverGen] END ArcWaterRiverGeneration::Rebuild elapsed=%.3fs, error=%ls",
			FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
	};
	using namespace ArcWaterRiverGenerationPrivate_7a3d;

	OutError = FText::GetEmpty();
	if (!IsInGameThread())
	{
		return Fail(LOCTEXT("GameThreadOnlyRebuild", "River generation must run on the game thread."), OutError);
	}
	AActor* const Owner = Modifier.GetOwner();
	if (Modifier.IsTemplate() || Owner == nullptr || Owner->IsTemplate() || Modifier.GetWorld() == nullptr)
	{
		return Fail(LOCTEXT("InstanceOnlyRebuild", "Rebuild Rivers is available only on a level instance component."), OutError);
	}

	Modifier.ClearGeneratedRivers();
	FArcWaterRiverGeneratedData GeneratedData;
	FTransform GeneratedFrame;
	if (!Compute(Modifier, GeneratedData, GeneratedFrame, OutError))
	{
		return false;
	}

	const FTransform InitialModifierTransform = Modifier.GetComponentTransform();
	AArcTerrain* const Terrain = Modifier.AffectedTerrain.Get();
	const FVector OwnerWorldLocation = Owner->GetActorLocation();
	const FVector2D GenerationBoundsSize = Modifier.GenerationBoundsSize;
	const TWeakObjectPtr<AActor> OwnerGuard(Owner);
	const TWeakObjectPtr<AArcTerrain> TerrainGuard(Terrain);
	const FTransform TerrainTransform = Terrain != nullptr ? Terrain->GetTerrainTransform() : FTransform::Identity;
	FArcWaterRiverModifierSettings Settings = Modifier.Settings;
	UCurveFloat* const SlopeCurveSource = Settings.WidthScaleBySlope.ExternalCurve;
	UCurveFloat* const ElevationCurveSource = Settings.WidthScaleByElevation.ExternalCurve;
	SnapshotCurve(Settings.WidthScaleBySlope);
	SnapshotCurve(Settings.WidthScaleByElevation);
	const TWeakObjectPtr<UCurveFloat> SlopeCurveGuard(SlopeCurveSource);
	const TWeakObjectPtr<UCurveFloat> ElevationCurveGuard(ElevationCurveSource);
	const bool bHadSlopeCurve = SlopeCurveSource != nullptr;
	const bool bHadElevationCurve = ElevationCurveSource != nullptr;
	const TFunction<bool()> InputsStillMatch = [&Modifier, OwnerGuard, TerrainGuard, TerrainTransform,
		InitialModifierTransform, OwnerWorldLocation, GenerationBoundsSize, Settings,
		SlopeCurveGuard, ElevationCurveGuard,
		bHadSlopeCurve, bHadElevationCurve]() -> bool
	{
		const AActor* const CurrentOwner = OwnerGuard.Get();
		const AArcTerrain* const CurrentTerrain = TerrainGuard.Get();
		const UCurveFloat* const CurrentSlopeCurve = Modifier.Settings.WidthScaleBySlope.ExternalCurve;
		const UCurveFloat* const CurrentElevationCurve = Modifier.Settings.WidthScaleByElevation.ExternalCurve;
		return CurrentOwner != nullptr && Modifier.GetOwner() == CurrentOwner
			&& CurrentOwner->GetActorLocation().Equals(OwnerWorldLocation, 1.e-6)
			&& CurrentTerrain != nullptr && Modifier.AffectedTerrain.Get() == CurrentTerrain
			&& CurrentTerrain->GetTerrainTransform().Equals(TerrainTransform, 1.e-6)
			&& Modifier.GetComponentTransform().Equals(InitialModifierTransform, 1.e-6)
			&& (CurrentSlopeCurve != nullptr) == bHadSlopeCurve
			&& (CurrentElevationCurve != nullptr) == bHadElevationCurve
			&& (!bHadSlopeCurve || CurrentSlopeCurve == SlopeCurveGuard.Get())
			&& (!bHadElevationCurve || CurrentElevationCurve == ElevationCurveGuard.Get())
			&& Modifier.GenerationBoundsSize == GenerationBoundsSize
			&& SettingsEqual(Modifier.Settings, Settings);
	};
	FScopedSlowTask SlowTask(10.0f, LOCTEXT("PublishProgress", "Publishing the river result"));
	SlowTask.MakeDialog(true);
	SlowTask.EnterProgressFrame(10.0f);
	const TFunction<bool()> ShouldContinue = [&SlowTask, &InputsStillMatch]() -> bool
	{
		return !SlowTask.ShouldCancel() && InputsStillMatch();
	};
	if (!ShouldContinue())
	{
		return Fail(InputsStillMatch()
			? LOCTEXT("CancelledBeforePublish", "River generation was cancelled.")
			: LOCTEXT("InputsChangedBeforePublish", "River generation inputs changed before publication; run Rebuild Rivers again."), OutError);
	}

	UE_LOGF(LogTemp, Display,"[RiverGen] PHASE Publishing the river result t=%.6f", FPlatformTime::Seconds());
	const bool bApplied = Modifier.ApplyGeneratedData(MoveTemp(GeneratedData), GeneratedFrame, OutError, ShouldContinue);
	UE_LOGF(LogTemp, Display,"[RiverGen] PUBLICATION applied=%d reaches=%d splineModifiers=%d maxFieldDims=%ls spacingCm=%.3f estimatedPairBytes=%lld error=%ls elapsed=%.3fs",
		bApplied ? 1 : 0, Modifier.GetGeneratedData().Reaches.Num(),
		Modifier.GetGeneratedSplineModifierCount(), *Modifier.GetGeneratedMaxFieldDims().ToString(),
		Modifier.GetGeneratedFieldSpacingCm(), Modifier.GetGeneratedEstimatedRasterBytes(), *OutError.ToString(),
		FPlatformTime::Seconds() - RiverLogStart);
	if (bApplied)
	{
		Modifier.MarkPackageDirty();
	}
	if (!bApplied && !InputsStillMatch())
	{
		OutError = LOCTEXT("InputsChangedDuringPublish", "River generation inputs changed before publication; run Rebuild Rivers again.");
	}
	return bApplied;
}

#undef LOCTEXT_NAMESPACE
