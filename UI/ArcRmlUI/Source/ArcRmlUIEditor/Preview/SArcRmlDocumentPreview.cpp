// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/SArcRmlDocumentPreview.h"

#include "ArcRmlUILog.h"
#include "ArcRmlUIModule.h"
#include "Engine/UserInterfaceSettings.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformProcess.h"
#include "Host/ArcRmlContentRect.h"
#include "Host/ArcRmlUISettingsSubsystem.h"
#include "Host/ArcRmlViewportSubsystem.h"
#include "Host/SArcRmlViewport.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Modules/ModuleManager.h"
#include "MVVM/ArcRmlModelBinding.h"
#include "MVVM/ArcRmlPreviewData.h"
#include "MVVM/ArcRmlViewModelDeclarations.h"
#include "MVVMViewModelBase.h"
#include "Preview/ArcRmlPreviewFields.h"
#include "Preview/SArcRmlPreviewBackground.h"
#include "Styling/AppStyle.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcRmlDocumentPreview"

namespace ArcRmlDocumentPreviewPrivate
{
	constexpr int32 ResolutionCount = 3;
	const FIntPoint Resolutions[ResolutionCount] = { FIntPoint(1920, 1080), FIntPoint(2560, 1440), FIntPoint(3440, 1440) };
	const TCHAR* const RmlUIModuleName = TEXT("ArcRmlUI");
	const TCHAR* const EnterRootClass = TEXT("enter");

	FText MakeResolutionText(const FIntPoint& Resolution)
	{
		return FText::Format(LOCTEXT("ResolutionFormat", "{0} x {1}"), FText::AsNumber(Resolution.X, &FNumberFormattingOptions::DefaultNoGrouping()),
			FText::AsNumber(Resolution.Y, &FNumberFormattingOptions::DefaultNoGrouping()));
	}

	FText MakeVariantText(const FString& Variant)
	{
		return Variant.IsEmpty() ? LOCTEXT("BaseVariant", "Base") : FText::FromString(Variant);
	}

	UClass* LoadPreviewClass(const FArcRmlViewModelDeclaration& Declaration)
	{
		UClass* ViewModelClass = FSoftClassPath(Declaration.ClassPath).TryLoadClass<UObject>();
		if (!ViewModelClass || !ViewModelClass->IsChildOf(UMVVMViewModelBase::StaticClass()) || ViewModelClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return nullptr;
		}
		return ViewModelClass;
	}

	TSharedRef<SWidget> MakeLabel(const FText& Text)
	{
		return SNew(STextBlock).Text(Text).ColorAndOpacity(FSlateColor::UseSubduedForeground());
	}
}

SArcRmlDocumentPreview::~SArcRmlDocumentPreview()
{
	Shutdown();
}

void SArcRmlDocumentPreview::Construct(const FArguments& InArgs, const FString& InDocumentPath)
{
	DocumentPath = InDocumentPath;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				ArcRmlDocumentPreviewPrivate::MakeLabel(LOCTEXT("Resolution", "Resolution"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &SArcRmlDocumentPreview::MakeResolutionMenu)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() -> FText
					{
						return ArcRmlDocumentPreviewPrivate::MakeResolutionText(GetResolution());
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				ArcRmlDocumentPreviewPrivate::MakeLabel(LOCTEXT("Variant", "Variant"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &SArcRmlDocumentPreview::MakeVariantMenu)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text_Lambda([this]() -> FText
					{
						return ArcRmlDocumentPreviewPrivate::MakeVariantText(PreviewVariant);
					})
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(SComboButton)
				.OnGetMenuContent_Static(&SArcRmlPreviewBackground::MakeSettingsMenu)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Background", "Background"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("OpenInTextEditor", "Open in text editor"))
				.OnClicked(this, &SArcRmlDocumentPreview::HandleOpenInTextEditor)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("Reload", "Reload"))
				.OnClicked(this, &SArcRmlDocumentPreview::HandleReload)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(TEXT("Brushes.Recessed")))
				.Padding(8.0f)
				[
					SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFit)
					[
						SNew(SDPIScaler)
						.DPIScale(this, &SArcRmlDocumentPreview::GetGameScale)
						[
							SNew(SBox)
							.WidthOverride_Lambda([this]() -> FOptionalSize
							{
								return GetResolution().X / GetGameScale();
							})
							.HeightOverride_Lambda([this]() -> FOptionalSize
							{
								return GetResolution().Y / GetGameScale();
							})
							[
								SNew(SOverlay)
								+ SOverlay::Slot()
								[
									SNew(SArcRmlPreviewBackground)
								]
								+ SOverlay::Slot()
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Top)
								.Padding(TAttribute<FMargin>::CreateLambda([this]() -> FMargin
								{
									const FSlateRect ContentRect = GetContentRect();
									const float GameScale = GetGameScale();
									return FMargin(ContentRect.Left / GameScale, ContentRect.Top / GameScale, 0.0f, 0.0f);
								}))
								[
									SAssignNew(ViewportSlot, SBox)
									.WidthOverride_Lambda([this]() -> FOptionalSize
									{
										return GetContentRect().GetSize2f().X / GetGameScale();
									})
									.HeightOverride_Lambda([this]() -> FOptionalSize
									{
										return GetContentRect().GetSize2f().Y / GetGameScale();
									})
								]
							]
						]
					]
				]
			]
			+ SSplitter::Slot()
			.Value(0.3f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				.Padding(6.0f)
				[
					SAssignNew(BindingsPanel, SVerticalBox)
				]
			]
		]
	];

	CachesClearedHandle = FArcRmlUIModule::Get().OnDocumentCachesCleared().AddSP(this, &SArcRmlDocumentPreview::Reload);
	Reload();
}

void SArcRmlDocumentPreview::Shutdown()
{
	if (bShutdown)
	{
		return;
	}
	bShutdown = true;

	if (FArcRmlUIModule* RmlUIModule = FModuleManager::GetModulePtr<FArcRmlUIModule>(ArcRmlDocumentPreviewPrivate::RmlUIModuleName))
	{
		RmlUIModule->OnDocumentCachesCleared().Remove(CachesClearedHandle);
	}
	CachesClearedHandle.Reset();

	ReleaseView();
	PreviewModels.Reset();
}

void SArcRmlDocumentPreview::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FPreviewModel& Model : PreviewModels)
	{
		Collector.AddReferencedObject(Model.ViewModel);
	}
}

FString SArcRmlDocumentPreview::GetReferencerName() const
{
	return TEXT("SArcRmlDocumentPreview");
}

void SArcRmlDocumentPreview::Reload()
{
	if (bShutdown)
	{
		return;
	}

	ReleaseView();
	FramePath.Reset();
	BindingsPanel->ClearChildren();

	const TSharedPtr<FArcRmlUIThread> UIThread = FArcRmlUIModule::Get().GetOrStartUIThread();
	if (!UIThread.IsValid())
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: preview of '%ls' has no UI thread", *DocumentPath);
		return;
	}

	TOptional<FArcRmlDocumentHead> Head = ArcRmlViewModelDeclarations::ReadDocumentHead(DocumentPath);
	if (!Head.IsSet())
	{
		return;
	}

	FramePath = Head->Meta.Frame;
	const bool bTab = !FramePath.IsEmpty();
	TArray<FArcRmlViewModelDeclaration> FrameDeclarations;
	if (bTab)
	{
		TOptional<FArcRmlDocumentHead> FrameHead = ArcRmlViewModelDeclarations::ReadFrameHead(DocumentPath, *Head);
		if (!FrameHead.IsSet())
		{
			return;
		}
		FrameDeclarations = MoveTemp(FrameHead->Declarations);
	}

	const TArray<FArcRmlViewModelDeclaration> Declarations = Head->Declarations;
	SyncPreviewModels(Declarations);

	VariantNames.Reset();
	for (const FArcRmlViewModelDeclaration& Declaration : Declarations)
	{
		if (!Declaration.IsPreview())
		{
			continue;
		}
		for (const FString& Variant : ArcRmlPreviewData::GetVariantNames(Declaration.PreviewPath))
		{
			VariantNames.AddUnique(Variant);
		}
	}

	const FString ViewPath = bTab ? FramePath : DocumentPath;
	Viewport = SNew(SArcRmlViewport, UIThread.ToSharedRef(), nullptr);
	Binding = MakeShared<FArcRmlModelBinding>(UIThread.ToSharedRef(), Viewport->GetViewId(), nullptr, nullptr,
		bTab ? MoveTemp(FrameDeclarations) : MoveTemp(Head->Declarations), bTab ? FString() : PreviewVariant, ViewPath);
	Binding->SetEmbeddedPath(bTab ? DocumentPath : FString());
	Binding->Start();
	if (bTab)
	{
		TabBinding = MakeShared<FArcRmlModelBinding>(UIThread.ToSharedRef(), Viewport->GetViewId(), nullptr, nullptr, MoveTemp(Head->Declarations), PreviewVariant,
			DocumentPath);
		TabBinding->Start();
	}

	const TSharedPtr<FArcRmlModelBinding> ModelBinding = bTab ? TabBinding : Binding;
	for (const FPreviewModel& Model : PreviewModels)
	{
		ModelBinding->SetViewModelObject(Model.Name, Model.ViewModel);
	}
	Viewport->SetOnViewModelCall(FArcRmlOnViewModelCall::CreateSP(this, &SArcRmlDocumentPreview::DispatchViewModelCall));
	Viewport->SetRootClass(ArcRmlDocumentPreviewPrivate::EnterRootClass, true);

	const FVector2f ContentSize = GetContentRect().GetSize2f();
	Viewport->LoadDocument(ViewPath, FIntPoint(FMath::RoundToInt(ContentSize.X), FMath::RoundToInt(ContentSize.Y)), GetGameScale(),
		FPlatformTime::Seconds());
	if (bTab)
	{
		Viewport->EmbedDocument(DocumentPath);
	}
	ViewportSlot->SetContent(Viewport.ToSharedRef());

	RebuildBindingsPanel(Declarations);
}

void SArcRmlDocumentPreview::ReleaseView()
{
	if (TabBinding.IsValid())
	{
		TabBinding->Shutdown();
		TabBinding.Reset();
	}

	if (Binding.IsValid())
	{
		Binding->Shutdown();
		Binding.Reset();
	}

	if (Viewport.IsValid())
	{
		Viewport->ReleaseView();
		Viewport.Reset();
	}

	if (ViewportSlot.IsValid())
	{
		ViewportSlot->SetContent(SNullWidget::NullWidget);
	}
}

void SArcRmlDocumentPreview::DispatchViewModelCall(const FArcRmlViewModelCall& Call)
{
	const TSharedPtr<FArcRmlModelBinding> Target = TabBinding.IsValid() && TabBinding->DeclaresModel(Call.ModelName) ? TabBinding : Binding;
	if (Target.IsValid())
	{
		Target->HandleViewModelCall(Call);
	}
}

void SArcRmlDocumentPreview::SyncPreviewModels(const TArray<FArcRmlViewModelDeclaration>& Declarations)
{
	TArray<FPreviewModel> Synced;
	for (const FArcRmlViewModelDeclaration& Declaration : Declarations)
	{
		if (Declaration.IsPreview())
		{
			continue;
		}

		UClass* ViewModelClass = ArcRmlDocumentPreviewPrivate::LoadPreviewClass(Declaration);
		if (!ViewModelClass)
		{
			continue;
		}

		const FPreviewModel* Existing = PreviewModels.FindByPredicate([&Declaration, ViewModelClass](const FPreviewModel& Model) -> bool
		{
			return Model.Name == Declaration.Name && Model.ViewModel && Model.ViewModel->GetClass() == ViewModelClass;
		});

		FPreviewModel& Model = Synced.AddDefaulted_GetRef();
		Model.Name = Declaration.Name;
		Model.ViewModel = Existing ? Existing->ViewModel.Get() : NewObject<UObject>(GetTransientPackage(), ViewModelClass, NAME_None, RF_Transient);
	}
	PreviewModels = MoveTemp(Synced);
}

void SArcRmlDocumentPreview::RebuildBindingsPanel(const TArray<FArcRmlViewModelDeclaration>& Declarations)
{
	BindingsPanel->ClearChildren();

	for (const FArcRmlViewModelDeclaration& Declaration : Declarations)
	{
		const TSharedRef<SVerticalBox> Entry = SNew(SVerticalBox);
		Entry->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Declaration.Name))
				.Font(FAppStyle::GetFontStyle(TEXT("NormalFontBold")))
			];

		if (Declaration.IsPreview())
		{
			Entry->AddSlot()
				.AutoHeight()
				[
					ArcRmlDocumentPreviewPrivate::MakeLabel(FText::Format(LOCTEXT("PreviewFile", "preview: {0}"), FText::FromString(Declaration.PreviewPath)))
				];
			Entry->AddSlot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text_Lambda([this]() -> FText
					{
						return FText::Format(LOCTEXT("PreviewVariant", "variant: {0}"), ArcRmlDocumentPreviewPrivate::MakeVariantText(PreviewVariant));
					})
				];
		}
		else
		{
			Entry->AddSlot()
				.AutoHeight()
				[
					ArcRmlDocumentPreviewPrivate::MakeLabel(FText::Format(LOCTEXT("ClassPath", "class: {0}"), FText::FromString(Declaration.ClassPath)))
				];
		}

		if (!Declaration.Context.IsNone())
		{
			Entry->AddSlot()
				.AutoHeight()
				[
					ArcRmlDocumentPreviewPrivate::MakeLabel(FText::Format(LOCTEXT("Context", "context: {0}"), FText::FromName(Declaration.Context)))
				];
		}

		if (!Declaration.ResolverPath.IsEmpty())
		{
			Entry->AddSlot()
				.AutoHeight()
				[
					ArcRmlDocumentPreviewPrivate::MakeLabel(FText::Format(LOCTEXT("Resolver", "resolver: {0}"), FText::FromString(Declaration.ResolverPath)))
				];
		}

		if (!Declaration.IsPreview())
		{
			const FPreviewModel* Model = PreviewModels.FindByPredicate([&Declaration](const FPreviewModel& Candidate) -> bool
			{
				return Candidate.Name == Declaration.Name;
			});
			Entry->AddSlot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					Model && Model->ViewModel
						? ArcRmlPreviewFields::MakeFieldList(*Model->ViewModel)
						: ArcRmlDocumentPreviewPrivate::MakeLabel(LOCTEXT("NoInstance", "class is not a loadable concrete view model; it binds as defaults"))
				];
		}

		BindingsPanel->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				Entry
			];
	}
}

TSharedRef<SWidget> SArcRmlDocumentPreview::MakeResolutionMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	for (int32 Index = 0; Index < ArcRmlDocumentPreviewPrivate::ResolutionCount; ++Index)
	{
		MenuBuilder.AddMenuEntry(ArcRmlDocumentPreviewPrivate::MakeResolutionText(ArcRmlDocumentPreviewPrivate::Resolutions[Index]), FText::GetEmpty(),
			FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &SArcRmlDocumentPreview::SelectResolution, Index)));
	}
	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SArcRmlDocumentPreview::MakeVariantMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddMenuEntry(ArcRmlDocumentPreviewPrivate::MakeVariantText(FString()), FText::GetEmpty(), FSlateIcon(),
		FUIAction(FExecuteAction::CreateSPLambda(this, [this]()
		{
			SelectVariant(FString());
		})));
	for (const FString& Variant : VariantNames)
	{
		MenuBuilder.AddMenuEntry(ArcRmlDocumentPreviewPrivate::MakeVariantText(Variant), FText::GetEmpty(), FSlateIcon(),
			FUIAction(FExecuteAction::CreateSPLambda(this, [this, Variant]()
			{
				SelectVariant(Variant);
			})));
	}
	return MenuBuilder.MakeWidget();
}

void SArcRmlDocumentPreview::SelectResolution(int32 Index)
{
	ResolutionIndex = Index;
}

void SArcRmlDocumentPreview::SelectVariant(const FString& Variant)
{
	if (Variant == PreviewVariant)
	{
		return;
	}

	PreviewVariant = Variant;
	const TSharedPtr<FArcRmlModelBinding> ModelBinding = TabBinding.IsValid() ? TabBinding : Binding;
	if (Variant.IsEmpty() || !ModelBinding.IsValid())
	{
		Reload();
		return;
	}
	ModelBinding->SetPreviewVariant(Variant);
}

FReply SArcRmlDocumentPreview::HandleOpenInTextEditor()
{
	FString Filename;
	if (!ArcRmlPaths::ToFilename(FArcRmlUIModule::Get().GetContentRoots(), DocumentPath, Filename))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: preview cannot resolve '%ls' to a file", *DocumentPath);
		return FReply::Handled();
	}
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*Filename, nullptr, ELaunchVerb::Edit);
	return FReply::Handled();
}

FReply SArcRmlDocumentPreview::HandleReload()
{
	TArray<FString> Changed;
	Changed.Add(DocumentPath);
	if (!FramePath.IsEmpty())
	{
		Changed.Add(FramePath);
	}
	UArcRmlViewportSubsystem::CompileAndReloadOpenDocuments(MoveTemp(Changed));
	return FReply::Handled();
}

FIntPoint SArcRmlDocumentPreview::GetResolution() const
{
	return ArcRmlDocumentPreviewPrivate::Resolutions[ResolutionIndex];
}

float SArcRmlDocumentPreview::GetGameScale() const
{
	const UUserInterfaceSettings* Settings = GetDefault<UUserInterfaceSettings>();
	return Settings->GetDPIScaleBasedOnSize(GetResolution()) / Settings->ApplicationScale * UArcRmlUISettingsSubsystem::GetDefaults().UIScale;
}

FSlateRect SArcRmlDocumentPreview::GetContentRect() const
{
	const FIntPoint Resolution = GetResolution();
	return ArcRmlContentRect::Compute(FSlateRect(0.0f, 0.0f, Resolution.X, Resolution.Y), FMargin(), UArcRmlUISettingsSubsystem::GetDefaults().MaxUIAspectRatio);
}

#undef LOCTEXT_NAMESPACE
