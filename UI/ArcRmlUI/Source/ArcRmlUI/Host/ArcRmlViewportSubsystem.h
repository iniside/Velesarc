// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcRmlDocumentRef.h"
#include "Containers/Ticker.h"
#include "GameplayTagContainer.h"
#include "MVVM/ArcRmlViewModelDeclarations.h"
#include "Subsystems/LocalPlayerSubsystem.h"

#include "ArcRmlViewportSubsystem.generated.h"

class FArcRmlModelBinding;
enum class EArcRmlDocumentAction : uint8;
struct FArcRmlViewModelCall;
class SArcRmlViewport;
class UArcRmlActivatableWidget;
class UArcSettingsModel;
class UInputAction;

UCLASS()
class ARCRMLUI_API UArcRmlViewportSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "ArcRmlUI")
	bool OpenDocument(const FArcRmlDocumentRef& Document, const UInputAction* CloseAction = nullptr);

	UFUNCTION(BlueprintCallable, Category = "ArcRmlUI")
	void CloseDocument(const FArcRmlDocumentRef& Document);

	// A document that is playing its leave transition counts as not open.
	bool IsDocumentOpen(const FArcRmlDocumentRef& Document) const;

	bool PreviewDocument(const FArcRmlDocumentRef& Document, const FString& Variant);
#if !UE_BUILD_SHIPPING
	void SetPreviewVariant(const FArcRmlDocumentRef& Document, const FString& Variant);
	void ClickElement(const FArcRmlDocumentRef& Document, const FString& Selector, int32 Index);
	void DumpLayout(const FArcRmlDocumentRef& Document, const FString& OutDir);
#endif

	float ReadMaxContentAspect() const;

	void NotifyActivationChanged(const UArcRmlActivatableWidget& Widget, const FArcRmlDocumentRef& Document, bool bActivated);

#if WITH_EDITOR
	static void CompileAndReloadOpenDocuments(TArray<FString>&& ChangedPackagePaths);
#endif

private:
	struct FOpenView
	{
		TWeakObjectPtr<UArcRmlActivatableWidget> Widget;
		TSharedPtr<SArcRmlViewport> Viewport;
		TSharedPtr<FArcRmlModelBinding> Binding;
		TSharedPtr<FArcRmlModelBinding> TabBinding;
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<const UInputAction> CloseAction;
		FString PreviewVariant;
		FString EmbeddedPath;
		FString EmbeddedPreviewVariant;
		FArcRmlDocumentMeta Meta;
		uint64 OpenOrder = 0;
		FTSTicker::FDelegateHandle LeaveTimeoutHandle;
		double LeaveStartSeconds = 0.0;
		bool bHidden = false;
		bool bClosing = false;
	};

	bool OpenView(const FString& Path, const UInputAction* CloseAction, const FString& PreviewVariant);
	bool OpenTab(const FString& TabPath, FArcRmlDocumentHead&& TabHead, const UInputAction* CloseAction, const FString& PreviewVariant, double OpenSeconds);
	FOpenView* CreateView(const FString& Path, FArcRmlDocumentHead&& Head, const UInputAction* CloseAction, const FString& PreviewVariant, double OpenSeconds);
	bool SwapEmbed(const FString& FramePath, FOpenView& View, const FString& TabPath, TArray<FArcRmlViewModelDeclaration>&& TabDeclarations,
		const FString& PreviewVariant);
	FString FindFramePath(const FString& TabPath) const;
	FString ResolveViewPath(const FString& Path) const;
	void DispatchViewModelCall(const FString& ViewPath, const FArcRmlViewModelCall& Call);
	void HandleDocumentAction(const FString& ViewPath, EArcRmlDocumentAction Action, const FString& Target);
	void CloseDocumentImmediately(const FString& Path);
	void StartLeave(const FString& Path, FOpenView& View);
	void CancelLeave(const FString& Path, FOpenView& View);
	void FinishLeave(const FString& Path, bool bTimedOut);
	void DestroyView(const FOpenView& View);
	void UpdateVisibility();
	void SetViewHidden(const FString& Path, FOpenView& View, bool bHidden);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

	// Sole owner of each open document's viewport and binding; they are destroyed only when the document leaves its layer.
	TMap<FString, FOpenView> OpenViews;
	uint64 NextOpenOrder = 0;
	FDelegateHandle WorldCleanupHandle;
	TWeakObjectPtr<UArcSettingsModel> SettingsModel;
};
