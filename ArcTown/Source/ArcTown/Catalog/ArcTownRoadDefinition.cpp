// Copyright Lukasz Baran. All Rights Reserved.

#include "Catalog/ArcTownRoadDefinition.h"

#if WITH_EDITOR

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/StaticMesh.h"

#define LOCTEXT_NAMESPACE "ArcTownRoadDefinition"

namespace ArcTownRoadDefinitionPrivate
{
	void AddError(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	}

	void ValidateMeshEntry(
		const FArcTownRoadMeshEntry& Entry,
		const FString& Label,
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		bool bRequireNanite)
	{
		if (Entry.Mesh.IsNull())
		{
			AddError(Context, Result, FText::Format(
				LOCTEXT("MeshEmpty", "{0} must name a mesh."),
				FText::FromString(Label)));
			return;
		}

		IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
		const FSoftObjectPath ResolvedPath = AssetRegistry.GetRedirectedObjectPath(Entry.Mesh.ToSoftObjectPath());

		FAssetData MeshAsset;
		const UE::AssetRegistry::EExists ExistsResult = AssetRegistry.TryGetAssetByObjectPath(ResolvedPath, MeshAsset);
		if (ExistsResult == UE::AssetRegistry::EExists::DoesNotExist)
		{
			AddError(Context, Result, FText::Format(
				LOCTEXT("MeshMissing", "{0} ({1}) does not resolve to an asset in the registry."),
				FText::FromString(Label),
				FText::FromString(Entry.Mesh.ToString())));
			return;
		}

		if (ExistsResult == UE::AssetRegistry::EExists::Exists && bRequireNanite)
		{
			bool bNaniteEnabled = false;
			if (MeshAsset.GetTagValue(TEXT("NaniteEnabled"), bNaniteEnabled) && !bNaniteEnabled)
			{
				AddError(Context, Result, FText::Format(
					LOCTEXT("MeshNoNanite", "{0} ({1}) does not have Nanite enabled; the road emitter rejects non-Nanite segment meshes, so this road class will emit no road segments at all."),
					FText::FromString(Label),
					FText::FromString(MeshAsset.AssetName.ToString())));
			}
		}
	}
}

EDataValidationResult UArcTownRoadDefinition::IsDataValid(FDataValidationContext& Context) const
{
	using namespace ArcTownRoadDefinitionPrivate;

	EDataValidationResult Result = Super::IsDataValid(Context);

	if (SegmentMeshes.IsEmpty())
	{
		AddError(Context, Result, LOCTEXT("SegmentMeshesEmpty", "SegmentMeshes must not be empty; a road class with no run mesh builds nothing."));
	}

	if (SurfaceChannel.IsNone())
	{
		AddError(Context, Result, LOCTEXT("SurfaceChannelEmpty", "SurfaceChannel must name a terrain weight channel; a road with no channel carves a trench nobody can see."));
	}

	for (int32 Index = 0; Index < SegmentMeshes.Num(); ++Index)
	{
		ValidateMeshEntry(SegmentMeshes[Index], FString::Printf(TEXT("SegmentMeshes[%d]"), Index), Context, Result, /*bRequireNanite=*/true);
	}

	for (int32 Valence = 0; Valence < JunctionMeshesByValence.Num(); ++Valence)
	{
		const FArcTownRoadMeshEntry& Entry = JunctionMeshesByValence[Valence];
		if (Entry.Mesh.IsNull())
		{
			continue;
		}

		if (Valence == 0 || Valence == 2)
		{
			AddError(Context, Result, FText::Format(
				LOCTEXT("JunctionValenceUnreachable", "JunctionMeshesByValence[{0}] can never be placed: valence 0 is not a node, and a valence 2 vertex is carried through as a bend instead of being cut into a junction."),
				FText::AsNumber(Valence)));
			continue;
		}

		ValidateMeshEntry(Entry, FString::Printf(TEXT("JunctionMeshesByValence[%d]"), Valence), Context, Result, /*bRequireNanite=*/false);
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

#endif
