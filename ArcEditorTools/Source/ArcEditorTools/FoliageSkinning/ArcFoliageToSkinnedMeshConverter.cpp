// Copyright Lukasz Baran. All Rights Reserved.

#include "FoliageSkinning/ArcFoliageToSkinnedMeshConverter.h"

#include "FoliageSkinning/ArcFoliageToSkinnedMeshSettings.h"
#include "FoliageSkinning/ArcFoliageMaskTracer.h"
#include "FoliageSkinning/ArcFoliageCutPipeline.h"
#include "FoliageSkinning/ArcFoliageSkeletonBuilder.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

#if WITH_EDITOR
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetUtils/CreateSkeletalMeshUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicWindImportData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "Logging/MessageLog.h"
#include "Misc/PackageName.h"
#include "ReferenceSkeleton.h"
#include "UObject/Package.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "ArcFoliageToSkinnedMeshConverter"

DEFINE_LOG_CATEGORY_STATIC(LogArcFoliageConverter, Log, All);

#if WITH_EDITOR
namespace ArcFoliageToSkinnedMeshConverterCpp
{
	using UE::Geometry::FDynamicMesh3;

	/** Report a specific conversion failure to the editor error log (with a toast) and return nullptr. */
	USkeletalMesh* Fail(const FText& Message)
	{
		FMessageLog Log("EditorErrors");
		Log.Error(Message);
		Log.Notify(LOCTEXT("ConversionFailed", "Foliage -> skinned mesh conversion failed."), EMessageSeverity::Error);
		UE_LOG(LogArcFoliageConverter, Warning, TEXT("%s"), *Message.ToString());
		return nullptr;
	}
}
#endif // WITH_EDITOR

USkeletalMesh* UArcFoliageToSkinnedMeshConverter::ConvertStaticMeshToSkinnedFoliage(
	UStaticMesh* StaticMesh,
	UArcFoliageToSkinnedMeshSettings* Settings)
{
#if WITH_EDITOR
	namespace Local = ArcFoliageToSkinnedMeshConverterCpp;
	using UE::Geometry::FDynamicMesh3;

	if (StaticMesh == nullptr)
	{
		return Local::Fail(LOCTEXT("NullStaticMesh", "Convert to Skinned Foliage: no source static mesh was provided."));
	}

	UArcFoliageToSkinnedMeshSettings* EffectiveSettings = Settings != nullptr
		? Settings
		: GetMutableDefault<UArcFoliageToSkinnedMeshSettings>();

	// --- 1-3. Source geometry -> mask trace -> card cut (shared pipeline) ---------------------------
	Arc::FoliageCut::FArcCutPipelineParams PipelineParams;
	PipelineParams.bUseOpacityMask = EffectiveSettings->bUseOpacityMask;
	PipelineParams.MaskTextureOverride = EffectiveSettings->MaskTextureOverride.Get();
	PipelineParams.WhiteLevelOverride = EffectiveSettings->WhiteLevelOverride;
	PipelineParams.bPassThroughUnmaskedSections = EffectiveSettings->bPassThroughUnmaskedSections;
	PipelineParams.TraceTemplate.SampleResolution = EffectiveSettings->SampleResolution;
	PipelineParams.TraceTemplate.bInvert = EffectiveSettings->bInvertMask;
	PipelineParams.TraceTemplate.Channel = EffectiveSettings->Channel;
	PipelineParams.TraceTemplate.MinCurveBoundsArea = EffectiveSettings->MinCurveBoundsArea;
	PipelineParams.TraceTemplate.SmoothingIterations = EffectiveSettings->SmoothingIterations;
	PipelineParams.TraceTemplate.SimplificationAmount = EffectiveSettings->SimplificationAmount;
	// TraceTemplate.WhiteLevel left default — resolved per-material inside the helper.

	FDynamicMesh3 CutMesh;
	TArray<UMaterialInterface*> AssetMaterials;
	const Arc::FoliageCut::ECutPipelineResult CutResult =
		Arc::FoliageCut::BuildCutMeshFromStaticMesh(*StaticMesh, PipelineParams, CutMesh, AssetMaterials);

	switch (CutResult)
	{
	case Arc::FoliageCut::ECutPipelineResult::Ok:
		break;
	case Arc::FoliageCut::ECutPipelineResult::NoMeshDescription:
		return Local::Fail(FText::Format(
			LOCTEXT("NullMeshDescription", "Convert to Skinned Foliage: '{0}' has no LOD0 mesh description (stripped source / Nanite-fallback-only import)."),
			FText::FromString(StaticMesh->GetName())));
	case Arc::FoliageCut::ECutPipelineResult::NoMaskResolved:
		return Local::Fail(FText::Format(
			LOCTEXT("NoMaskResolved", "Convert to Skinned Foliage: no opacity mask could be resolved on any material slot of '{0}', and no mask-texture override was set."),
			FText::FromString(StaticMesh->GetName())));
	case Arc::FoliageCut::ECutPipelineResult::ZeroClosedCurves:
		return Local::Fail(FText::Format(
			LOCTEXT("ZeroClosedCurves", "Convert to Skinned Foliage: the opacity mask(s) on '{0}' produced no closed contour curves (check WhiteLevel / channel / invert settings)."),
			FText::FromString(StaticMesh->GetName())));
	case Arc::FoliageCut::ECutPipelineResult::ZeroGeometry:
		return Local::Fail(FText::Format(
			LOCTEXT("ZeroGeometry", "Convert to Skinned Foliage: '{0}' produced no geometry to skin."),
			FText::FromString(StaticMesh->GetName())));
	default:
		return Local::Fail(FText::Format(
			LOCTEXT("ZeroGeometry", "Convert to Skinned Foliage: '{0}' produced no geometry to skin."),
			FText::FromString(StaticMesh->GetName())));
	}

	// --- 4. Skeleton + skin bind on the cut mesh ---------------------------------------------------
	Arc::FoliageSkeleton::FArcSkeletonBuildParams SkeletonParams;
	SkeletonParams.MinBonesPerClump = EffectiveSettings->MinBonesPerClump;
	SkeletonParams.MaxBonesPerClump = EffectiveSettings->MaxBonesPerClump;
	SkeletonParams.BoneSpacing = EffectiveSettings->BoneSpacing;
	SkeletonParams.MaxClumps = EffectiveSettings->MaxClumps;
	SkeletonParams.MaxInfluences = EffectiveSettings->MaxInfluences;
	SkeletonParams.BindStiffness = EffectiveSettings->BindStiffness;

	FReferenceSkeleton RefSkeleton;
	Arc::FoliageSkeleton::FArcSkeletonBuildStats SkeletonStats;
	if (!Arc::FoliageSkeleton::BuildClumpSkeletonAndBind(CutMesh, SkeletonParams, RefSkeleton, SkeletonStats))
	{
		return Local::Fail(FText::Format(
			LOCTEXT("SkeletonBindFailed", "Convert to Skinned Foliage: failed to build the clump skeleton / bind skin weights for '{0}'."),
			FText::FromString(StaticMesh->GetName())));
	}

	// --- 5. Asset creation: USkeleton then USkeletalMesh -------------------------------------------
	const FString SourcePackageName = StaticMesh->GetOutermost()->GetName();
	const FString PackagePath = FPackageName::GetLongPackagePath(SourcePackageName);
	const FString BaseName = StaticMesh->GetName();
	const FString MeshAssetName = BaseName + EffectiveSettings->AssetSuffix;
	const FString MeshPackagePath = PackagePath / MeshAssetName;
	const FString SkeletonAssetName = MeshAssetName + TEXT("_Skeleton");
	const FString SkeletonPackagePath = PackagePath / SkeletonAssetName;

	UPackage* SkeletonPackage = CreatePackage(*SkeletonPackagePath);
	if (SkeletonPackage == nullptr)
	{
		return Local::Fail(FText::Format(
			LOCTEXT("SkeletonPackageFailed", "Convert to Skinned Foliage: could not create the skeleton package '{0}'."),
			FText::FromString(SkeletonPackagePath)));
	}
	SkeletonPackage->FullyLoad();

	USkeleton* Skeleton = NewObject<USkeleton>(SkeletonPackage, FName(*SkeletonAssetName), RF_Public | RF_Standalone);
	if (Skeleton == nullptr)
	{
		return Local::Fail(FText::Format(
			LOCTEXT("SkeletonObjectFailed", "Convert to Skinned Foliage: could not create the USkeleton object '{0}'."),
			FText::FromString(SkeletonAssetName)));
	}

	UE::AssetUtils::FSkeletalMeshAssetOptions Options;
	Options.NewAssetPath = MeshPackagePath;
	Options.Skeleton = Skeleton;
	Options.RefSkeleton = &RefSkeleton;
	Options.NumSourceModels = 1;
	Options.SourceMeshes.DynamicMeshes = { &CutMesh };
	Options.AssetMaterials = AssetMaterials;
	Options.NumMaterialSlots = AssetMaterials.Num();
	Options.bApplyNaniteSettings = true;
	Options.NaniteSettings.bEnabled = EffectiveSettings->bEnableNanite;
	Options.NaniteSettings.ShapePreservation = EffectiveSettings->NaniteShapePreservation;

	UE::AssetUtils::FSkeletalMeshResults Results;
	const UE::AssetUtils::ECreateSkeletalMeshResult CreateResult =
		UE::AssetUtils::CreateSkeletalMeshAsset(Options, Results);
	if (CreateResult != UE::AssetUtils::ECreateSkeletalMeshResult::Ok || Results.SkeletalMesh == nullptr)
	{
		return Local::Fail(FText::Format(
			LOCTEXT("AssetCreationFailed", "Convert to Skinned Foliage: CreateSkeletalMeshAsset failed for '{0}' (code {1})."),
			FText::FromString(MeshAssetName),
			FText::AsNumber(static_cast<int32>(CreateResult))));
	}

	// --- 6. Dynamic Wind asset user data -----------------------------------------------------------
	// One ground-cover simulation group holding every clump bone. Bone index 0 ("root") is left out of
	// the joint list so each chain anchors at its clump's first bone. ImportSkeletalData creates the
	// UDynamicWindSkeletalData user data, derives the bone chains, and sets bIsEnabled.
	if (EffectiveSettings->bAddDynamicWindData)
	{
		FDynamicWindSkeletalImportData WindImportData;
		WindImportData.bIsGroundCover = true;

		FDynamicWindSimulationGroupData WindGroup;
		WindGroup.Influence = 1.0f;
		WindImportData.SimulationGroups.Add(WindGroup);

		const TArray<FMeshBoneInfo>& RawBones = RefSkeleton.GetRawRefBoneInfo();
		for (int32 BoneIndex = 1; BoneIndex < RawBones.Num(); ++BoneIndex)
		{
			WindImportData.Joints.Add({RawBones[BoneIndex].Name, 0});
		}

		if (DynamicWind::ImportSkeletalData(*Results.SkeletalMesh, WindImportData) == nullptr)
		{
			UE_LOG(LogArcFoliageConverter, Warning,
				TEXT("Convert to Skinned Foliage: failed to add Dynamic Wind skeletal data to '%s'."),
				*MeshAssetName);
		}
	}

	// --- 7. Registry + dirty + optional save -------------------------------------------------------
	// CreateSkeletalMeshAsset already ran MergeAllBonesToBoneTree and set the skeleton preview mesh;
	// register both new assets so they appear in the Content Browser.
	FAssetRegistryModule::AssetCreated(Skeleton);
	FAssetRegistryModule::AssetCreated(Results.SkeletalMesh);

	UPackage* MeshPackage = Results.SkeletalMesh->GetOutermost();
	SkeletonPackage->MarkPackageDirty();
	if (MeshPackage != nullptr)
	{
		MeshPackage->MarkPackageDirty();
	}

	if (EffectiveSettings->bSavePackages)
	{
		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Add(SkeletonPackage);
		if (MeshPackage != nullptr)
		{
			PackagesToSave.Add(MeshPackage);
		}
		UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, /*bOnlyDirty=*/false);
	}

	UE_LOG(LogArcFoliageConverter, Log,
		TEXT("Converted '%s' -> '%s': %d clump(s), %d bone(s), %d triangle(s)."),
		*BaseName, *MeshAssetName, SkeletonStats.ClumpCount, SkeletonStats.BoneCount, CutMesh.TriangleCount());

	return Results.SkeletalMesh;
#else  // WITH_EDITOR
	return nullptr;
#endif // WITH_EDITOR
}

#undef LOCTEXT_NAMESPACE
