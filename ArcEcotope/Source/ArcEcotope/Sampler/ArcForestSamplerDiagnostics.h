// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"

ARCECOTOPE_API DECLARE_LOG_CATEGORY_EXTERN(LogArcForestSampler, Log, All);

namespace UE::ArcForest::Sampler
{
	struct FArcForestSamplerDiagnosticsBand
	{
		FGuid SpeciesId;
		int32 BandIndex = INDEX_NONE;
		float MinHeightCm = 0.0f;
		float MaxHeightCm = 0.0f;
		double ExpectedStems = 0.0;
	};

	struct FArcForestSamplerDiagnosticsStem
	{
		FVector2D PositionCm = FVector2D::ZeroVector;
		double CrownRadiusCm = 0.0;
		int32 EntryOrdinal = INDEX_NONE;
		int32 PassIndex = INDEX_NONE;
		int32 RecordCellIndex = INDEX_NONE;
	};

	struct FArcForestSamplerDiagnosticsCell
	{
		int32 RecordCellIndex = INDEX_NONE;
		int32 UsableStart = 0;
		int32 UsableCount = 0;
	};

	struct FArcForestSamplerDiagnosticsRejections
	{
		int64 GeneratedCandidates = 0;
		int64 SkippedRecordCells = 0;
		int64 OffUsableSurface = 0;
		int64 MissingSurfaceZ = 0;
		int64 NoBandWithHabitat = 0;
		int64 NoBandDrawn = 0;
		int64 AcceptanceRejected = 0;
		int64 Inhibited = 0;
	};

	struct FArcForestSamplerDiagnosticsContext
	{
		FGuid SimulationId;
		int32 Seed = 0;
		double MaximumCrownOccupancy = 0.0;
		double UnevenAgedness = 0.0;
		FVector2D BaseOriginCm = FVector2D::ZeroVector;
		FIntPoint BaseDimensions = FIntPoint::ZeroValue;
		double BaseCellSizeCm = 0.0;
		FArcForestSamplerDiagnosticsRejections Rejections;
		TArrayView<const FArcForestSamplerDiagnosticsBand> DescendingBands;
		TArrayView<const FArcForestSamplerDiagnosticsStem> Stems;
		TArrayView<const FArcForestSamplerDiagnosticsCell> EmittingCells;
		TArrayView<const int32> UsableBaseCells;
	};

	ARCECOTOPE_API void ReportSamplerDiagnostics(const FArcForestSamplerDiagnosticsContext& Context);
}
