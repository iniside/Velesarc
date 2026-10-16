// Copyright Lukasz Baran. All Rights Reserved.

#include "FoliageSkinning/ArcFoliageToStaticMeshConverter.h"

#include "FoliageSkinning/ArcFoliageToStaticMeshSettings.h"
#include "FoliageSkinning/ArcFoliageCutPipeline.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetUtils/CreateStaticMeshUtil.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "FileHelpers.h"
#include "Logging/MessageLog.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "ArcFoliageToStaticMeshConverter"

DEFINE_LOG_CATEGORY_STATIC(LogArcFoliageStaticConverter, Log, All);

#if WITH_EDITOR
namespace ArcFoliageToStaticMeshConverterCpp
{
	/** Report a specific conversion failure to the editor error log (with a toast) and return nullptr. */
	UStaticMesh* Fail(const FText& Message)
	{
		FMessageLog Log("EditorErrors");
		Log.Error(Message);
		Log.Notify(LOCTEXT("ConversionFailed", "Foliage -> Nanite static mesh conversion failed."), EMessageSeverity::Error);
		UE_LOG(LogArcFoliageStaticConverter, Warning, TEXT("%s"), *Message.ToString());
		return nullptr;
	}
}
#endif // WITH_EDITOR

UStaticMesh* UArcFoliageToStaticMeshConverter::ConvertStaticMeshToNaniteStaticMesh(
	UStaticMesh* StaticMesh,
	UArcFoliageToStaticMeshSettings* Settings)
{
#if WITH_EDITOR
	namespace Local = ArcFoliageToStaticMeshConverterCpp;
	using UE::Geometry::FDynamicMesh3;

	if (StaticMesh == nullptr)
	{
		return Local::Fail(LOCTEXT("NullStaticMesh", "Convert to Nanite Static Mesh: no source static mesh was provided."));
	}

	UArcFoliageToStaticMeshSettings* EffectiveSettings = Settings != nullptr
		? Settings
		: GetMutableDefault<UArcFoliageToStaticMeshSettings>();

	// --- 1. Source -> cut geometry (shared pipeline) ----------------------------------------------
	Arc::FoliageCut::FArcCutPipelineParams Params;
	Params.bUseOpacityMask = EffectiveSettings->bUseOpacityMask;
	Params.MaskTextureOverride = EffectiveSettings->MaskTextureOverride.Get();
	Params.WhiteLevelOverride = EffectiveSettings->WhiteLevelOverride;
	Params.bPassThroughUnmaskedSections = EffectiveSettings->bPassThroughUnmaskedSections;
	Params.TraceTemplate.SampleResolution = EffectiveSettings->SampleResolution;
	Params.TraceTemplate.bInvert = EffectiveSettings->bInvertMask;
	Params.TraceTemplate.Channel = EffectiveSettings->Channel;
	Params.TraceTemplate.MinCurveBoundsArea = EffectiveSettings->MinCurveBoundsArea;
	Params.TraceTemplate.SmoothingIterations = EffectiveSettings->SmoothingIterations;
	Params.TraceTemplate.SimplificationAmount = EffectiveSettings->SimplificationAmount;

	FDynamicMesh3 CutMesh;
	TArray<UMaterialInterface*> AssetMaterials;
	const Arc::FoliageCut::ECutPipelineResult CutResult =
		Arc::FoliageCut::BuildCutMeshFromStaticMesh(*StaticMesh, Params, CutMesh, AssetMaterials);

	switch (CutResult)
	{
	case Arc::FoliageCut::ECutPipelineResult::Ok:
		break;
	case Arc::FoliageCut::ECutPipelineResult::NoMeshDescription:
		return Local::Fail(FText::Format(
			LOCTEXT("NullMeshDescription", "Convert to Nanite Static Mesh: '{0}' has no LOD0 mesh description (stripped source / Nanite-fallback-only import)."),
			FText::FromString(StaticMesh->GetName())));
	case Arc::FoliageCut::ECutPipelineResult::NoMaskResolved:
		return Local::Fail(FText::Format(
			LOCTEXT("NoMaskResolved", "Convert to Nanite Static Mesh: no opacity mask could be resolved on any material slot of '{0}', and no mask-texture override was set."),
			FText::FromString(StaticMesh->GetName())));
	case Arc::FoliageCut::ECutPipelineResult::ZeroClosedCurves:
		return Local::Fail(FText::Format(
			LOCTEXT("ZeroClosedCurves", "Convert to Nanite Static Mesh: the opacity mask(s) on '{0}' produced no closed contour curves (check WhiteLevel / channel / invert settings)."),
			FText::FromString(StaticMesh->GetName())));
	case Arc::FoliageCut::ECutPipelineResult::ZeroGeometry:
		return Local::Fail(FText::Format(
			LOCTEXT("ZeroGeometry", "Convert to Nanite Static Mesh: '{0}' produced no geometry to build."),
			FText::FromString(StaticMesh->GetName())));
	default:
		return Local::Fail(FText::Format(
			LOCTEXT("UnknownCutResult", "Convert to Nanite Static Mesh: the cut pipeline returned an unknown result for '{0}'."),
			FText::FromString(StaticMesh->GetName())));
	}

	// --- 2. Names ---------------------------------------------------------------------------------
	const FString SourcePackageName = StaticMesh->GetOutermost()->GetName();
	const FString PackagePath = FPackageName::GetLongPackagePath(SourcePackageName);
	const FString BaseName = StaticMesh->GetName();
	const FString MeshAssetName = BaseName + EffectiveSettings->AssetSuffix;
	const FString MeshPackagePath = PackagePath / MeshAssetName;

	// --- 3. Asset creation: UStaticMesh -----------------------------------------------------------
	// CreateStaticMeshAsset creates its own package from NewAssetPath when UsePackage is null, so do
	// NOT pre-create a package (unlike the skeletal path).
	UE::AssetUtils::FStaticMeshAssetOptions Options;
	Options.NewAssetPath = MeshPackagePath;
	Options.NumSourceModels = 1;
	Options.SourceMeshes.DynamicMeshes = { &CutMesh };
	Options.AssetMaterials = AssetMaterials;
	Options.NumMaterialSlots = AssetMaterials.Num();
	Options.bGenerateNaniteEnabledMesh = EffectiveSettings->bEnableNanite;
	Options.NaniteSettings.bEnabled = EffectiveSettings->bEnableNanite;
	Options.NaniteSettings.ShapePreservation = EffectiveSettings->NaniteShapePreservation;

	UE::AssetUtils::FStaticMeshResults Results;
	const UE::AssetUtils::ECreateStaticMeshResult CreateResult =
		UE::AssetUtils::CreateStaticMeshAsset(Options, Results);
	if (CreateResult != UE::AssetUtils::ECreateStaticMeshResult::Ok || Results.StaticMesh == nullptr)
	{
		return Local::Fail(FText::Format(
			LOCTEXT("AssetCreationFailed", "Convert to Nanite Static Mesh: CreateStaticMeshAsset failed for '{0}' (code {1})."),
			FText::FromString(MeshAssetName),
			FText::AsNumber(static_cast<int32>(CreateResult))));
	}

	// --- 4. Registry + dirty + optional save ------------------------------------------------------
	UPackage* MeshPackage = Results.StaticMesh->GetOutermost();
	FAssetRegistryModule::AssetCreated(Results.StaticMesh);
	MeshPackage->MarkPackageDirty();

	if (EffectiveSettings->bSavePackages)
	{
		UEditorLoadingAndSavingUtils::SavePackages({ MeshPackage }, /*bOnlyDirty=*/false);
	}

	UE_LOG(LogArcFoliageStaticConverter, Log,
		TEXT("Converted '%s' -> '%s': %d triangle(s), Nanite=%d."),
		*BaseName, *MeshAssetName, CutMesh.TriangleCount(), EffectiveSettings->bEnableNanite ? 1 : 0);

	return Results.StaticMesh;
#else  // WITH_EDITOR
	return nullptr;
#endif // WITH_EDITOR
}

#undef LOCTEXT_NAMESPACE
