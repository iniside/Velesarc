// Copyright Lukasz Baran. All Rights Reserved.

#include "FoliageSkinning/ArcFoliageCutPipeline.h"

#include "FoliageSkinning/ArcFoliageMaskTracer.h"
#include "FoliageSkinning/ArcFoliageCardCutter.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Polygon2.h"

#if WITH_EDITOR
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshIndexMappings.h"
#include "DynamicMeshEditor.h"
#include "Engine/Texture2D.h"
#include "MeshDescription.h"
#include "MeshDescriptionToDynamicMesh.h"
#endif // WITH_EDITOR

DEFINE_LOG_CATEGORY_STATIC(LogArcFoliageCut, Log, All);

Arc::FoliageCut::ECutPipelineResult Arc::FoliageCut::BuildCutMeshFromStaticMesh(
	const UStaticMesh& StaticMesh,
	const FArcCutPipelineParams& Params,
	UE::Geometry::FDynamicMesh3& OutCutMesh,
	TArray<UMaterialInterface*>& OutAssetMaterials)
{
#if WITH_EDITOR
	using UE::Geometry::FDynamicMesh3;
	using UE::Geometry::FDynamicMeshEditor;
	using UE::Geometry::FMeshIndexMappings;
	using UE::Geometry::FPolygon2f;

	// --- 1. Source geometry: FMeshDescription -> FDynamicMesh3 -------------------------------------
	FMeshDescription* MeshDescription = StaticMesh.GetMeshDescription(0);
	if (MeshDescription == nullptr)
	{
		return ECutPipelineResult::NoMeshDescription;
	}

	FDynamicMesh3 SourceMesh;
	{
		FMeshDescriptionToDynamicMesh Converter;
		// Compact polygon-group IDs so they arrive as section indices (== MaterialID) on the dynamic mesh.
		Converter.bUseCompactedPolygonGroupIDValues = true;
		Converter.Convert(MeshDescription, SourceMesh);
	}

	// --- 2. Per material slot: resolve mask + white level, trace once per unique mask texture -------
	const TArray<FStaticMaterial>& StaticMaterials = StaticMesh.GetStaticMaterials();

	OutAssetMaterials.Reserve(StaticMaterials.Num());
	for (const FStaticMaterial& StaticMaterial : StaticMaterials)
	{
		OutAssetMaterials.Add(StaticMaterial.MaterialInterface);
	}

	FDynamicMesh3& CutMesh = OutCutMesh;

	if (Params.bUseOpacityMask)
	{
		TMap<UTexture2D*, TArray<FPolygon2f>> CurveCache;   // Traced curves per unique mask texture (empty = trace failed).
		TMap<UTexture2D*, TSet<int32>> SectionsByTexture;   // Masked section (slot) indices grouped by their mask texture.
		TSet<int32> AllMaskedSections;                      // Every masked section index across all textures.

		for (int32 SlotIndex = 0; SlotIndex < StaticMaterials.Num(); ++SlotIndex)
		{
			UMaterialInterface* Material = StaticMaterials[SlotIndex].MaterialInterface;

			UTexture2D* MaskTexture = Params.MaskTextureOverride != nullptr
				? Params.MaskTextureOverride
				: Arc::FoliageMask::ResolveMaskTexture(Material);
			if (MaskTexture == nullptr)
			{
				continue; // Unmasked slot — handled by the pass-through path below.
			}

			AllMaskedSections.Add(SlotIndex);
			SectionsByTexture.FindOrAdd(MaskTexture).Add(SlotIndex);

			if (!CurveCache.Contains(MaskTexture))
			{
				const float WhiteLevel = Params.WhiteLevelOverride >= 0.0f
					? Params.WhiteLevelOverride
					: Arc::FoliageMask::ResolveWhiteLevel(Material);

				Arc::FoliageMask::FArcMaskTraceParams TraceParams = Params.TraceTemplate;
				TraceParams.WhiteLevel = WhiteLevel;

				TArray<FPolygon2f> Curves;
				const Arc::FoliageMask::ETraceResult TraceResult = Arc::FoliageMask::TracePerimeterCurves(
					MaskTexture, TraceParams, Curves);
				UE_LOG(LogArcFoliageCut, Display,
					TEXT("Mask trace '%s': result=%d curves=%d (WhiteLevel=%.3f Channel=%d Invert=%d SampleRes=%d)"),
					*MaskTexture->GetName(), static_cast<int32>(TraceResult), Curves.Num(),
					WhiteLevel, static_cast<int32>(Params.TraceTemplate.Channel),
					Params.TraceTemplate.bInvert ? 1 : 0, Params.TraceTemplate.SampleResolution);
				if (TraceResult != Arc::FoliageMask::ETraceResult::Success)
				{
					Curves.Reset(); // Failed trace -> empty group; its sections are cut away, not stamped.
				}
				CurveCache.Add(MaskTexture, MoveTemp(Curves));
			}
		}

		if (AllMaskedSections.Num() == 0)
		{
			return ECutPipelineResult::NoMaskResolved;
		}

		// Count texture groups whose trace produced usable closed curves.
		int32 ValidTextureGroups = 0;
		for (const TPair<UTexture2D*, TArray<FPolygon2f>>& Pair : CurveCache)
		{
			if (Pair.Value.Num() > 0)
			{
				++ValidTextureGroups;
			}
		}
		if (ValidTextureGroups == 0)
		{
			return ECutPipelineResult::ZeroClosedCurves;
		}

		// --- 3. Card cut (composition documented below) ------------------------------------------------
		//
		// Composition strategy:
		//   * One unique mask texture (Kikuyu / GrassyEmbankment, and Lolium once the override collapses
		//     every slot onto one texture): a single CutMeshToMask does the whole job — the traced curves
		//     cut the masked sections and bPassThroughUnmaskedSections copies the rest in one pass.
		//   * Multiple unique mask textures (the general path): CutMeshToMask clears its output each call,
		//     so it is run once per texture group (only that group's sections masked,
		//     bPassThroughUnmaskedSections=false) into a scratch mesh that is appended into a single
		//     combined mesh via FDynamicMeshEditor::AppendMesh. A final pass — MaskedSectionIndices = ALL
		//     masked sections (so none of them are duplicated as pass-through) with an EMPTY curve list and
		//     bPassThroughUnmaskedSections honoring the setting — appends the never-masked sections exactly
		//     once. No triangle is duplicated and every emitted triangle keeps its source MaterialID.
		if (SectionsByTexture.Num() == 1)
		{
			const TPair<UTexture2D*, TSet<int32>>& OnlyGroup = *SectionsByTexture.CreateConstIterator();
			const TArray<FPolygon2f>& Curves = CurveCache.FindChecked(OnlyGroup.Key);

			Arc::FoliageCut::FArcCardCutParams CutParams;
			CutParams.MaskedSectionIndices = OnlyGroup.Value;
			CutParams.bPassThroughUnmaskedSections = Params.bPassThroughUnmaskedSections;

			Arc::FoliageCut::FArcCardCutStats CutStats;
			Arc::FoliageCut::CutMeshToMask(SourceMesh, Curves, CutParams, CutMesh, CutStats);
		}
		else
		{
			CutMesh.EnableAttributes();
			CutMesh.Attributes()->EnableMaterialID();
			FDynamicMeshEditor CombineEditor(&CutMesh);

			for (const TPair<UTexture2D*, TSet<int32>>& Group : SectionsByTexture)
			{
				const TArray<FPolygon2f>& Curves = CurveCache.FindChecked(Group.Key);
				if (Curves.Num() == 0)
				{
					continue; // Failed-trace group: its sections are cut away.
				}

				Arc::FoliageCut::FArcCardCutParams CutParams;
				CutParams.MaskedSectionIndices = Group.Value;
				CutParams.bPassThroughUnmaskedSections = false;

				FDynamicMesh3 GroupMesh;
				Arc::FoliageCut::FArcCardCutStats CutStats;
				if (Arc::FoliageCut::CutMeshToMask(SourceMesh, Curves, CutParams, GroupMesh, CutStats))
				{
					FMeshIndexMappings IndexMaps;
					CombineEditor.AppendMesh(&GroupMesh, IndexMaps);
				}
			}

			// Final pass-through of the never-masked sections (all masked sections are marked so they are
			// neither re-cut nor duplicated; the empty curve list stamps nothing for them).
			if (Params.bPassThroughUnmaskedSections)
			{
				Arc::FoliageCut::FArcCardCutParams PassThroughParams;
				PassThroughParams.MaskedSectionIndices = AllMaskedSections;
				PassThroughParams.bPassThroughUnmaskedSections = true;

				const TArray<FPolygon2f> NoCurves;
				FDynamicMesh3 PassThroughMesh;
				Arc::FoliageCut::FArcCardCutStats PassThroughStats;
				if (Arc::FoliageCut::CutMeshToMask(SourceMesh, NoCurves, PassThroughParams, PassThroughMesh, PassThroughStats))
				{
					FMeshIndexMappings IndexMaps;
					CombineEditor.AppendMesh(&PassThroughMesh, IndexMaps);
				}
			}
		}
	}
	else
	{
		// No opacity mask requested: pass the full source geometry through unchanged.
		CutMesh = MoveTemp(SourceMesh);
		ensure(CutMesh.HasAttributes() && CutMesh.Attributes()->HasMaterialID());
	}

	if (CutMesh.TriangleCount() == 0)
	{
		return ECutPipelineResult::ZeroGeometry;
	}

	return ECutPipelineResult::Ok;
#else  // WITH_EDITOR
	return ECutPipelineResult::NoMeshDescription;
#endif // WITH_EDITOR
}
