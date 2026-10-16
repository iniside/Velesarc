// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcLevelBrowserDebugger.h"

#include "imgui.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFeatureData.h"
#include "GameFeaturesSubsystem.h"
#include "GameFeatureTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace ArcLevelBrowserDebuggerLocal
{
	bool IsProjectLevelPackage(const FString& PackageName)
	{
		if (!FPackageName::IsValidLongPackageName(PackageName))
		{
			return false;
		}

		if (PackageName.StartsWith(TEXT("/Engine/"))
			|| PackageName.StartsWith(TEXT("/Temp/"))
			|| PackageName.StartsWith(TEXT("/Script/"))
			|| PackageName.Contains(TEXT("UEDPIE")))
		{
			return false;
		}

		FString Filename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename))
		{
			return false;
		}

		// Keeps /Game plus every mounted project-plugin content root, drops engine plugin roots.
		const FString FullPath = FPaths::ConvertRelativePathToFull(Filename);
		const FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		return FullPath.StartsWith(ProjectPath);
	}

	const char* ToTargetStateLabel(const EGameFeatureTargetState TargetState)
	{
		switch (TargetState)
		{
		case EGameFeatureTargetState::Installed:	return "Installed";
		case EGameFeatureTargetState::Registered:	return "Registered";
		case EGameFeatureTargetState::Loaded:		return "Loaded";
		case EGameFeatureTargetState::Active:		return "Active";
		default:									return "?";
		}
	}

	bool IsAtTargetState(const EGameFeaturePluginState CurrentState, const EGameFeatureTargetState TargetState)
	{
		switch (TargetState)
		{
		case EGameFeatureTargetState::Installed:	return CurrentState == EGameFeaturePluginState::Installed;
		case EGameFeatureTargetState::Registered:	return CurrentState == EGameFeaturePluginState::Registered;
		case EGameFeatureTargetState::Loaded:		return CurrentState == EGameFeaturePluginState::Loaded;
		case EGameFeatureTargetState::Active:		return CurrentState == EGameFeaturePluginState::Active;
		default:									return false;
		}
	}

	void DrawMappedAssets(const FString& PluginURL)
	{
		UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();

		const UGameFeatureData* Data = Subsystem.GetGameFeatureDataForActivePluginByURL(PluginURL);
		if (Data == nullptr)
		{
			Data = Subsystem.GetGameFeatureDataForRegisteredPluginByURL(PluginURL);
		}

		if (Data == nullptr)
		{
			ImGui::TextDisabled("No GameFeatureData loaded (state < Registered).");
			return;
		}

		const TArray<FPrimaryAssetTypeInfo>& TypesToScan = Data->GetPrimaryAssetTypesToScan();
		ImGui::Text("Primary asset types to scan: %d", TypesToScan.Num());

		for (const FPrimaryAssetTypeInfo& TypeInfo : TypesToScan)
		{
			ImGui::BulletText("%s (base class %s)%s%s",
				TCHAR_TO_UTF8(*TypeInfo.PrimaryAssetType.ToString()),
				TCHAR_TO_UTF8(*TypeInfo.GetAssetBaseClass().ToString()),
				TypeInfo.bHasBlueprintClasses ? " [blueprint classes]" : "",
				TypeInfo.bIsEditorOnly ? " [editor only]" : "");

			ImGui::Indent();
			for (const FDirectoryPath& Directory : TypeInfo.GetDirectories())
			{
				ImGui::TextDisabled("dir: %s", TCHAR_TO_UTF8(*Directory.Path));
			}
			for (const FSoftObjectPath& SpecificAsset : TypeInfo.GetSpecificAssets())
			{
				ImGui::TextDisabled("asset: %s", TCHAR_TO_UTF8(*SpecificAsset.ToString()));
			}
			for (const FString& ScanPath : TypeInfo.AssetScanPaths)
			{
				ImGui::TextDisabled("scan: %s", TCHAR_TO_UTF8(*ScanPath));
			}
			ImGui::TextDisabled("tracked assets: %d", TypeInfo.NumberOfAssets);
			ImGui::Unindent();
		}

		const TArray<UGameFeatureAction*>& Actions = Data->GetActions();
		ImGui::Text("Actions: %d", Actions.Num());

		for (const UGameFeatureAction* Action : Actions)
		{
			if (Action != nullptr)
			{
				ImGui::BulletText("%s", TCHAR_TO_UTF8(*Action->GetClass()->GetName()));
			}
			else
			{
				ImGui::BulletText("<null action>");
			}
		}
	}
}

void FArcLevelBrowserDebugger::Initialize()
{
	RefreshLevels();
	SelectedLevelIndex = -1;
}

void FArcLevelBrowserDebugger::Uninitialize()
{
	CachedLevels.Empty();
	CachedLevelNames.Empty();
	CachedPackageNames.Empty();
	SelectedLevelIndex = -1;
}

void FArcLevelBrowserDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Levels", &bShow);

	if (ImGui::BeginTabBar("ArcLevelBrowserTabs"))
	{
		if (ImGui::BeginTabItem("Levels"))
		{
			const float PanelWidth = ImGui::GetContentRegionAvail().x;
			const float LeftWidth = PanelWidth * 0.30f;

			if (ImGui::BeginChild("LevelList", ImVec2(LeftWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
			{
				DrawLevelList();
			}
			ImGui::EndChild();

			ImGui::SameLine();

			if (ImGui::BeginChild("LevelDetail", ImVec2(0, 0), ImGuiChildFlags_Borders))
			{
				if (CachedPackageNames.IsValidIndex(SelectedLevelIndex))
				{
					DrawLevelDetail();
				}
				else
				{
					ImGui::TextDisabled("Select a level from the list.");
				}
			}
			ImGui::EndChild();

			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("Game Features"))
		{
			DrawGameFeatures();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	ImGui::End();
}

void FArcLevelBrowserDebugger::DrawLevelList()
{
	if (ImGui::Button("Refresh"))
	{
		RefreshLevels();
		SelectedLevelIndex = -1;
	}

	ImGui::SameLine();
	ImGui::Text("%d levels", CachedLevels.Num());

	ImGui::InputText("##LevelFilter", FilterBuf, IM_ARRAYSIZE(FilterBuf));

	const FString Filter = FString(UTF8_TO_TCHAR(FilterBuf)).ToLower();

	for (int32 i = 0; i < CachedLevelNames.Num(); ++i)
	{
		if (!Filter.IsEmpty()
			&& !CachedLevelNames[i].ToLower().Contains(Filter)
			&& !CachedPackageNames[i].ToLower().Contains(Filter))
		{
			continue;
		}

		ImGui::PushID(i);
		const bool bSelected = (SelectedLevelIndex == i);
		if (ImGui::Selectable(TCHAR_TO_UTF8(*CachedLevelNames[i]), bSelected))
		{
			SelectedLevelIndex = i;
		}
		ImGui::PopID();
	}
}

void FArcLevelBrowserDebugger::DrawLevelDetail()
{
	const FString& LevelName = CachedLevelNames[SelectedLevelIndex];
	const FString& PackageName = CachedPackageNames[SelectedLevelIndex];

	ImGui::Text("Level: %s", TCHAR_TO_UTF8(*LevelName));
	ImGui::Text("Package: %s", TCHAR_TO_UTF8(*PackageName));
	ImGui::Separator();

	UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	if (World)
	{
		if (ImGui::Button("Open Level"))
		{
			UGameplayStatics::OpenLevel(World, FName(*PackageName), true);
		}
	}
	else
	{
		ImGui::TextDisabled("No play world.");
	}

	ImGui::SameLine();

	if (ImGui::Button("Copy Path"))
	{
		ImGui::SetClipboardText(TCHAR_TO_UTF8(*PackageName));
	}
}

void FArcLevelBrowserDebugger::DrawGameFeatures()
{
	UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();

	TArray<FGameFeatureInfo> Features;
	Subsystem.ForEachGameFeature([&Features](FGameFeatureInfo&& Info)
	{
		Features.Add(MoveTemp(Info));
	});

	Features.Sort([](const FGameFeatureInfo& A, const FGameFeatureInfo& B)
	{
		return A.Name.Compare(B.Name, ESearchCase::IgnoreCase) < 0;
	});

	if (LastGameFeatureStatus.IsEmpty())
	{
		ImGui::TextDisabled("No state change requested yet.");
	}
	else
	{
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*LastGameFeatureStatus));
	}

	ImGui::Separator();

	const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
	if (!ImGui::BeginTable("ArcGameFeaturesTable", 4, TableFlags))
	{
		return;
	}

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 170.0f);
	ImGui::TableSetupColumn("Built-in", ImGuiTableColumnFlags_WidthFixed, 60.0f);
	ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 300.0f);
	ImGui::TableHeadersRow();

	for (int32 FeatureIndex = 0; FeatureIndex < Features.Num(); ++FeatureIndex)
	{
		const FGameFeatureInfo& Feature = Features[FeatureIndex];

		ImGui::TableNextRow();
		ImGui::PushID(FeatureIndex);

		ImGui::TableNextColumn();
		if (ImGui::TreeNode("FeatureRow", "%s", TCHAR_TO_UTF8(*Feature.Name)))
		{
			ArcLevelBrowserDebuggerLocal::DrawMappedAssets(Feature.URL);
			ImGui::TreePop();
		}

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*UE::GameFeatures::ToString(Feature.CurrentState)));

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(Feature.bLoadedAsBuiltIn ? "yes" : "no");

		ImGui::TableNextColumn();
		for (int32 TargetIndex = 0; TargetIndex < static_cast<int32>(EGameFeatureTargetState::Count); ++TargetIndex)
		{
			const EGameFeatureTargetState TargetState = static_cast<EGameFeatureTargetState>(TargetIndex);
			const char* TargetLabel = ArcLevelBrowserDebuggerLocal::ToTargetStateLabel(TargetState);

			if (TargetIndex > 0)
			{
				ImGui::SameLine();
			}

			ImGui::BeginDisabled(ArcLevelBrowserDebuggerLocal::IsAtTargetState(Feature.CurrentState, TargetState));
			if (ImGui::Button(TargetLabel))
			{
				const FString PluginURL = Feature.URL;
				const FString FeatureName = Feature.Name;
				const FString TargetName = ANSI_TO_TCHAR(TargetLabel);
				TWeakPtr<uint8> WeakToken = AliveToken;

				Subsystem.ChangeGameFeatureTargetState(PluginURL, TargetState,
					FGameFeaturePluginChangeStateComplete::CreateLambda(
						[this, WeakToken, FeatureName, TargetName](const UE::GameFeatures::FResult& Result)
						{
							if (!WeakToken.Pin().IsValid())
							{
								return;
							}

							if (Result.HasError())
							{
								LastGameFeatureStatus = FString::Printf(TEXT("%s -> %s failed: %s"), *FeatureName, *TargetName, *Result.GetError());
								return;
							}

							LastGameFeatureStatus = FString::Printf(TEXT("%s -> %s"), *FeatureName, *TargetName);
							RefreshLevels();
							SelectedLevelIndex = -1;
						}));
			}
			ImGui::EndDisabled();
		}

		ImGui::PopID();
	}

	ImGui::EndTable();
}

void FArcLevelBrowserDebugger::RefreshLevels()
{
	CachedLevels.Empty();
	CachedLevelNames.Empty();
	CachedPackageNames.Empty();

	const IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

	FARFilter Filter;
	Filter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> FoundAssets;
	AssetRegistry.GetAssets(Filter, FoundAssets);

	CachedLevels.Reserve(FoundAssets.Num());
	for (const FAssetData& Asset : FoundAssets)
	{
		if (Asset.HasAnyPackageFlags(PKG_ContainsMap) && ArcLevelBrowserDebuggerLocal::IsProjectLevelPackage(Asset.PackageName.ToString()))
		{
			CachedLevels.Add(Asset);
		}
	}

	CachedLevels.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.PackageName.LexicalLess(B.PackageName);
	});

	CachedLevelNames.Reserve(CachedLevels.Num());
	CachedPackageNames.Reserve(CachedLevels.Num());
	for (const FAssetData& Asset : CachedLevels)
	{
		CachedLevelNames.Add(Asset.AssetName.ToString());
		CachedPackageNames.Add(Asset.PackageName.ToString());
	}
}
