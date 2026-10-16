// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcRmlUIPerf.h"
#include "Containers/Ticker.h"
#include "FieldNotificationId.h"
#include "MVVM/ArcRmlModelTypes.h"
#include "MVVM/ArcRmlViewModelBindings.h"
#include "MVVM/ArcRmlViewModelDeclarations.h"
#include "UObject/GCObject.h"

class APlayerController;
class FObjectPropertyBase;
struct FArcRmlStructList;
struct FInstancedStruct;
class FArcRmlUIThread;
class ULocalPlayer;
class UMVVMViewModelCollectionObject;
class UMVVMViewModelContextResolver;
class UArcRmlResolverContextWidget;
class UWorld;

enum class EArcRmlDocumentAction : uint8
{
	Open,
	Close
};

DECLARE_DELEGATE_TwoParams(FArcRmlOnDocumentAction, EArcRmlDocumentAction, const FString&);

class ARCRMLUI_API FArcRmlModelBinding final : public FGCObject
{
public:
	FArcRmlModelBinding(const TSharedRef<FArcRmlUIThread>& InThread, uint32 InViewId, ULocalPlayer* InLocalPlayer, UWorld* InWorld,
		TArray<FArcRmlViewModelDeclaration>&& InDeclarations, const FString& InPreviewVariant, const FString& InDocumentPath);
	virtual ~FArcRmlModelBinding() override;

	void Start();
	void Shutdown();
	void SetPaused(bool bInPaused);
	bool SetPreviewVariant(const FString& Variant);
	void SetViewModelObject(const FString& ModelName, UObject* ViewModel);
	void HandleViewModelCall(const FArcRmlViewModelCall& Call);
	void SetOnDocumentAction(FArcRmlOnDocumentAction&& InOnDocumentAction);
	void SetEmbeddedPath(const FString& Path);
	bool DeclaresModel(const FString& ModelName) const;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

private:
	struct FDeclarationState
	{
		FArcRmlViewModelDeclaration Declaration;
		FArcRmlSourceId SourceId;
		TObjectPtr<UClass> ViewModelClass;
		TObjectPtr<UMVVMViewModelContextResolver> Resolver;
		TObjectPtr<UObject> ViewModel;
		bool bValid = false;
		bool bBound = false;
		bool bEmbeddedFieldBound = false;
		bool bResolvedFlagBound = false;
		bool bSentResolved = false;
		bool bPreviewResolvedBound = false;
		bool bSentPreviewResolved = false;
	};

	struct FListMirror
	{
		TArray<FString> Keys;
		TArray<uint64> Versions;
		uint64 StructureVersion = 0;
	};

	struct FSourceInfo
	{
		FString ModelName;
		TArray<FArcRmlPathEntry> Path;
		UClass* Class = nullptr;
	};

	bool InitializeDeclaration(FDeclarationState& State);
	void StartPreviewDeclaration(FArcRmlUIThread& UIThread, FDeclarationState& State);
	void ReloadPreviewDeclaration(FArcRmlUIThread& UIThread, FDeclarationState& State);
	bool HandleDocumentAction(const FArcRmlViewModelCall& Call);
	void LogPreviewCall(const FArcRmlViewModelCall& Call) const;
	UObject* ResolveViewModel(FDeclarationState& State);
	void ResolveDeclarations(bool bOnlyUnresolved);
	void SetViewModel(FDeclarationState& State, UObject* ViewModel);
	UMVVMViewModelCollectionObject* EnsureCollection();
	void EnsureProxyWidget();
	APlayerController* GetPlayerController() const;

	void BuildSource(const FArcRmlSourceId& SourceId, const FString& ModelName, const TArray<FArcRmlPathEntry>& Path, UClass* Class, UObject* Object);
	UObject* EvaluateChildObject(UObject* ParentObject, FName FieldName);
	void EvaluateArrayElements(UObject* ParentObject, TArray<UObject*>& OutElements, FName FieldName);
	void HandleArrayElementAdded(FArcRmlSourceId ElementId, int32 Index, UObject* ElementObject, FArcRmlSourceId ParentId, FName FieldName, UClass* ElementClass);
	void HandleSourceRemoved(FArcRmlSourceId SourceId);
	void HandleFieldChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId, FArcRmlSourceId SourceId);
	void HandleCollectionChanged();

	FArcRmlValue ReadObject(UObject* Object, UClass* Class, int32 Depth, const FString* ValuePath);
	FArcRmlValue ReadField(UObject* Object, UClass* Class, const UE::FieldNotification::FFieldVariant& Field, int32 Depth, const FString* ValuePath);
	FArcRmlValue ConvertValue(const FProperty& Property, const void* ValuePtr, int32 Depth, const FString& FieldKey, const FString* ValuePath);
	FArcRmlValue ConvertStruct(const UScriptStruct& Struct, const void* ValuePtr, int32 Depth, const FString& FieldKey);
	FArcRmlValue ConvertListRow(const FInstancedStruct& Row, int32 Depth, const FString& FieldKey);
	FArcRmlValue ConvertStructList(const FArcRmlStructList& List, int32 Depth, const FString& FieldKey, const FString* ValuePath);
	TArray<FArcRmlListOp> DiffStructList(FListMirror& Mirror, const FArcRmlStructList& List, int32 Depth, const FString& FieldKey);

	bool FillParameters(UFunction& Function, uint8* Params, const FArcRmlViewModelCall& Call, TArray<FString>& OutResolvedArguments);
	bool ResolveArgument(const FArcRmlValue& Argument, const FArcRmlViewModelCall& Call, FArcRmlValue& OutArgument) const;
	bool FillObjectParameter(const FObjectPropertyBase& Parameter, void* ValuePtr, const FArcRmlViewModelCall& Call, int32& InOutArgumentIndex,
		TArray<FString>& OutResolvedArguments);

	bool Tick(float DeltaTime);
	void Flush();
	void FlushResolvedFlags(FArcRmlUIThread& UIThread, bool bResolved);
	void AddEmbeddedField(FDeclarationState& State, FArcRmlValue& Snapshot, const FString& Source);

	TWeakPtr<FArcRmlUIThread> Thread;
	uint32 ViewId = 0;
	TWeakObjectPtr<ULocalPlayer> LocalPlayer;
	TWeakObjectPtr<UWorld> World;
	TArray<FDeclarationState> Declarations;
	FString PreviewVariant;
	FString EmbeddedPath;
	TObjectPtr<UArcRmlResolverContextWidget> ProxyWidget;
	FArcRmlOnDocumentAction OnDocumentAction;
	TUniquePtr<FArcRmlViewModelBindings> Bindings;
	TMap<FArcRmlSourceId, FSourceInfo> SourceInfos;
	TSet<TPair<FArcRmlSourceId, FName>> DirtyFields;
	TSet<FString> WarnedFields;
	// Keyed by model path; each binding diffs against what it sent to its own document.
	TMap<FString, FListMirror> ListMirrors;
	int32 ConvertedRows = 0;
	TWeakObjectPtr<UMVVMViewModelCollectionObject> Collection;
	FDelegateHandle CollectionChangedHandle;
	FTSTicker::FDelegateHandle TickerHandle;
	float RetryElapsed = 0.0f;
	bool bHasPlayer = false;
	bool bStarted = false;
	bool bPaused = false;
	bool bPublishesEmbedded = false;

#if !UE_BUILD_SHIPPING
	struct FModelPerf
	{
		int32 Notifications = 0;
		int32 Deltas = 0;
		int32 ListOps = 0;
		int32 ConvertedRows = 0;
		ArcRmlUIPerf::FTiming Convert;
	};

	void ReportPerf();

	FString DocumentPath;
	TMap<FString, FModelPerf> ModelPerf;
	ArcRmlUIPerf::FReportClock PerfClock;
#endif
};
