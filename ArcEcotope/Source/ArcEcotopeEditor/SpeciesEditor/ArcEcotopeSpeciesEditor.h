// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"

class SArcEcotopeBandDetails;
class SArcEcotopeBandList;
class SArcEcotopeBandPreviewViewport;
class SDockTab;
class UArcEcotopeSpeciesModel;

struct FArcEcotopeBandSelection
{
	int32 GetBandIndex() const { return BandIndex; }

	void SetBandIndex(int32 InIndex)
	{
		BandIndex = InIndex;
		OnChanged.Broadcast();
	}

	// The only channel between the three panels: the band details customization is built through an
	// argument-less FOnGetDetailCustomizationInstance and cannot be handed the index any other way.
	FSimpleMulticastDelegate OnChanged;

private:
	int32 BandIndex = INDEX_NONE;
};

class FArcEcotopeSpeciesEditor
	: public FAssetEditorToolkit
	, public FEditorUndoClient
{
public:
	static const FName BandListTabId;
	static const FName ViewportTabId;
	static const FName BandDetailsTabId;
	static const FName ToolkitFName;
	static const FName AppIdentifier;

	static TSharedRef<FArcEcotopeSpeciesEditor> CreateEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UArcEcotopeSpeciesModel* SpeciesToEdit);

	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	// FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	FArcEcotopeSpeciesEditor();

	void RefreshAfterUndoRedo();

	void InitSpeciesEditor(
		const EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UArcEcotopeSpeciesModel* SpeciesToEdit);

	TSharedRef<SDockTab> SpawnTab_BandList(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_BandDetails(const FSpawnTabArgs& Args);

	TSharedRef<FArcEcotopeBandSelection> Selection;
	TSharedPtr<SArcEcotopeBandList> BandList;
	TSharedPtr<SArcEcotopeBandPreviewViewport> PreviewViewport;
	TSharedPtr<SArcEcotopeBandDetails> BandDetails;
};
