// Copyright Lukasz Baran. All Rights Reserved.

#include "TracePresets/ArcTracePresetsStatusBar.h"

#include "CoreGlobals.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuMisc.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "Trace/Trace.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTracePresets, Log, All);

#define LOCTEXT_NAMESPACE "ArcTracePresetsStatusBar"

namespace ArcTracePresetsStatusBar_Local
{
	const FName MenuOwnerName(TEXT("ArcTracePresetsStatusBar"));
	const TCHAR* const PresetSectionName = TEXT("Trace.ChannelPresets");
	FDelegateHandle StartupCallbackHandle;

	struct FArcTracePreset
	{
		FString Name;
		FString ChannelList;
		bool bIsEnginePreset = false;
	};

	struct FArcTraceChannelState
	{
		FString RawName;
		bool bIsEnabled = false;
		bool bIsReadOnly = false;
	};

	FString StripChannelSuffix(const FString& InName)
	{
		FString Result = InName.TrimStartAndEnd();

		if (Result.Len() > 7)
		{
			Result.RemoveFromEnd(TEXT("Channel"), ESearchCase::CaseSensitive);
		}

		return Result;
	}

	FString NormalizeChannelName(const FString& InName)
	{
		FString Result = StripChannelSuffix(InName);

		// The channel hash drops one trailing 's', so "log", "logs" and "LogsChannel" are one channel.
		if (Result.Len() > 0 && (Result[Result.Len() - 1] == TEXT('s') || Result[Result.Len() - 1] == TEXT('S')))
		{
			Result = Result.LeftChop(1);
		}

		return Result.ToLower();
	}

	void GatherChannels(TMap<FString, FArcTraceChannelState>& OutChannels)
	{
		UE::Trace::EnumerateChannels([](const UE::Trace::FChannelInfo& Info, void* User) -> bool
		{
			TMap<FString, FArcTraceChannelState>* Channels = static_cast<TMap<FString, FArcTraceChannelState>*>(User);

			const FString RawName(StringCast<TCHAR>(Info.Name).Get());

			FArcTraceChannelState& State = Channels->FindOrAdd(NormalizeChannelName(RawName));
			State.RawName = RawName;
			State.bIsEnabled = Info.bIsEnabled;
			State.bIsReadOnly = Info.bIsReadOnly;

			return true;
		}, &OutChannels);
	}

	void GatherPresets(TArray<FArcTracePreset>& OutPresets)
	{
		FTraceAuxiliary::EnumerateFixedChannelPresets(
			[&OutPresets](const FTraceAuxiliary::FChannelPreset& Preset) -> FTraceAuxiliary::EEnumerateResult
			{
				const FString ChannelList(Preset.ChannelList);
				if (!Preset.bIsReadOnly && !ChannelList.IsEmpty())
				{
					OutPresets.Add(FArcTracePreset{ FString(Preset.Name), ChannelList, true });
				}
				return FTraceAuxiliary::EEnumerateResult::Continue;
			});

		FTraceAuxiliary::EnumerateChannelPresetsFromSettings(
			[&OutPresets](const FTraceAuxiliary::FChannelPreset& Preset) -> FTraceAuxiliary::EEnumerateResult
			{
				const FString ChannelList(Preset.ChannelList);
				if (!Preset.bIsReadOnly && !ChannelList.IsEmpty())
				{
					OutPresets.Add(FArcTracePreset{ FString(Preset.Name), ChannelList, false });
				}
				return FTraceAuxiliary::EEnumerateResult::Continue;
			});
	}

	void GatherWantedChannels(const FArcTracePreset& Preset, TSet<FString>& OutWanted)
	{
		TArray<FString> Tokens;
		Preset.ChannelList.ParseIntoArray(Tokens, TEXT(","), true);

		for (const FString& Token : Tokens)
		{
			OutWanted.Add(NormalizeChannelName(Token));
		}
	}

	bool IsPresetActive(const FArcTracePreset& Preset, const TMap<FString, FArcTraceChannelState>& Channels)
	{
		TSet<FString> WantedChannels;
		GatherWantedChannels(Preset, WantedChannels);
		if (WantedChannels.Num() == 0)
		{
			return false;
		}

		for (const FString& Wanted : WantedChannels)
		{
			const FArcTraceChannelState* State = Channels.Find(Wanted);
			if (State == nullptr)
			{
				return false;
			}

			if (!State->bIsReadOnly && !State->bIsEnabled)
			{
				return false;
			}
		}

		for (const TPair<FString, FArcTraceChannelState>& Channel : Channels)
		{
			if (Channel.Value.bIsEnabled && !Channel.Value.bIsReadOnly && !WantedChannels.Contains(Channel.Key))
			{
				return false;
			}
		}

		return true;
	}

	void ApplyPreset(const FArcTracePreset& Preset)
	{
		TSet<FString> WantedChannels;
		GatherWantedChannels(Preset, WantedChannels);

		TMap<FString, FArcTraceChannelState> Channels;
		GatherChannels(Channels);

		TArray<FString> ToDisable;
		for (const TPair<FString, FArcTraceChannelState>& Channel : Channels)
		{
			if (Channel.Value.bIsEnabled && !Channel.Value.bIsReadOnly && !WantedChannels.Contains(Channel.Key))
			{
				ToDisable.Add(Channel.Value.RawName);
			}
		}

		if (ToDisable.Num() > 0)
		{
			FTraceAuxiliary::DisableChannels(*FString::Join(ToDisable, TEXT(",")));
		}

		// Apply the channel list, never the preset name: token expansion is one level deep, so a preset named inside
		// another preset value would reach the channel lookup unresolved.
		FTraceAuxiliary::EnableChannels(*Preset.ChannelList);
	}

	FString MakePresetValueFromEnabledChannels()
	{
		TMap<FString, FArcTraceChannelState> Channels;
		GatherChannels(Channels);

		TArray<FString> Names;
		for (const TPair<FString, FArcTraceChannelState>& Channel : Channels)
		{
			if (!Channel.Value.bIsEnabled || Channel.Value.bIsReadOnly)
			{
				continue;
			}

			const FString Name = StripChannelSuffix(Channel.Value.RawName);
			if (!Name.IsEmpty())
			{
				Names.Add(Name);
			}
		}

		Names.Sort();

		return FString::Join(Names, TEXT(","));
	}

	void SplitIntoLines(const FString& Text, TArray<FString>& OutLines)
	{
		// Each element keeps its own trailing CR, so a join with LF reproduces every byte the splice did not touch.
		int32 LineStart = 0;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			if (Text[Index] == TEXT('\n'))
			{
				OutLines.Add(Text.Mid(LineStart, Index - LineStart));
				LineStart = Index + 1;
			}
		}

		OutLines.Add(Text.Mid(LineStart));
	}

	bool UsesCarriageReturns(const FString& Text)
	{
		int32 NumLineFeeds = 0;
		int32 NumCarriageReturns = 0;

		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			if (Text[Index] == TEXT('\n'))
			{
				++NumLineFeeds;
				if (Index > 0 && Text[Index - 1] == TEXT('\r'))
				{
					++NumCarriageReturns;
				}
			}
		}

		return NumLineFeeds == 0 || NumCarriageReturns * 2 >= NumLineFeeds;
	}

	void AppendLineAtEnd(TArray<FString>& Lines, const FString& Content, const FString& CarriageReturn)
	{
		if (Lines.Num() > 0 && Lines.Last().IsEmpty())
		{
			Lines.Last() = Content + CarriageReturn;
		}
		else
		{
			if (Lines.Num() > 0 && !Lines.Last().EndsWith(TEXT("\r"), ESearchCase::CaseSensitive))
			{
				Lines.Last().Append(CarriageReturn);
			}

			Lines.Add(Content + CarriageReturn);
		}

		Lines.Add(FString());
	}

	bool WritePresetToProjectConfig(const FString& Name, const FString& Value, bool bRemove)
	{
		const FString IniPath = FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini");

		if (IFileManager::Get().IsReadOnly(*IniPath))
		{
			UE_LOG(LogArcTracePresets, Warning, TEXT("Trace preset '%s' not written: '%s' is read only."), *Name, *IniPath);
			return false;
		}

		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *IniPath))
		{
			UE_LOG(LogArcTracePresets, Warning, TEXT("Trace preset '%s' not written: reading '%s' failed."), *Name, *IniPath);
			return false;
		}

		const FString CarriageReturn = UsesCarriageReturns(Text) ? TEXT("\r") : TEXT("");

		TArray<FString> Lines;
		SplitIntoLines(Text, Lines);

		const FString SectionHeader = FString::Printf(TEXT("[%s]"), PresetSectionName);

		int32 SectionStart = INDEX_NONE;
		int32 SectionEnd = Lines.Num();
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			const FString Trimmed = Lines[Index].TrimStartAndEnd();
			if (SectionStart == INDEX_NONE)
			{
				if (Trimmed.Equals(SectionHeader, ESearchCase::IgnoreCase))
				{
					SectionStart = Index;
				}
			}
			else if (Trimmed.StartsWith(TEXT("["), ESearchCase::CaseSensitive))
			{
				SectionEnd = Index;
				break;
			}
		}

		int32 MatchIndex = INDEX_NONE;
		if (SectionStart != INDEX_NONE)
		{
			for (int32 Index = SectionStart + 1; Index < SectionEnd; ++Index)
			{
				int32 EqualsIndex = INDEX_NONE;
				if (!Lines[Index].FindChar(TEXT('='), EqualsIndex))
				{
					continue;
				}

				if (Lines[Index].Left(EqualsIndex).TrimStartAndEnd().Equals(Name, ESearchCase::IgnoreCase))
				{
					MatchIndex = Index;
					break;
				}
			}
		}

		if (bRemove)
		{
			if (MatchIndex == INDEX_NONE)
			{
				UE_LOG(LogArcTracePresets, Warning,
					TEXT("Trace preset '%s' not deleted: '%s' does not declare it."), *Name, *IniPath);
				return false;
			}

			Lines.RemoveAt(MatchIndex);
		}
		else if (MatchIndex != INDEX_NONE)
		{
			Lines[MatchIndex] = Name + TEXT("=") + Value + CarriageReturn;
		}
		else if (SectionStart != INDEX_NONE)
		{
			int32 InsertIndex = SectionEnd;
			while (InsertIndex > SectionStart + 1 && Lines[InsertIndex - 1].TrimStartAndEnd().IsEmpty())
			{
				--InsertIndex;
			}

			Lines.Insert(Name + TEXT("=") + Value + CarriageReturn, InsertIndex);
		}
		else
		{
			AppendLineAtEnd(Lines, SectionHeader, CarriageReturn);
			AppendLineAtEnd(Lines, Name + TEXT("=") + Value, CarriageReturn);
		}

		// AutoDetect would rewrite the whole file as UTF-16 the moment it holds one non-ASCII character.
		if (!FFileHelper::SaveStringToFile(FString::Join(Lines, TEXT("\n")), *IniPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogArcTracePresets, Warning, TEXT("Trace preset '%s' not written: saving '%s' failed."), *Name, *IniPath);
			return false;
		}

		if (GConfig != nullptr)
		{
			FConfigFile* EngineFile = GConfig->FindConfigFile(GEngineIni);
			const bool bHasEngineFile = EngineFile != nullptr;
			const bool bWasDirty = bHasEngineFile && EngineFile->Dirty;

			if (bRemove)
			{
				GConfig->RemoveKeyFromSection(PresetSectionName, FName(*Name), GEngineIni);
			}
			else
			{
				GConfig->SetString(PresetSectionName, *Name, *Value, GEngineIni);
			}

			EngineFile = GConfig->FindConfigFile(GEngineIni);
			if (bHasEngineFile && EngineFile != nullptr)
			{
				EngineFile->Dirty = bWasDirty;
			}
		}

		return true;
	}

	bool IsValidPresetNameCharacter(TCHAR Character)
	{
		return (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('a') && Character <= TEXT('z'))
			|| (Character >= TEXT('0') && Character <= TEXT('9'))
			|| Character == TEXT('_');
	}

	void SavePreset(const FString& InName)
	{
		const FString Name = InName.TrimStartAndEnd();

		if (Name.IsEmpty())
		{
			UE_LOG(LogArcTracePresets, Warning, TEXT("Trace preset not saved: the name is empty."));
			return;
		}

		for (int32 Index = 0; Index < Name.Len(); ++Index)
		{
			if (!IsValidPresetNameCharacter(Name[Index]))
			{
				UE_LOG(LogArcTracePresets, Warning,
					TEXT("Trace preset '%s' not saved: the name may only contain letters, digits and '_'."), *Name);
				return;
			}
		}

		// FTraceAuxiliary resolves the fixed presets before the ini section, so an entry named after one is never reachable.
		const TCHAR* FixedPresetNames[] = { TEXT("Default"), TEXT("Memory"), TEXT("Memory_Light") };
		for (const TCHAR* FixedName : FixedPresetNames)
		{
			if (Name.Equals(FixedName, ESearchCase::IgnoreCase))
			{
				UE_LOG(LogArcTracePresets, Warning,
					TEXT("Trace preset '%s' not saved: a fixed engine preset already owns that name."), *Name);
				return;
			}
		}

		const FString Value = MakePresetValueFromEnabledChannels();
		if (Value.IsEmpty())
		{
			UE_LOG(LogArcTracePresets, Warning,
				TEXT("Trace preset '%s' not saved: no writable channel is currently enabled."), *Name);
			return;
		}

		if (WritePresetToProjectConfig(Name, Value, false))
		{
			UE_LOG(LogArcTracePresets, Log, TEXT("Trace preset '%s' saved as '%s'."), *Name, *Value);
		}
	}

	void DeletePreset(const FString& Name)
	{
		if (WritePresetToProjectConfig(Name, FString(), true))
		{
			UE_LOG(LogArcTracePresets, Log, TEXT("Trace preset '%s' deleted."), *Name);
		}
	}

	class SArcTracePresetsStatusBar : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SArcTracePresetsStatusBar) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			ChildSlot
			[
				SNew(SComboButton)
				.ContentPadding(FMargin(6.0f, 0.0f))
				.MenuPlacement(MenuPlacement_AboveAnchor)
				.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
				.OnGetMenuContent(this, &SArcTracePresetsStatusBar::MakeMenu)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("TracePresetsLabel", "Trace Presets"))
				]
			];
		}

	private:
		void AddPresetEntries(FMenuBuilder& MenuBuilder, const TArray<FArcTracePreset>& Presets,
			const TSharedRef<TMap<FString, FArcTraceChannelState>>& Channels, bool bEnginePresets)
		{
			for (const FArcTracePreset& Preset : Presets)
			{
				if (Preset.bIsEnginePreset != bEnginePresets)
				{
					continue;
				}

				MenuBuilder.AddMenuEntry(
					FText::FromString(Preset.Name),
					FText::FromString(Preset.ChannelList),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([Preset]()
						{
							ApplyPreset(Preset);
						}),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda([Preset, Channels]() -> bool
						{
							return IsPresetActive(Preset, *Channels);
						})),
					NAME_None,
					EUserInterfaceActionType::RadioButton);
			}
		}

		TSharedRef<SWidget> MakeMenu()
		{
			TArray<FArcTracePreset> Presets;
			GatherPresets(Presets);

			TSharedRef<TMap<FString, FArcTraceChannelState>> Channels = MakeShared<TMap<FString, FArcTraceChannelState>>();
			GatherChannels(*Channels);

			int32 NumProjectPresets = 0;
			for (const FArcTracePreset& Preset : Presets)
			{
				if (!Preset.bIsEnginePreset)
				{
					++NumProjectPresets;
				}
			}

			FMenuBuilder MenuBuilder(true, nullptr);

			MenuBuilder.BeginSection(TEXT("ArcTraceEnginePresets"), LOCTEXT("EnginePresetsSection", "Engine Presets"));
			AddPresetEntries(MenuBuilder, Presets, Channels, true);
			MenuBuilder.EndSection();

			if (NumProjectPresets > 0)
			{
				MenuBuilder.BeginSection(TEXT("ArcTraceProjectPresets"), LOCTEXT("ProjectPresetsSection", "Project Presets"));
				AddPresetEntries(MenuBuilder, Presets, Channels, false);
				MenuBuilder.EndSection();
			}

			MenuBuilder.BeginSection(TEXT("ArcTracePresetAuthoring"), LOCTEXT("AuthoringSection", "Authoring"));

			MenuBuilder.AddWidget(
				SNew(SEditableTextBox)
				.HintText(LOCTEXT("SavePresetHint", "Preset name"))
				.OnTextCommitted_Lambda([](const FText& InText, ETextCommit::Type InCommitType)
				{
					if (InCommitType == ETextCommit::OnEnter)
					{
						SavePreset(InText.ToString());
					}
				}),
				LOCTEXT("SavePresetLabel", "Save Current Channels as Preset"));

			if (NumProjectPresets > 0)
			{
				MenuBuilder.AddSubMenu(
					LOCTEXT("DeletePresetLabel", "Delete Preset"),
					LOCTEXT("DeletePresetToolTip", "Remove a project preset from DefaultEngine.ini."),
					FNewMenuDelegate::CreateLambda([Presets](FMenuBuilder& InSubMenuBuilder)
					{
						for (const FArcTracePreset& Preset : Presets)
						{
							if (Preset.bIsEnginePreset)
							{
								continue;
							}

							InSubMenuBuilder.AddMenuEntry(
								FText::FromString(Preset.Name),
								FText::FromString(Preset.ChannelList),
								FSlateIcon(),
								FUIAction(FExecuteAction::CreateLambda([Preset]()
								{
									DeletePreset(Preset.Name);
								})));
						}
					}));
			}

			MenuBuilder.EndSection();

			return MenuBuilder.MakeWidget();
		}
	};

	void ExtendStatusBar()
	{
		FToolMenuOwnerScoped OwnerScoped(MenuOwnerName);

		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.StatusBar.ToolBar"));
		if (Menu == nullptr)
		{
			return;
		}

		FToolMenuSection& Section = Menu->AddSection(TEXT("ArcTracePresets"), FText::GetEmpty(),
			FToolMenuInsert(TEXT("Insights"), EToolMenuInsertType::After));

		Section.AddDynamicEntry(TEXT("ArcTracePresets"), FNewToolMenuSectionDelegate::CreateLambda(
			[](FToolMenuSection& InSection)
			{
				InSection.AddEntry(FToolMenuEntry::InitWidget(TEXT("ArcTracePresets"),
					SNew(SArcTracePresetsStatusBar), FText::GetEmpty(), true, false));
			}));
	}
}

namespace ArcTracePresetsStatusBar
{
	void Register()
	{
		ArcTracePresetsStatusBar_Local::StartupCallbackHandle = UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateStatic(&ArcTracePresetsStatusBar_Local::ExtendStatusBar));
	}

	void Unregister()
	{
		UToolMenus::UnRegisterStartupCallback(ArcTracePresetsStatusBar_Local::StartupCallbackHandle);
		UToolMenus::UnregisterOwner(ArcTracePresetsStatusBar_Local::MenuOwnerName);
	}
}

#undef LOCTEXT_NAMESPACE
