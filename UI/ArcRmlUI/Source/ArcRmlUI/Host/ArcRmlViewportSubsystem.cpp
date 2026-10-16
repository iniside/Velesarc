// Copyright Lukasz Baran. All Rights Reserved.

#include "Host/ArcRmlViewportSubsystem.h"

#include "ArcRmlUILog.h"
#include "ArcRmlUIModule.h"
#include "ArcRmlUIPerf.h"
#include "ArcRmlUISettings.h"
#include "ArcRmlUITags.h"
#include "CommonInputSettings.h"
#include "Descriptors/ArcSettingDescriptor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Host/ArcRmlActivatableWidget.h"
#include "Host/ArcRmlContentRect.h"
#include "Host/SArcRmlViewport.h"
#include "ICommonInputModule.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Misc/Paths.h"
#include "Model/ArcSettingsModel.h"
#include "MVVM/ArcRmlModelBinding.h"
#include "MVVM/ArcRmlViewModelDeclarations.h"
#include "PrimaryGameLayout.h"
#include "Thread/ArcRmlUIThread.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

namespace ArcRmlViewportSubsystemPrivate
{
	UArcRmlViewportSubsystem* FindSubsystem(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		ULocalPlayer* LocalPlayer = World->GetFirstLocalPlayerFromController();
		if (!LocalPlayer && GEngine)
		{
			LocalPlayer = GEngine->GetFirstGamePlayer(World);
		}
		return LocalPlayer ? LocalPlayer->GetSubsystem<UArcRmlViewportSubsystem>() : nullptr;
	}

	void HandleOpenCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOGF(LogArcRmlUI, Warning, "Usage: ArcRml.Open <Path>");
			return;
		}

		UArcRmlViewportSubsystem* Subsystem = FindSubsystem(World);
		if (!Subsystem)
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRml.Open: no local player in this world");
			return;
		}

		Subsystem->OpenDocument(FArcRmlDocumentRef{Args[0]});
	}

	TArray<FKey> QueryMappedKeys(ULocalPlayer& LocalPlayer, const UInputAction* Action)
	{
		const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer.GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
		if (!Action || !InputSubsystem)
		{
			return TArray<FKey>();
		}
		return InputSubsystem->QueryKeysMappedToAction(Action);
	}

	void HandleCloseCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOGF(LogArcRmlUI, Warning, "Usage: ArcRml.Close <Path>");
			return;
		}

		if (UArcRmlViewportSubsystem* Subsystem = FindSubsystem(World))
		{
			Subsystem->CloseDocument(FArcRmlDocumentRef{Args[0]});
		}
	}

	FAutoConsoleCommandWithWorldAndArgs OpenCommand(
		TEXT("ArcRml.Open"),
		TEXT("Opens an RML document on its layer for the first local player: ArcRml.Open <Path>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleOpenCommand));

	void HandlePreviewCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOGF(LogArcRmlUI, Warning, "Usage: ArcRml.Preview <Path> [Variant]");
			return;
		}

		UArcRmlViewportSubsystem* Subsystem = FindSubsystem(World);
		if (!Subsystem)
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRml.Preview: no local player in this world");
			return;
		}

		Subsystem->PreviewDocument(FArcRmlDocumentRef{Args[0]}, Args.Num() > 1 ? Args[1] : FString());
	}

	FAutoConsoleCommandWithWorldAndArgs PreviewCommand(
		TEXT("ArcRml.Preview"),
		TEXT("Opens an RML document with a preview variant, reopening it if it is open: ArcRml.Preview <Path> [Variant]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandlePreviewCommand));

	FAutoConsoleCommandWithWorldAndArgs CloseCommand(
		TEXT("ArcRml.Close"),
		TEXT("Closes an RML document opened with ArcRml.Open: ArcRml.Close <Path>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleCloseCommand));

#if WITH_EDITOR
	void HandleDumpExpandedCommand(const TArray<FString>& Args)
	{
		if (Args.Num() < 1)
		{
			UE_LOGF(LogArcRmlUI, Warning, "Usage: ArcRml.DumpExpanded <Path>");
			return;
		}

		const TSharedPtr<FArcRmlUIThread> UIThread = FArcRmlUIModule::Get().GetOrStartUIThread();
		if (UIThread.IsValid())
		{
			UIThread->DumpExpandedDocument(Args[0]);
		}
	}

	FAutoConsoleCommand DumpExpandedCommand(
		TEXT("ArcRml.DumpExpanded"),
		TEXT("Writes an RML document after component expansion to Saved/ArcRml/<Path>: ArcRml.DumpExpanded <Path>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&HandleDumpExpandedCommand));
#endif

#if !UE_BUILD_SHIPPING
	void HandlePreviewSetCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOGF(LogArcRmlUI, Warning, "Usage: ArcRml.PreviewSet <Path> <Variant>");
			return;
		}

		UArcRmlViewportSubsystem* Subsystem = FindSubsystem(World);
		if (!Subsystem)
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRml.PreviewSet: no local player in this world");
			return;
		}

		Subsystem->SetPreviewVariant(FArcRmlDocumentRef{Args[0]}, Args[1]);
	}

	FAutoConsoleCommandWithWorldAndArgs PreviewSetCommand(
		TEXT("ArcRml.PreviewSet"),
		TEXT("Swaps the preview variant of an open RML document without reopening it: ArcRml.PreviewSet <Path> <Variant>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandlePreviewSetCommand));

	void HandleClickCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 2)
		{
			UE_LOGF(LogArcRmlUI, Warning, "Usage: ArcRml.Click <Path> <Selector> [Index]");
			return;
		}

		UArcRmlViewportSubsystem* Subsystem = FindSubsystem(World);
		if (!Subsystem)
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRml.Click: no local player in this world");
			return;
		}

		Subsystem->ClickElement(FArcRmlDocumentRef{Args[0]}, Args[1], Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 0);
	}

	FAutoConsoleCommandWithWorldAndArgs ClickCommand(
		TEXT("ArcRml.Click"),
		TEXT("Sends a synthetic left click to the centre of the Index-th visible element matching Selector in an open RML document: ArcRml.Click <Path> <Selector> [Index]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleClickCommand));

	void HandleLayoutDumpCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOGF(LogArcRmlUI, Warning, "Usage: ArcRml.LayoutDump <Path> [OutDir]");
			return;
		}

		UArcRmlViewportSubsystem* Subsystem = FindSubsystem(World);
		if (!Subsystem)
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRml.LayoutDump: no local player in this world");
			return;
		}

		const FString OutDir = Args.Num() > 1 ? Args[1] : FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ArcRml/LayoutDump"));
		Subsystem->DumpLayout(FArcRmlDocumentRef{Args[0]}, FPaths::ConvertRelativePathToFull(OutDir));
	}

	FAutoConsoleCommandWithWorldAndArgs LayoutDumpCommand(
		TEXT("ArcRml.LayoutDump"),
		TEXT("After the next update of an open RML document, writes its layout (<Name>.layout.json) and its live DOM as HTML for Chrome (<Name>.html) to OutDir, default Saved/ArcRml/LayoutDump: ArcRml.LayoutDump <Path> [OutDir]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&HandleLayoutDumpCommand));
#endif
}

void UArcRmlViewportSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &UArcRmlViewportSubsystem::HandleWorldCleanup);
	UArcSettingsModel* Model = GetLocalPlayer()->GetGameInstance()->GetSubsystem<UArcSettingsModel>();
	check(Model);
	SettingsModel = Model;
}

void UArcRmlViewportSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	TArray<FString> Paths;
	OpenViews.GetKeys(Paths);
	for (const FString& Path : Paths)
	{
		CloseDocumentImmediately(Path);
	}
	Super::Deinitialize();
}

bool UArcRmlViewportSubsystem::OpenDocument(const FArcRmlDocumentRef& Document, const UInputAction* CloseAction)
{
	return OpenView(ArcRmlPaths::NormalizePackagePath(Document.Path), CloseAction, FString());
}

bool UArcRmlViewportSubsystem::PreviewDocument(const FArcRmlDocumentRef& Document, const FString& Variant)
{
	const FString Path = ArcRmlPaths::NormalizePackagePath(Document.Path);
	const FString ViewPath = ResolveViewPath(Path);
	const UInputAction* CloseAction = nullptr;
	if (const FOpenView* Existing = OpenViews.Find(ViewPath))
	{
		CloseAction = Existing->CloseAction.Get();
		CloseDocumentImmediately(ViewPath);
	}
	return OpenView(Path, CloseAction, Variant);
}

#if !UE_BUILD_SHIPPING
void UArcRmlViewportSubsystem::SetPreviewVariant(const FArcRmlDocumentRef& Document, const FString& Variant)
{
	const FString Path = ArcRmlPaths::NormalizePackagePath(Document.Path);
	const FString ViewPath = ResolveViewPath(Path);
	FOpenView* View = OpenViews.Find(ViewPath);
	const bool bTab = ViewPath != Path;
	const TSharedPtr<FArcRmlModelBinding> Binding = View ? (bTab ? View->TabBinding : View->Binding) : TSharedPtr<FArcRmlModelBinding>();
	if (!Binding.IsValid())
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRml.PreviewSet: '%ls' is not open", *Path);
		return;
	}

	if (!Binding->SetPreviewVariant(Variant))
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRml.PreviewSet: '%ls' has no preview model with variant '%ls'", *Path, *Variant);
		return;
	}

	if (bTab)
	{
		View->EmbeddedPreviewVariant = Variant;
	}
	else
	{
		View->PreviewVariant = Variant;
	}
	UE_LOGF(LogArcRmlUI, Log, "ArcRml.PreviewSet: '%ls' preview variant set to '%ls'", *Path, *Variant);
}

void UArcRmlViewportSubsystem::ClickElement(const FArcRmlDocumentRef& Document, const FString& Selector, int32 Index)
{
	const FString Path = ArcRmlPaths::NormalizePackagePath(Document.Path);
	const FOpenView* View = OpenViews.Find(ResolveViewPath(Path));
	if (!View || !View->Viewport.IsValid())
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRml.Click: '%ls' is not open", *Path);
		return;
	}

	const TSharedPtr<FArcRmlUIThread> UIThread = FArcRmlUIModule::Get().GetOrStartUIThread();
	if (UIThread.IsValid())
	{
		UIThread->ClickElement(View->Viewport->GetViewId(), Selector, Index);
	}
}

void UArcRmlViewportSubsystem::DumpLayout(const FArcRmlDocumentRef& Document, const FString& OutDir)
{
	const FString Path = ArcRmlPaths::NormalizePackagePath(Document.Path);
	const FOpenView* View = OpenViews.Find(ResolveViewPath(Path));
	if (!View || !View->Viewport.IsValid())
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRml.LayoutDump: '%ls' is not open", *Path);
		return;
	}

	const TSharedPtr<FArcRmlUIThread> UIThread = FArcRmlUIModule::Get().GetOrStartUIThread();
	if (UIThread.IsValid())
	{
		UIThread->DumpLayout(View->Viewport->GetViewId(), OutDir);
	}
}
#endif

bool UArcRmlViewportSubsystem::OpenView(const FString& Path, const UInputAction* CloseAction, const FString& PreviewVariant)
{
	const double OpenSeconds = FPlatformTime::Seconds();
	if (Path.IsEmpty())
	{
		return false;
	}

	if (FOpenView* Existing = OpenViews.Find(Path))
	{
		if (Existing->bClosing)
		{
			CancelLeave(Path, *Existing);
			return true;
		}
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: open '%ls' ignored, it is already open", *Path);
		return true;
	}

	const FString EmbeddingFramePath = FindFramePath(Path);
	if (!EmbeddingFramePath.IsEmpty())
	{
		FOpenView& Frame = OpenViews.FindChecked(EmbeddingFramePath);
		if (Frame.bClosing)
		{
			CancelLeave(EmbeddingFramePath, Frame);
			return true;
		}
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: open '%ls' ignored, it is already embedded in '%ls'", *Path, *EmbeddingFramePath);
		return true;
	}

	ARCRMLUI_PERF(const uint64 HeadStartCycles = FPlatformTime::Cycles64());
	TOptional<FArcRmlDocumentHead> Head = ArcRmlViewModelDeclarations::ReadDocumentHead(Path);
	if (!Head.IsSet())
	{
		return false;
	}
#if !UE_BUILD_SHIPPING
	if (ArcRmlUIPerf::IsEnabled())
	{
		UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: '%ls' head %.2f ms", *Path, ArcRmlUIPerf::SinceMs(HeadStartCycles));
	}
#endif

	if (!Head->Meta.Frame.IsEmpty())
	{
		return OpenTab(Path, MoveTemp(*Head), CloseAction, PreviewVariant, OpenSeconds);
	}
	return CreateView(Path, MoveTemp(*Head), CloseAction, PreviewVariant, OpenSeconds) != nullptr;
}

bool UArcRmlViewportSubsystem::OpenTab(const FString& TabPath, FArcRmlDocumentHead&& TabHead, const UInputAction* CloseAction, const FString& PreviewVariant,
	double OpenSeconds)
{
	const FString FramePath = TabHead.Meta.Frame;
	FOpenView* Frame = OpenViews.Find(FramePath);
	if (Frame)
	{
		for (const FArcRmlViewModelDeclaration& Declaration : TabHead.Declarations)
		{
			if (Frame->Binding->DeclaresModel(Declaration.Name))
			{
				UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot open '%ls': view model '%ls' is also declared by its frame '%ls'", *TabPath, *Declaration.Name, *FramePath);
				return false;
			}
		}
		if (Frame->bClosing)
		{
			CancelLeave(FramePath, *Frame);
		}
		return SwapEmbed(FramePath, *Frame, TabPath, MoveTemp(TabHead.Declarations), PreviewVariant);
	}

	TOptional<FArcRmlDocumentHead> FrameHead = ArcRmlViewModelDeclarations::ReadFrameHead(TabPath, TabHead);
	if (!FrameHead.IsSet())
	{
		return false;
	}

	Frame = CreateView(FramePath, MoveTemp(*FrameHead), CloseAction, FString(), OpenSeconds);
	if (!Frame)
	{
		return false;
	}
	return SwapEmbed(FramePath, *Frame, TabPath, MoveTemp(TabHead.Declarations), PreviewVariant);
}

bool UArcRmlViewportSubsystem::SwapEmbed(const FString& FramePath, FOpenView& View, const FString& TabPath, TArray<FArcRmlViewModelDeclaration>&& TabDeclarations,
	const FString& PreviewVariant)
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UWorld* World = View.World.Get();
	const TSharedPtr<FArcRmlUIThread> UIThread = FArcRmlUIModule::Get().GetOrStartUIThread();
	if (!LocalPlayer || !World || !UIThread.IsValid() || !View.Viewport.IsValid())
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot embed '%ls' into '%ls': the frame view has no local player world", *TabPath, *FramePath);
		return false;
	}

	const uint32 ViewId = View.Viewport->GetViewId();
	const FString PreviousPath = View.EmbeddedPath;
	if (View.TabBinding.IsValid())
	{
		UIThread->ClearEmbed(ViewId);
		View.TabBinding->Shutdown();
		View.TabBinding.Reset();
	}

	View.TabBinding = MakeShared<FArcRmlModelBinding>(UIThread.ToSharedRef(), ViewId, LocalPlayer, World, MoveTemp(TabDeclarations), PreviewVariant, TabPath);
	View.TabBinding->SetOnDocumentAction(FArcRmlOnDocumentAction::CreateWeakLambda(this, [this, FramePath](EArcRmlDocumentAction Action, const FString& Target)
	{
		HandleDocumentAction(FramePath, Action, Target);
	}));
	View.TabBinding->Start();
	View.TabBinding->SetPaused(View.bHidden);
	UIThread->EmbedDocument(ViewId, TabPath);
	View.EmbeddedPath = TabPath;
	View.EmbeddedPreviewVariant = PreviewVariant;
	View.Binding->SetEmbeddedPath(TabPath);
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: '%ls' view %u embeds '%ls' (was '%ls')", *FramePath, ViewId, *TabPath, *PreviousPath);
	return true;
}

UArcRmlViewportSubsystem::FOpenView* UArcRmlViewportSubsystem::CreateView(const FString& Path, FArcRmlDocumentHead&& Head, const UInputAction* CloseAction,
	const FString& PreviewVariant, double OpenSeconds)
{
	ARCRMLUI_PERF(const uint64 OpenStartCycles = FPlatformTime::Cycles64());
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
	if (!LocalPlayer || !World)
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: cannot open '%ls' without a local player world", *Path);
		return nullptr;
	}

	UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(LocalPlayer);
	if (!Layout)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot open '%ls': the local player has no primary game layout", *Path);
		return nullptr;
	}

	UCommonActivatableWidgetContainerBase* Layer = Layout->GetLayerWidget(Head.Meta.Layer);
	if (!Layer)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot open '%ls': layer '%ls' is not registered in the primary game layout", *Path,
			*Head.Meta.Layer.ToString());
		return nullptr;
	}

	const TSharedPtr<FArcRmlUIThread> UIThread = FArcRmlUIModule::Get().GetOrStartUIThread();
	if (!UIThread.IsValid())
	{
		return nullptr;
	}

	TArray<FKey> BackKeys;
	TArray<FKey> CloseKeys;
	if (Head.Meta.InputMode != EArcRmlInputMode::Game)
	{
		BackKeys = ArcRmlViewportSubsystemPrivate::QueryMappedKeys(*LocalPlayer, ICommonInputModule::GetSettings().GetEnhancedInputBackAction());
		CloseKeys = ArcRmlViewportSubsystemPrivate::QueryMappedKeys(*LocalPlayer, CloseAction);
		if (BackKeys.IsEmpty())
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: '%ls': no keys are mapped to the CommonUI Enhanced Input back action", *Path);
		}
	}

	const FString BackKeyNames = FString::JoinBy(BackKeys, TEXT(", "), [](const FKey& Key) { return Key.ToString(); });
	const FString CloseKeyNames = FString::JoinBy(CloseKeys, TEXT(", "), [](const FKey& Key) { return Key.ToString(); });

	const float MaxContentAspect = ReadMaxContentAspect();
	FOpenView View;
	View.Viewport = SNew(SArcRmlViewport, UIThread.ToSharedRef(), LocalPlayer);
	View.World = World;
	View.CloseAction = CloseAction;
	View.PreviewVariant = PreviewVariant;
	View.Meta = Head.Meta;
	View.OpenOrder = NextOpenOrder++;
	View.Binding = MakeShared<FArcRmlModelBinding>(UIThread.ToSharedRef(), View.Viewport->GetViewId(), LocalPlayer, World,
		MoveTemp(Head.Declarations), PreviewVariant, Path);
	View.Binding->SetEmbeddedPath(FString());
	ARCRMLUI_PERF(const uint64 BindingStartCycles = FPlatformTime::Cycles64());
	View.Binding->Start();
	ARCRMLUI_PERF(const double BindingStartMs = ArcRmlUIPerf::SinceMs(BindingStartCycles));
	View.Binding->SetOnDocumentAction(FArcRmlOnDocumentAction::CreateWeakLambda(this, [this, Path](EArcRmlDocumentAction Action, const FString& Target)
	{
		HandleDocumentAction(Path, Action, Target);
	}));
	View.Viewport->SetOnViewModelCall(FArcRmlOnViewModelCall::CreateWeakLambda(this, [this, Path](const FArcRmlViewModelCall& Call)
	{
		DispatchViewModelCall(Path, Call);
	}));
	View.Viewport->SetKeyPolicy(View.Meta.InputMode, MoveTemp(BackKeys), MoveTemp(CloseKeys), FSimpleDelegate::CreateWeakLambda(this, [this, Path]()
	{
		CloseDocument(FArcRmlDocumentRef{Path});
	}));
	View.Viewport->SetOnRootTransitionEnd(FSimpleDelegate::CreateWeakLambda(this, [this, Path]()
	{
		const FString PathToFinish = Path;
		FinishLeave(PathToFinish, false);
	}));
	View.Viewport->SetRootClass(TEXT("enter"), true);
	const FGeometry& LayoutGeometry = Layout->GetCachedGeometry();
	const FVector2f LayoutPosition = LayoutGeometry.GetAbsolutePosition();
	const FSlateRect ContentRect = ArcRmlContentRect::Compute(FSlateRect(LayoutPosition, LayoutPosition + LayoutGeometry.GetAbsoluteSize()),
		ArcRmlContentRect::GetSafeMargin(), MaxContentAspect);
	const FVector2f ContentSize = ContentRect.GetSize2f();
	View.Viewport->LoadDocument(Path, FIntPoint(FMath::RoundToInt(ContentSize.X), FMath::RoundToInt(ContentSize.Y)), LayoutGeometry.Scale, OpenSeconds);

	const TSharedRef<SArcRmlViewport> Viewport = View.Viewport.ToSharedRef();
	View.Widget = Layer->AddWidget<UArcRmlActivatableWidget>(UArcRmlActivatableWidget::StaticClass(),
		[this, &Path, &View, &Viewport](UArcRmlActivatableWidget& Widget)
		{
			Widget.InitDocument(*this, FArcRmlDocumentRef{Path}, View.Meta, Viewport);
		});
	if (!View.Widget.IsValid())
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: cannot open '%ls': pushing it onto layer '%ls' failed", *Path, *View.Meta.Layer.ToString());
		DestroyView(View);
		return nullptr;
	}

#if !UE_BUILD_SHIPPING
	if (ArcRmlUIPerf::IsEnabled())
	{
		UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' open (game thread): binding start %.2f ms, total %.2f ms",
			View.Viewport->GetViewId(), *Path, BindingStartMs, ArcRmlUIPerf::SinceMs(OpenStartCycles));
	}
#endif
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: opened '%ls' view %u layer '%ls' input-mode %ls hide-lower %ls back keys [%ls] close keys [%ls] activated %ls",
		*Path, View.Viewport->GetViewId(), *View.Meta.Layer.ToString(), View.Meta.InputMode == EArcRmlInputMode::Game ? TEXT("game") : View.Meta.InputMode == EArcRmlInputMode::Menu ? TEXT("menu") : TEXT("all"),
		View.Meta.bHideLower ? TEXT("true") : TEXT("false"), *BackKeyNames, *CloseKeyNames,
		View.Widget->IsActivated() ? TEXT("true") : TEXT("false"));
	FOpenView& Added = OpenViews.Add(Path, MoveTemp(View));
	UpdateVisibility();
	return &Added;
}

FString UArcRmlViewportSubsystem::FindFramePath(const FString& TabPath) const
{
	for (const TPair<FString, FOpenView>& Pair : OpenViews)
	{
		if (!Pair.Value.EmbeddedPath.IsEmpty() && Pair.Value.EmbeddedPath == TabPath)
		{
			return Pair.Key;
		}
	}
	return FString();
}

FString UArcRmlViewportSubsystem::ResolveViewPath(const FString& Path) const
{
	return OpenViews.Contains(Path) ? Path : FindFramePath(Path);
}

void UArcRmlViewportSubsystem::DispatchViewModelCall(const FString& ViewPath, const FArcRmlViewModelCall& Call)
{
	const FOpenView* View = OpenViews.Find(ViewPath);
	if (!View)
	{
		return;
	}

	const TSharedPtr<FArcRmlModelBinding> Target = View->TabBinding.IsValid() && View->TabBinding->DeclaresModel(Call.ModelName) ? View->TabBinding : View->Binding;
	if (Target.IsValid())
	{
		Target->HandleViewModelCall(Call);
	}
}

void UArcRmlViewportSubsystem::HandleDocumentAction(const FString& ViewPath, EArcRmlDocumentAction Action, const FString& Target)
{
	if (Action == EArcRmlDocumentAction::Open)
	{
		const FOpenView* View = OpenViews.Find(ViewPath);
		OpenDocument(FArcRmlDocumentRef{Target}, View ? View->CloseAction.Get() : nullptr);
	}
	else
	{
		CloseDocument(FArcRmlDocumentRef{ViewPath});
	}
}

void UArcRmlViewportSubsystem::CloseDocument(const FArcRmlDocumentRef& Document)
{
	const FString Path = ArcRmlPaths::NormalizePackagePath(Document.Path);
	const FString ViewPath = ResolveViewPath(Path);
	FOpenView* View = OpenViews.Find(ViewPath);
	if (!View)
	{
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: close '%ls' ignored, it is not open", *Path);
		return;
	}

	if (View->bClosing)
	{
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: close '%ls' ignored, it is already closing", *ViewPath);
		return;
	}

	if (View->bHidden || !View->Widget.IsValid() || !View->Viewport.IsValid())
	{
		CloseDocumentImmediately(ViewPath);
		return;
	}

	StartLeave(ViewPath, *View);
}

void UArcRmlViewportSubsystem::StartLeave(const FString& Path, FOpenView& View)
{
	const float TimeoutSeconds = GetDefault<UArcRmlUISettings>()->LeaveTimeoutSeconds;
	View.bClosing = true;
	View.LeaveStartSeconds = FPlatformTime::Seconds();
	View.Viewport->SetRootClass(TEXT("enter"), false);
	View.Viewport->SetRootClass(TEXT("leave"), true);
	View.Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	View.LeaveTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, Path](float DeltaTime)
	{
		FinishLeave(Path, true);
		return false;
	}), TimeoutSeconds);
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: '%ls' leave started (timeout %.2f s)", *Path, TimeoutSeconds);
	UpdateVisibility();
}

void UArcRmlViewportSubsystem::CancelLeave(const FString& Path, FOpenView& View)
{
	FTSTicker::RemoveTicker(View.LeaveTimeoutHandle);
	View.LeaveTimeoutHandle.Reset();
	View.bClosing = false;
	View.Viewport->SetRootClass(TEXT("leave"), false);
	View.Viewport->SetRootClass(TEXT("enter"), true);
	if (UArcRmlActivatableWidget* Widget = View.Widget.Get(); Widget && !View.bHidden)
	{
		Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: '%ls' leave cancelled after %.0f ms, reopened", *Path,
		(FPlatformTime::Seconds() - View.LeaveStartSeconds) * 1000.0);
	UpdateVisibility();
}

void UArcRmlViewportSubsystem::FinishLeave(const FString& Path, bool bTimedOut)
{
	const FOpenView* View = OpenViews.Find(Path);
	if (!View || !View->bClosing)
	{
		return;
	}

	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: '%ls' leave finished by %ls after %.0f ms", *Path, bTimedOut ? TEXT("timeout") : TEXT("root transition end"),
		(FPlatformTime::Seconds() - View->LeaveStartSeconds) * 1000.0);
	CloseDocumentImmediately(Path);
}

void UArcRmlViewportSubsystem::CloseDocumentImmediately(const FString& Path)
{
	FOpenView View;
	if (!OpenViews.RemoveAndCopyValue(Path, View))
	{
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: close '%ls' ignored, it is not open", *Path);
		return;
	}
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: closing '%ls'", *Path);

	if (UArcRmlActivatableWidget* Widget = View.Widget.Get())
	{
		if (UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(GetLocalPlayer()))
		{
			Layout->FindAndRemoveWidgetFromLayer(Widget);
		}
	}

	DestroyView(View);
	UpdateVisibility();
}

bool UArcRmlViewportSubsystem::IsDocumentOpen(const FArcRmlDocumentRef& Document) const
{
	const FOpenView* View = OpenViews.Find(ResolveViewPath(ArcRmlPaths::NormalizePackagePath(Document.Path)));
	return View && !View->bClosing;
}

void UArcRmlViewportSubsystem::NotifyActivationChanged(const UArcRmlActivatableWidget& Widget, const FArcRmlDocumentRef& Document, bool bActivated)
{
	const FString& Path = Document.Path;
	const FOpenView* View = OpenViews.Find(Path);
	if (!View || View->Widget.Get() != &Widget)
	{
		return;
	}

	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: '%ls' %ls", *Path, bActivated ? TEXT("activated") : TEXT("deactivated"));
	UpdateVisibility();
}

void UArcRmlViewportSubsystem::UpdateVisibility()
{
	const TArray<FGameplayTag>& LayerOrder = GetDefault<UArcRmlUISettings>()->LayerOrder;
	int32 HighestHidingLayer = INDEX_NONE;
	for (const TPair<FString, FOpenView>& Pair : OpenViews)
	{
		if (Pair.Value.Meta.bHideLower && !Pair.Value.bClosing)
		{
			HighestHidingLayer = FMath::Max(HighestHidingLayer, LayerOrder.IndexOfByKey(Pair.Value.Meta.Layer));
		}
	}

	for (TPair<FString, FOpenView>& Pair : OpenViews)
	{
		const UArcRmlActivatableWidget* Widget = Pair.Value.Widget.Get();
		const bool bBuried = !Widget || !Widget->IsActivated();
		const bool bCovered = LayerOrder.IndexOfByKey(Pair.Value.Meta.Layer) < HighestHidingLayer;
		SetViewHidden(Pair.Key, Pair.Value, bBuried || bCovered);
	}
}

void UArcRmlViewportSubsystem::SetViewHidden(const FString& Path, FOpenView& View, bool bHidden)
{
	if (View.bHidden == bHidden)
	{
		return;
	}

	View.bHidden = bHidden;
	if (UArcRmlActivatableWidget* Widget = View.Widget.Get())
	{
		Widget->SetVisibility(bHidden ? ESlateVisibility::Collapsed : View.bClosing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::SelfHitTestInvisible);
	}
	if (View.Binding.IsValid())
	{
		View.Binding->SetPaused(bHidden);
	}
	if (View.TabBinding.IsValid())
	{
		View.TabBinding->SetPaused(bHidden);
	}
	if (View.Viewport.IsValid())
	{
		View.Viewport->SetViewPaused(bHidden);
	}
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: '%ls' %ls", *Path, bHidden ? TEXT("hidden") : TEXT("shown"));
}

#if WITH_EDITOR
void UArcRmlViewportSubsystem::CompileAndReloadOpenDocuments(TArray<FString>&& ChangedPackagePaths)
{
	check(IsInGameThread());

	const TSharedPtr<FArcRmlUIThread> UIThread = FArcRmlUIModule::Get().GetUIThread();
	if (!UIThread.IsValid())
	{
		return;
	}

	if (!ChangedPackagePaths.IsEmpty())
	{
		UIThread->CompileChanged(MoveTemp(ChangedPackagePaths));
	}
	UIThread->ClearDocumentCaches();
	FArcRmlUIModule::Get().OnDocumentCachesCleared().Broadcast();

	TArray<UArcRmlViewportSubsystem*> Subsystems;
	for (TObjectIterator<UArcRmlViewportSubsystem> It; It; ++It)
	{
		if (It->OpenViews.Num() > 0)
		{
			Subsystems.Add(*It);
		}
	}

	if (Subsystems.IsEmpty())
	{
		return;
	}

	struct FReloadEntry
	{
		UArcRmlViewportSubsystem* Subsystem = nullptr;
		FString ViewPath;
		FString OpenPath;
		FString PreviewVariant;
		TWeakObjectPtr<const UInputAction> CloseAction;
		int32 LayerIndex = INDEX_NONE;
		uint64 OpenOrder = 0;
		bool bClosing = false;
	};

	const TArray<FGameplayTag>& LayerOrder = GetDefault<UArcRmlUISettings>()->LayerOrder;
	TArray<FReloadEntry> Entries;
	for (UArcRmlViewportSubsystem* Subsystem : Subsystems)
	{
		for (const TPair<FString, FOpenView>& Pair : Subsystem->OpenViews)
		{
			FReloadEntry& Entry = Entries.AddDefaulted_GetRef();
			Entry.Subsystem = Subsystem;
			const bool bEmbeds = !Pair.Value.EmbeddedPath.IsEmpty();
			Entry.ViewPath = Pair.Key;
			Entry.OpenPath = bEmbeds ? Pair.Value.EmbeddedPath : Pair.Key;
			Entry.PreviewVariant = bEmbeds ? Pair.Value.EmbeddedPreviewVariant : Pair.Value.PreviewVariant;
			Entry.CloseAction = Pair.Value.CloseAction;
			Entry.LayerIndex = LayerOrder.IndexOfByKey(Pair.Value.Meta.Layer);
			Entry.OpenOrder = Pair.Value.OpenOrder;
			Entry.bClosing = Pair.Value.bClosing;
		}
	}

	Entries.Sort([](const FReloadEntry& A, const FReloadEntry& B)
	{
		if (A.LayerIndex != B.LayerIndex)
		{
			return A.LayerIndex < B.LayerIndex;
		}
		return A.OpenOrder < B.OpenOrder;
	});

	for (const FReloadEntry& Entry : Entries)
	{
		Entry.Subsystem->CloseDocumentImmediately(Entry.ViewPath);
	}

	for (const FReloadEntry& Entry : Entries)
	{
		if (Entry.bClosing)
		{
			continue;
		}
		Entry.Subsystem->OpenView(Entry.OpenPath, Entry.CloseAction.Get(), Entry.PreviewVariant);
	}
}
#endif

void UArcRmlViewportSubsystem::DestroyView(const FOpenView& View)
{
	FTSTicker::RemoveTicker(View.LeaveTimeoutHandle);

	if (View.TabBinding.IsValid())
	{
		View.TabBinding->Shutdown();
	}

	if (View.Binding.IsValid())
	{
		View.Binding->Shutdown();
	}

	if (View.Viewport.IsValid())
	{
		View.Viewport->ReleaseView();
	}
}

void UArcRmlViewportSubsystem::HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	TArray<FString> ToClose;
	for (const TPair<FString, FOpenView>& Pair : OpenViews)
	{
		if (!Pair.Value.World.IsValid() || Pair.Value.World.Get() == World)
		{
			ToClose.Add(Pair.Key);
		}
	}

	for (const FString& Path : ToClose)
	{
		CloseDocumentImmediately(Path);
	}
}

float UArcRmlViewportSubsystem::ReadMaxContentAspect() const
{
	const UArcSettingsModel* Model = SettingsModel.Get();
	check(Model);
	const FGameplayTag Tag = ArcRmlUITags::Settings_Video_Discrete_MaxUIAspectRatio;
	const FInstancedStruct* Descriptor = Model->GetDescriptor(Tag);
	const FArcDiscreteSettingDescriptor* Discrete = Descriptor ? Descriptor->GetPtr<FArcDiscreteSettingDescriptor>() : nullptr;
	checkf(Discrete, TEXT("ArcRmlUI: setting '%s' is not registered as a discrete setting"), *Tag.ToString());
	const int32 Index = Model->GetDiscreteIndex(Tag);
	checkf(Discrete->Options.IsValidIndex(Index), TEXT("ArcRmlUI: setting '%s' has index %d, outside its %d options"), *Tag.ToString(), Index, Discrete->Options.Num());
	return FCString::Atof(*Discrete->Options[Index].Value);
}
