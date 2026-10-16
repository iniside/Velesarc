// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTraceDebugger.h"

#include "ArcTraceFileName.h"

#include "imgui.h"
#include "Misc/Paths.h"
#include "Misc/StringBuilder.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Trace/Config.h"
#include "Trace/Trace.h"

namespace ArcTraceDebuggerLocal
{
	void CopyToBuffer(const FString& Source, char* Buffer, int32 BufferSize)
	{
		const FTCHARToUTF8 Converted(*Source);
		FCStringAnsi::Strncpy(Buffer, reinterpret_cast<const ANSICHAR*>(Converted.Get()), BufferSize);
	}

	const char* ToSystemStatusLabel(const FTraceAuxiliary::ETraceSystemStatus Status)
	{
		switch (Status)
		{
		case FTraceAuxiliary::ETraceSystemStatus::NotAvailable:				return "Not available";
		case FTraceAuxiliary::ETraceSystemStatus::Available:					return "Available";
		case FTraceAuxiliary::ETraceSystemStatus::TracingToServer:			return "Tracing to server";
		case FTraceAuxiliary::ETraceSystemStatus::TracingToFile:				return "Tracing to file";
		case FTraceAuxiliary::ETraceSystemStatus::TracingToSecureNetwork:	return "Tracing to secure network";
		case FTraceAuxiliary::ETraceSystemStatus::TracingToCustomRelay:		return "Tracing to custom relay";
		default:															return "?";
		}
	}

	const char* ToConnectionTypeLabel(const FTraceAuxiliary::EConnectionType Type)
	{
		switch (Type)
		{
		case FTraceAuxiliary::EConnectionType::Network:			return "Network";
		case FTraceAuxiliary::EConnectionType::File:				return "File";
		case FTraceAuxiliary::EConnectionType::Relay:			return "Relay";
		case FTraceAuxiliary::EConnectionType::SecureNetwork:	return "SecureNetwork";
		case FTraceAuxiliary::EConnectionType::None:			return "None (memory)";
		default:												return "?";
		}
	}

	FString GetActiveChannels()
	{
		TStringBuilder<2048> Builder;
		FTraceAuxiliary::GetActiveChannelsString(Builder);
		FString ActiveChannels = FString(Builder.ToView());
		ActiveChannels.RemoveFromEnd(TEXT(","));
		return ActiveChannels;
	}

	int32 CountActiveChannels(const FString& ActiveChannels)
	{
		if (ActiveChannels.IsEmpty())
		{
			return 0;
		}

		int32 Count = 1;
		for (const TCHAR Character : ActiveChannels)
		{
			if (Character == TEXT(','))
			{
				++Count;
			}
		}
		return Count;
	}

	FString DescribeActiveChannels()
	{
		const FString ActiveChannels = GetActiveChannels();
		if (ActiveChannels.IsEmpty())
		{
			return FString(TEXT("NO channels enabled - trace will record no data"));
		}
		return FString::Printf(TEXT("%d channels: %s"), CountActiveChannels(ActiveChannels), *ActiveChannels);
	}

	// UE::Trace::EnumerateChannels takes a raw function pointer, so the destination array travels through the void* User argument.
	bool CollectChannel(const UE::Trace::FChannelInfo& Info, void* User)
	{
		TArray<FArcTraceChannelEntry>* Collected = static_cast<TArray<FArcTraceChannelEntry>*>(User);
		FArcTraceChannelEntry& Entry = Collected->AddDefaulted_GetRef();
		Entry.Name = Info.Name != nullptr ? FString(ANSI_TO_TCHAR(Info.Name)) : FString();
		Entry.Description = Info.Desc != nullptr ? FString(ANSI_TO_TCHAR(Info.Desc)) : FString();
		Entry.bEnabled = Info.bIsEnabled;
		Entry.bReadOnly = Info.bIsReadOnly;
		return true;
	}

	void DrawLabelledValue(const char* Label, const FString& Value)
	{
		ImGui::TextUnformatted(Label);
		ImGui::SameLine();
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*Value));
	}
}

void FArcTraceDebugger::Initialize()
{
	ArcTraceDebuggerLocal::CopyToBuffer(
		FPaths::ConvertRelativePathToFull(FPaths::ProfilingDir()), DirectoryBuf, UE_ARRAY_COUNT(DirectoryBuf));
	ArcTraceDebuggerLocal::CopyToBuffer(TEXT("127.0.0.1"), HostBuf, UE_ARRAY_COUNT(HostBuf));
	ChannelFilterBuf[0] = 0;

	PendingFileName = ArcTraceFileName::MakeTraceFileName(TEXT(""));
	ChannelRefreshTimer = 0.0f;

	RefreshChannels();
	RefreshPresets();
}

void FArcTraceDebugger::Uninitialize()
{
	Channels.Empty();
	Presets.Empty();
	LastStatus.Empty();
}

void FArcTraceDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(900, 640), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Trace / Insights", &bShow))
	{
		ImGui::End();
		return;
	}

#if UE_TRACE_ENABLED
	const bool bConnectedNow = FTraceAuxiliary::IsConnected();
	if (bWasConnected && !bConnectedNow)
	{
		PendingFileName = ArcTraceFileName::MakeTraceFileName(TEXT(""));
	}
	bWasConnected = bConnectedNow;

	ChannelRefreshTimer += ImGui::GetIO().DeltaTime;
	if (ChannelRefreshTimer >= 1.0f)
	{
		ChannelRefreshTimer = 0.0f;
		RefreshChannels();
	}

	DrawStatus();
	ImGui::Separator();
	DrawControls();
	ImGui::Separator();
	DrawTarget();
	ImGui::Separator();
	DrawPresets();
	ImGui::Separator();
	DrawChannels();

	ImGui::Separator();
	ImGui::TextUnformatted("Last action:");
	ImGui::SameLine();
	if (LastStatus.IsEmpty())
	{
		ImGui::TextDisabled("none");
	}
	else
	{
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*LastStatus));
	}
#else
	ImGui::TextDisabled("Trace disabled at compile time (UE_TRACE_ENABLED=0).");
#endif

	ImGui::End();
}

void FArcTraceDebugger::DrawStatus()
{
	const FTraceAuxiliary::ETraceSystemStatus SystemStatus = FTraceAuxiliary::GetTraceSystemStatus();
	const bool bConnected = FTraceAuxiliary::IsConnected();
	const bool bPaused = FTraceAuxiliary::IsPaused();

	const ImVec4 StatusColor = bConnected && !bPaused
		? ImVec4(0.40f, 0.90f, 0.40f, 1.0f)
		: ImVec4(0.65f, 0.65f, 0.65f, 1.0f);

	ImGui::TextColored(StatusColor, "%s%s",
		ArcTraceDebuggerLocal::ToSystemStatusLabel(SystemStatus), bPaused ? " (paused)" : "");

	ImGui::Text("Connected: %s   Connection: %s",
		bConnected ? "yes" : "no",
		ArcTraceDebuggerLocal::ToConnectionTypeLabel(FTraceAuxiliary::GetConnectionType()));

	const FString Destination = FTraceAuxiliary::GetTraceDestinationString();
	ArcTraceDebuggerLocal::DrawLabelledValue("Destination:", Destination.IsEmpty() ? FString(TEXT("<none>")) : Destination);

	const FString ActiveChannels = ArcTraceDebuggerLocal::GetActiveChannels();
	ImGui::TextUnformatted("Active channels:");
	ImGui::SameLine();
	if (ActiveChannels.IsEmpty())
	{
		ImGui::TextDisabled("<none>");
	}
	else
	{
		const FString ShortChannels = ActiveChannels.Len() > 140 ? ActiveChannels.Left(140) + TEXT(" ...") : ActiveChannels;
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*ShortChannels));
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(600.0f);
			ImGui::TextUnformatted(TCHAR_TO_UTF8(*ActiveChannels));
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}
	}
}

void FArcTraceDebugger::DrawControls()
{
	const bool bConnected = FTraceAuxiliary::IsConnected();
	const bool bPaused = FTraceAuxiliary::IsPaused();

	ImGui::BeginDisabled(bConnected);
	if (ImGui::Button("Start"))
	{
		StartTrace();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!bConnected);
	if (ImGui::Button("Stop"))
	{
		const bool bStopped = FTraceAuxiliary::Stop();
		LastStatus = bStopped ? TEXT("Stopped trace") : TEXT("Stop failed");
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!bConnected);
	if (bPaused)
	{
		if (ImGui::Button("Resume"))
		{
			FTraceAuxiliary::Resume();
			LastStatus = TEXT("Resumed trace");
			RefreshChannels();
		}
	}
	else
	{
		if (ImGui::Button("Pause"))
		{
			FTraceAuxiliary::Pause();
			LastStatus = TEXT("Paused trace");
			RefreshChannels();
		}
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Write Snapshot"))
	{
		WriteSnapshotToFile();
	}

	ImGui::SameLine();
	if (ImGui::Button("Send Snapshot"))
	{
		SendSnapshotToHost();
	}
}

void FArcTraceDebugger::DrawTarget()
{
	ImGui::TextUnformatted("Target");

	int32 ModeIndex = static_cast<int32>(TargetMode);
	ImGui::RadioButton("File", &ModeIndex, static_cast<int32>(EArcTraceTargetMode::File));
	ImGui::SameLine();
	ImGui::RadioButton("Network", &ModeIndex, static_cast<int32>(EArcTraceTargetMode::Network));
	ImGui::SameLine();
	ImGui::RadioButton("None (memory)", &ModeIndex, static_cast<int32>(EArcTraceTargetMode::None));
	TargetMode = static_cast<EArcTraceTargetMode>(ModeIndex);

	if (TargetMode == EArcTraceTargetMode::File)
	{
		ImGui::SetNextItemWidth(640.0f);
		ImGui::InputText("Directory", DirectoryBuf, IM_ARRAYSIZE(DirectoryBuf));

		ImGui::Text("File name: %s", TCHAR_TO_UTF8(*PendingFileName));

		const FString Destination = FTraceAuxiliary::GetTraceDestinationString();
		const bool bTracingToFile = FTraceAuxiliary::IsConnected()
			&& FTraceAuxiliary::GetConnectionType() == FTraceAuxiliary::EConnectionType::File;
		const FString DisplayPath = bTracingToFile && !Destination.IsEmpty()
			? Destination
			: BuildFullPath(PendingFileName);

		ImGui::Text("Full path: %s", TCHAR_TO_UTF8(*DisplayPath));
		ImGui::SameLine();
		if (ImGui::Button("Copy Path"))
		{
			ImGui::SetClipboardText(TCHAR_TO_UTF8(*DisplayPath));
			LastStatus = FString::Printf(TEXT("Copied path %s"), *DisplayPath);
		}
	}
	else if (TargetMode == EArcTraceTargetMode::Network)
	{
		ImGui::SetNextItemWidth(320.0f);
		ImGui::InputText("Host", HostBuf, IM_ARRAYSIZE(HostBuf));
	}

	ImGui::Checkbox("Truncate file", &bTruncateFile);
	ImGui::SameLine();
	ImGui::Checkbox("Exclude tail", &bExcludeTail);
}

void FArcTraceDebugger::DrawPresets()
{
	ImGui::Text("Channel presets (%d)", Presets.Num());
	ImGui::SameLine();
	if (ImGui::Button("Refresh##Presets"))
	{
		RefreshPresets();
	}

	const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
		| ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
	if (!ImGui::BeginTable("ArcTracePresets", 4, TableFlags, ImVec2(0.0f, 150.0f)))
	{
		return;
	}

	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 180.0f);
	ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthFixed, 60.0f);
	ImGui::TableSetupColumn("Channels", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 160.0f);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (int32 PresetIndex = 0; PresetIndex < Presets.Num(); ++PresetIndex)
	{
		const FArcTracePresetEntry& Preset = Presets[PresetIndex];

		ImGui::TableNextRow();
		ImGui::PushID(PresetIndex);

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*Preset.Name));

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(Preset.bFromSettings ? "ini" : "code");

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*Preset.ChannelList));
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(600.0f);
			ImGui::TextUnformatted(TCHAR_TO_UTF8(*Preset.ChannelList));
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		ImGui::TableNextColumn();
		ImGui::BeginDisabled(Preset.bReadOnly);
		if (ImGui::Button("Apply"))
		{
			FTraceAuxiliary::EnableChannels(*Preset.ChannelList);
			LastStatus = FString::Printf(TEXT("Applied preset %s"), *Preset.Name);
			RefreshChannels();
		}
		const bool bApplyHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);

		ImGui::SameLine();
		if (ImGui::Button("Set Only"))
		{
			FTraceAuxiliary::DisableChannels(nullptr);
			FTraceAuxiliary::EnableChannels(*Preset.ChannelList);
			LastStatus = FString::Printf(TEXT("Set channels to preset %s"), *Preset.Name);
			RefreshChannels();
		}
		const bool bSetOnlyHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
		ImGui::EndDisabled();

		if (Preset.bReadOnly && (bApplyHovered || bSetOnlyHovered))
		{
			ImGui::SetTooltip("read-only preset, command line only");
		}

		ImGui::PopID();
	}

	ImGui::EndTable();
}

void FArcTraceDebugger::DrawChannels()
{
	ImGui::Text("Channels (%d)", Channels.Num());
	ImGui::SameLine();
	if (ImGui::Button("Refresh##Channels"))
	{
		RefreshChannels();
	}

	ImGui::SetNextItemWidth(320.0f);
	ImGui::InputText("Filter", ChannelFilterBuf, IM_ARRAYSIZE(ChannelFilterBuf));

	const FString Filter = FString(UTF8_TO_TCHAR(ChannelFilterBuf));

	if (ImGui::Button("Enable All Visible"))
	{
		for (FArcTraceChannelEntry& Entry : Channels)
		{
			if (!Entry.bReadOnly && MatchesFilter(Entry, Filter))
			{
				UE::Trace::ToggleChannel(*Entry.Name, true);
			}
		}
		LastStatus = TEXT("Enabled visible channels");
		RefreshChannels();
	}

	ImGui::SameLine();
	if (ImGui::Button("Disable All Visible"))
	{
		for (FArcTraceChannelEntry& Entry : Channels)
		{
			if (!Entry.bReadOnly && MatchesFilter(Entry, Filter))
			{
				UE::Trace::ToggleChannel(*Entry.Name, false);
			}
		}
		LastStatus = TEXT("Disabled visible channels");
		RefreshChannels();
	}

	ImGui::SameLine();
	if (ImGui::Button("Disable All"))
	{
		FTraceAuxiliary::DisableChannels(nullptr);
		LastStatus = TEXT("Disabled all channels");
		RefreshChannels();
	}

	const ImGuiTableFlags TableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg
		| ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
	if (!ImGui::BeginTable("ArcTraceChannels", 4, TableFlags, ImVec2(0.0f, 260.0f)))
	{
		return;
	}

	ImGui::TableSetupColumn("On", ImGuiTableColumnFlags_WidthFixed, 32.0f);
	ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 220.0f);
	ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableSetupColumn("Read-only", ImGuiTableColumnFlags_WidthFixed, 70.0f);
	ImGui::TableSetupScrollFreeze(0, 1);
	ImGui::TableHeadersRow();

	for (int32 ChannelIndex = 0; ChannelIndex < Channels.Num(); ++ChannelIndex)
	{
		FArcTraceChannelEntry& Entry = Channels[ChannelIndex];
		if (!MatchesFilter(Entry, Filter))
		{
			continue;
		}

		ImGui::TableNextRow();
		ImGui::PushID(ChannelIndex);

		ImGui::TableNextColumn();
		bool bEnabled = Entry.bEnabled;
		ImGui::BeginDisabled(Entry.bReadOnly);
		if (ImGui::Checkbox("##Enabled", &bEnabled))
		{
			Entry.bEnabled = UE::Trace::ToggleChannel(*Entry.Name, bEnabled);
			LastStatus = FString::Printf(TEXT("Channel %s -> %s"), *Entry.Name, Entry.bEnabled ? TEXT("on") : TEXT("off"));
		}
		ImGui::EndDisabled();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*Entry.Name));

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(TCHAR_TO_UTF8(*Entry.Description));

		ImGui::TableNextColumn();
		ImGui::TextUnformatted(Entry.bReadOnly ? "yes" : "");

		ImGui::PopID();
	}

	ImGui::EndTable();
}

bool FArcTraceDebugger::MatchesFilter(const FArcTraceChannelEntry& Entry, const FString& Filter)
{
	return Filter.IsEmpty() || Entry.Name.Contains(Filter) || Entry.Description.Contains(Filter);
}

void FArcTraceDebugger::RefreshChannels()
{
	Channels.Reset();
	UE::Trace::EnumerateChannels(&ArcTraceDebuggerLocal::CollectChannel, &Channels);
	Channels.Sort([](const FArcTraceChannelEntry& Lhs, const FArcTraceChannelEntry& Rhs) -> bool
	{
		return Lhs.Name < Rhs.Name;
	});
}

void FArcTraceDebugger::RefreshPresets()
{
	Presets.Reset();

	TArray<FArcTracePresetEntry>* Collected = &Presets;

	// FChannelPreset exposes borrowed TCHAR pointers the header forbids storing, so both fields are copied into FString here.
	FTraceAuxiliary::EnumerateFixedChannelPresets(
		[Collected](const FTraceAuxiliary::FChannelPreset& Preset) -> FTraceAuxiliary::EEnumerateResult
		{
			FArcTracePresetEntry& Entry = Collected->AddDefaulted_GetRef();
			Entry.Name = Preset.Name != nullptr ? FString(Preset.Name) : FString();
			Entry.ChannelList = Preset.ChannelList != nullptr ? FString(Preset.ChannelList) : FString();
			Entry.bReadOnly = Preset.bIsReadOnly;
			Entry.bFromSettings = false;
			return FTraceAuxiliary::EEnumerateResult::Continue;
		});

	FTraceAuxiliary::EnumerateChannelPresetsFromSettings(
		[Collected](const FTraceAuxiliary::FChannelPreset& Preset) -> FTraceAuxiliary::EEnumerateResult
		{
			FArcTracePresetEntry& Entry = Collected->AddDefaulted_GetRef();
			Entry.Name = Preset.Name != nullptr ? FString(Preset.Name) : FString();
			Entry.ChannelList = Preset.ChannelList != nullptr ? FString(Preset.ChannelList) : FString();
			Entry.bReadOnly = Preset.bIsReadOnly;
			Entry.bFromSettings = true;
			return FTraceAuxiliary::EEnumerateResult::Continue;
		});
}

FString FArcTraceDebugger::GetTargetDirectory() const
{
	FString Directory = FString(UTF8_TO_TCHAR(DirectoryBuf)).TrimStartAndEnd();
	if (Directory.IsEmpty())
	{
		Directory = FPaths::ConvertRelativePathToFull(FPaths::ProfilingDir());
	}
	return Directory;
}

FString FArcTraceDebugger::BuildFullPath(const FString& FileName) const
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(GetTargetDirectory(), FileName));
}

void FArcTraceDebugger::StartTrace()
{
	FTraceAuxiliary::FOptions Options;
	Options.bTruncateFile = bTruncateFile;
	Options.bExcludeTail = bExcludeTail;

	const FString ChannelSummary = ArcTraceDebuggerLocal::DescribeActiveChannels();

	switch (TargetMode)
	{
	case EArcTraceTargetMode::File:
	{
		const FString FullPath = BuildFullPath(PendingFileName);
		const bool bStarted = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, *FullPath, nullptr, &Options);
		LastStatus = FString::Printf(TEXT("Started trace -> %s (%s) | %s"),
			*FullPath, bStarted ? TEXT("ok") : TEXT("failed"), *ChannelSummary);
		break;
	}
	case EArcTraceTargetMode::Network:
	{
		const FString Host = FString(UTF8_TO_TCHAR(HostBuf)).TrimStartAndEnd();
		const bool bStarted = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::Network, *Host, nullptr, &Options);
		LastStatus = FString::Printf(TEXT("Started trace -> %s (%s) | %s"),
			*Host, bStarted ? TEXT("ok") : TEXT("failed"), *ChannelSummary);
		break;
	}
	case EArcTraceTargetMode::None:
	{
		const bool bStarted = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::None, nullptr, nullptr, &Options);
		LastStatus = FString::Printf(TEXT("Started trace -> memory (%s) | %s"),
			bStarted ? TEXT("ok") : TEXT("failed"), *ChannelSummary);
		break;
	}
	}

	RefreshChannels();
}

void FArcTraceDebugger::WriteSnapshotToFile()
{
	const FString FullPath = BuildFullPath(ArcTraceFileName::MakeTraceFileName(TEXT("-snapshot")));
	const bool bWritten = FTraceAuxiliary::WriteSnapshot(*FullPath);
	LastStatus = FString::Printf(TEXT("Snapshot -> %s (%s)"), *FullPath, bWritten ? TEXT("ok") : TEXT("failed"));
}

void FArcTraceDebugger::SendSnapshotToHost()
{
	const FString Host = FString(UTF8_TO_TCHAR(HostBuf)).TrimStartAndEnd();
	const bool bSent = FTraceAuxiliary::SendSnapshot(Host.IsEmpty() ? nullptr : *Host, 0, 0);
	LastStatus = FString::Printf(TEXT("Snapshot sent to %s (%s)"),
		Host.IsEmpty() ? TEXT("<default>") : *Host, bSent ? TEXT("ok") : TEXT("failed"));
}
