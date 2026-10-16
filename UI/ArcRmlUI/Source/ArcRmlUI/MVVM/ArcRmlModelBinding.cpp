// Copyright Lukasz Baran. All Rights Reserved.

#include "MVVM/ArcRmlModelBinding.h"

#include "ArcRmlStructList.h"
#include "ArcRmlUILog.h"
#include "Bindings/MVVMBindingHelper.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "HAL/PlatformTime.h"
#include "INotifyFieldValueChanged.h"
#include "MVVM/ArcRmlPreviewData.h"
#include "MVVM/ArcRmlResolverContextWidget.h"
#include "MVVMGameSubsystem.h"
#include "MVVMViewModelBase.h"
#include "Thread/ArcRmlUIThread.h"
#include "Types/MVVMViewModelCollection.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"
#include "View/MVVMViewModelContextResolver.h"

namespace ArcRmlModelBindingPrivate
{
	using FFieldId = UE::FieldNotification::FFieldId;
	using FFieldVariant = UE::FieldNotification::FFieldVariant;

	constexpr int32 MaxDepth = 16;
	constexpr float RetryInterval = 1.0f;
	const TCHAR* const ResolvedFieldName = TEXT("ArcResolved");
	const TCHAR* const EmbeddedFieldName = TEXT("ArcEmbedded");
	const FName WorldContextParameterName(TEXT("WorldContextObject"));
	const TCHAR* const DragArgumentPrefix = TEXT("drag:");
	const TCHAR* const TargetArgumentPrefix = TEXT("target:");
	const TCHAR* const ViewModelArgumentPrefix = TEXT("vm:");
	const TCHAR* const OpenActionPrefix = TEXT("open:");
	const TCHAR* const CloseActionName = TEXT("close");

	bool IsViewModelClass(const UClass* Class)
	{
		return Class && Class->IsChildOf(UMVVMViewModelBase::StaticClass());
	}

	FFieldVariant FindField(const UClass* Class, FName FieldName)
	{
		if (!Class)
		{
			return FFieldVariant();
		}
		if (const UFunction* Function = Class->FindFunctionByName(FieldName))
		{
			return FFieldVariant(Function);
		}
		if (const FProperty* Property = Class->FindPropertyByName(FieldName))
		{
			return FFieldVariant(Property);
		}
		return FFieldVariant();
	}

	const FProperty* GetValueProperty(const FFieldVariant& Field)
	{
		if (Field.IsProperty())
		{
			return Field.GetProperty();
		}
		if (Field.IsFunction())
		{
			return UE::MVVM::BindingHelper::GetReturnProperty(Field.GetFunction());
		}
		return nullptr;
	}

	bool IsStructList(const FProperty* Property)
	{
		const FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		return StructProperty && StructProperty->Struct == FArcRmlStructList::StaticStruct();
	}

	FString AppendPath(const FString& Base, const FArcRmlPathEntry& Entry)
	{
		return Entry.Index != INDEX_NONE ? FString::Printf(TEXT("%s/#%d"), *Base, Entry.Index) : FString::Printf(TEXT("%s/%s"), *Base, *Entry.Name);
	}

	FString MakePath(const FString& ModelName, const TArray<FArcRmlPathEntry>& Path)
	{
		FString Result = ModelName;
		for (const FArcRmlPathEntry& Entry : Path)
		{
			Result = AppendPath(Result, Entry);
		}
		return Result;
	}

	TBitArray<> FindLongestIncreasing(const TArray<int32>& Sequence, int32 ValueCount)
	{
		TArray<int32> TailIndices;
		TArray<int32> Previous;
		Previous.Init(INDEX_NONE, Sequence.Num());
		for (int32 Index = 0; Index < Sequence.Num(); ++Index)
		{
			const int32 Value = Sequence[Index];
			int32 Low = 0;
			int32 High = TailIndices.Num();
			while (Low < High)
			{
				const int32 Mid = (Low + High) / 2;
				if (Sequence[TailIndices[Mid]] < Value)
				{
					Low = Mid + 1;
				}
				else
				{
					High = Mid;
				}
			}
			Previous[Index] = Low > 0 ? TailIndices[Low - 1] : INDEX_NONE;
			if (Low == TailIndices.Num())
			{
				TailIndices.Add(Index);
			}
			else
			{
				TailIndices[Low] = Index;
			}
		}

		TBitArray<> Result(false, ValueCount);
		int32 Cursor = TailIndices.Num() > 0 ? TailIndices.Last() : INDEX_NONE;
		while (Cursor != INDEX_NONE)
		{
			Result[Sequence[Cursor]] = true;
			Cursor = Previous[Cursor];
		}
		return Result;
	}

	void ForEachSourceField(UClass* Class, TFunctionRef<void(FFieldId, const FFieldVariant&)> Callback)
	{
		UObject* DefaultObject = Class ? Class->GetDefaultObject() : nullptr;
		const INotifyFieldValueChanged* Notify = Cast<INotifyFieldValueChanged>(DefaultObject);
		if (!Notify)
		{
			return;
		}

		Notify->GetFieldNotificationDescriptor().ForEachField(Class, [DefaultObject, &Callback](FFieldId FieldId) -> bool
		{
			const FFieldVariant Field = FieldId.ToVariant(DefaultObject);
			const bool bReadable = Field.IsProperty()
				? UE::MVVM::BindingHelper::IsValidForSourceBinding(Field.GetProperty())
				: Field.IsFunction() && UE::MVVM::BindingHelper::IsValidForSourceBinding(Field.GetFunction());
			if (bReadable)
			{
				Callback(FieldId, Field);
			}
			return true;
		});
	}

	bool ReadFieldValue(UObject* Object, UClass* Class, const FFieldVariant& Field, TFunctionRef<void(const FProperty&, const void*)> Visitor)
	{
		const FProperty* ValueProperty = GetValueProperty(Field);
		if (!ValueProperty)
		{
			return false;
		}

		if (Field.IsProperty())
		{
			UObject* Container = Object ? Object : (Class ? Class->GetDefaultObject() : nullptr);
			if (!Container)
			{
				return false;
			}
			if (IsStructList(ValueProperty))
			{
				Visitor(*ValueProperty, ValueProperty->ContainerPtrToValuePtr<void>(Container));
				return true;
			}
			void* Buffer = FMemory::Malloc(ValueProperty->GetSize(), ValueProperty->GetMinAlignment());
			ValueProperty->InitializeValue(Buffer);
			ValueProperty->GetValue_InContainer(Container, Buffer);
			Visitor(*ValueProperty, Buffer);
			ValueProperty->DestroyValue(Buffer);
			FMemory::Free(Buffer);
			return true;
		}

		UFunction* Function = const_cast<UFunction*>(Field.GetFunction());
		uint8* Params = static_cast<uint8*>(FMemory::Malloc(FMath::Max<int32>(Function->ParmsSize, 1), Function->GetMinAlignment()));
		Function->InitializeStruct(Params);
		if (Object)
		{
			Object->ProcessEvent(Function, Params);
		}
		Visitor(*ValueProperty, ValueProperty->ContainerPtrToValuePtr<void>(Params));
		Function->DestroyStruct(Params);
		FMemory::Free(Params);
		return true;
	}

	FString ArgumentToString(const FArcRmlValue& Value)
	{
		switch (Value.Kind)
		{
		case EArcRmlValueKind::Bool:
			return Value.bBool ? TEXT("true") : TEXT("false");
		case EArcRmlValueKind::Int:
			return LexToString(Value.Int);
		case EArcRmlValueKind::Float:
			return FString::SanitizeFloat(Value.Float);
		case EArcRmlValueKind::String:
			return Value.String;
		default:
			return FString();
		}
	}

	bool ConvertArgument(const FProperty& Parameter, void* ValuePtr, const FArcRmlValue& Argument)
	{
		if (Argument.Kind == EArcRmlValueKind::Empty || Argument.Kind == EArcRmlValueKind::Array || Argument.Kind == EArcRmlValueKind::Struct
			|| Argument.Kind == EArcRmlValueKind::Colour)
		{
			return false;
		}

		if (const FBoolProperty* BoolParameter = CastField<FBoolProperty>(&Parameter))
		{
			bool bValue = false;
			switch (Argument.Kind)
			{
			case EArcRmlValueKind::Bool:
				bValue = Argument.bBool;
				break;
			case EArcRmlValueKind::Int:
				bValue = Argument.Int != 0;
				break;
			case EArcRmlValueKind::Float:
				bValue = Argument.Float != 0.0;
				break;
			default:
				bValue = Argument.String.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Argument.String == TEXT("1");
				break;
			}
			BoolParameter->SetPropertyValue(ValuePtr, bValue);
			return true;
		}

		const FNumericProperty* NumericParameter = CastField<FNumericProperty>(&Parameter);
		if (NumericParameter && NumericParameter->GetIntPropertyEnum() == nullptr)
		{
			switch (Argument.Kind)
			{
			case EArcRmlValueKind::Bool:
			case EArcRmlValueKind::Int:
			{
				const int64 IntValue = Argument.Kind == EArcRmlValueKind::Bool ? (Argument.bBool ? 1 : 0) : Argument.Int;
				if (NumericParameter->IsFloatingPoint())
				{
					NumericParameter->SetFloatingPointPropertyValue(ValuePtr, static_cast<double>(IntValue));
				}
				else
				{
					NumericParameter->SetIntPropertyValue(ValuePtr, IntValue);
				}
				return true;
			}
			case EArcRmlValueKind::Float:
				if (NumericParameter->IsFloatingPoint())
				{
					NumericParameter->SetFloatingPointPropertyValue(ValuePtr, Argument.Float);
				}
				else
				{
					NumericParameter->SetIntPropertyValue(ValuePtr, static_cast<int64>(FMath::RoundToDouble(Argument.Float)));
				}
				return true;
			default:
			{
				const FString Trimmed = Argument.String.TrimStartAndEnd();
				if (!Trimmed.IsNumeric())
				{
					return false;
				}
				NumericParameter->SetNumericPropertyValueFromString(ValuePtr, *Trimmed);
				return true;
			}
			}
		}

		const FString Text = ArgumentToString(Argument);
		if (const FStrProperty* StringParameter = CastField<FStrProperty>(&Parameter))
		{
			StringParameter->SetPropertyValue(ValuePtr, Text);
			return true;
		}
		if (const FNameProperty* NameParameter = CastField<FNameProperty>(&Parameter))
		{
			NameParameter->SetPropertyValue(ValuePtr, FName(*Text));
			return true;
		}
		if (const FTextProperty* TextParameter = CastField<FTextProperty>(&Parameter))
		{
			TextParameter->SetPropertyValue(ValuePtr, FText::FromString(Text));
			return true;
		}
		return Parameter.ImportText_Direct(*Text, ValuePtr, nullptr, PPF_None) != nullptr;
	}
}

FArcRmlModelBinding::FArcRmlModelBinding(const TSharedRef<FArcRmlUIThread>& InThread, uint32 InViewId, ULocalPlayer* InLocalPlayer, UWorld* InWorld,
	TArray<FArcRmlViewModelDeclaration>&& InDeclarations, const FString& InPreviewVariant, const FString& InDocumentPath)
	: Thread(InThread)
	, ViewId(InViewId)
	, LocalPlayer(InLocalPlayer)
	, World(InWorld)
	, PreviewVariant(InPreviewVariant)
	, bHasPlayer(InLocalPlayer != nullptr)
{
	check((InLocalPlayer == nullptr) == (InWorld == nullptr));
	ARCRMLUI_PERF(DocumentPath = InDocumentPath);
	Declarations.Reserve(InDeclarations.Num());
	for (FArcRmlViewModelDeclaration& Declaration : InDeclarations)
	{
		FDeclarationState& State = Declarations.AddDefaulted_GetRef();
		State.Declaration = MoveTemp(Declaration);
	}
}

FArcRmlModelBinding::~FArcRmlModelBinding()
{
	Shutdown();
}

void FArcRmlModelBinding::Start()
{
	check(IsInGameThread());
	if (bStarted)
	{
		return;
	}
	bStarted = true;

	const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin();
	if (!PinnedThread.IsValid())
	{
		return;
	}

	Bindings = MakeUnique<FArcRmlViewModelBindings>();
	Bindings->SetOnSourceRemoved(FArcRmlViewModelBindings::FSourceRemovedDelegate::CreateRaw(this, &FArcRmlModelBinding::HandleSourceRemoved));

	if (bHasPlayer)
	{
		EnsureCollection();
		EnsureProxyWidget();
	}

	for (FDeclarationState& State : Declarations)
	{
		if (State.Declaration.IsPreview())
		{
			StartPreviewDeclaration(*PinnedThread, State);
			continue;
		}

		if (!InitializeDeclaration(State))
		{
			continue;
		}

		State.ViewModel = bHasPlayer ? ResolveViewModel(State) : nullptr;
		if (bHasPlayer && !State.ViewModel && !State.Declaration.bOptional)
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: view model '%ls' is not available yet; it binds as defaults until it resolves", *State.Declaration.Name);
		}

		BuildSource(State.SourceId, State.Declaration.Name, TArray<FArcRmlPathEntry>(), State.ViewModelClass, State.ViewModel);
		FArcRmlValue Snapshot = ReadObject(State.ViewModel, State.ViewModelClass, 0, &State.Declaration.Name);
		if (Snapshot.MemberNames.Contains(ArcRmlModelBindingPrivate::ResolvedFieldName))
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: view model '%ls' (%ls) has a field named '%ls', which collides with the resolved flag; the flag is not bound",
				*State.Declaration.Name, *State.ViewModelClass->GetPathName(), ArcRmlModelBindingPrivate::ResolvedFieldName);
		}
		else
		{
			FArcRmlValue& ResolvedValue = Snapshot.Children.AddDefaulted_GetRef();
			ResolvedValue.Kind = EArcRmlValueKind::Bool;
			ResolvedValue.bBool = State.ViewModel != nullptr;
			Snapshot.MemberNames.Add(ArcRmlModelBindingPrivate::ResolvedFieldName);
			State.bResolvedFlagBound = true;
			State.bSentResolved = ResolvedValue.bBool;
		}
		AddEmbeddedField(State, Snapshot, State.ViewModelClass->GetPathName());
		PinnedThread->BindModel(ViewId, State.Declaration.Name, MoveTemp(Snapshot));
		State.bBound = true;
	}

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FArcRmlModelBinding::Tick));
}

void FArcRmlModelBinding::Shutdown()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	if (Bindings)
	{
		for (const FDeclarationState& State : Declarations)
		{
			if (State.bValid && State.ViewModel && !State.Declaration.OnClose.IsEmpty())
			{
				FArcRmlViewModelCall CloseCall;
				CloseCall.ModelName = State.Declaration.Name;
				CloseCall.FunctionName = State.Declaration.OnClose;
				HandleViewModelCall(CloseCall);
			}
		}
	}

	if (const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin())
	{
		for (FDeclarationState& State : Declarations)
		{
			if (State.bBound)
			{
				PinnedThread->UnbindModel(ViewId, State.Declaration.Name);
			}
		}
	}
	for (FDeclarationState& State : Declarations)
	{
		State.bBound = false;
	}

	if (UMVVMViewModelCollectionObject* CollectionObject = Collection.Get())
	{
		CollectionObject->OnCollectionChanged().Remove(CollectionChangedHandle);
	}
	Collection.Reset();
	CollectionChangedHandle.Reset();

	Bindings.Reset();
	SourceInfos.Reset();
	DirtyFields.Reset();
	ListMirrors.Reset();

	for (FDeclarationState& State : Declarations)
	{
		if (State.Resolver && State.ViewModel)
		{
			State.Resolver->DestroyInstance(State.ViewModel, nullptr);
		}
		State.ViewModel = nullptr;
		State.Resolver = nullptr;
		State.ViewModelClass = nullptr;
		State.bValid = false;
		State.bEmbeddedFieldBound = false;
		State.bResolvedFlagBound = false;
		State.bSentResolved = false;
		State.bPreviewResolvedBound = false;
		State.bSentPreviewResolved = false;
	}
	ProxyWidget = nullptr;
}

void FArcRmlModelBinding::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ProxyWidget);
	for (FDeclarationState& State : Declarations)
	{
		Collector.AddReferencedObject(State.ViewModelClass);
		Collector.AddReferencedObject(State.Resolver);
		Collector.AddReferencedObject(State.ViewModel);
	}
}

FString FArcRmlModelBinding::GetReferencerName() const
{
	return TEXT("FArcRmlModelBinding");
}

bool FArcRmlModelBinding::InitializeDeclaration(FDeclarationState& State)
{
	const FArcRmlViewModelDeclaration& Declaration = State.Declaration;
	State.SourceId = FArcRmlSourceId::Make(Declaration.Name);

	UClass* ViewModelClass = FSoftClassPath(Declaration.ClassPath).TryLoadClass<UObject>();
	if (!ArcRmlModelBindingPrivate::IsViewModelClass(ViewModelClass))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: view model '%ls' has class '%ls', which is not a loadable UMVVMViewModelBase", *Declaration.Name, *Declaration.ClassPath);
		return false;
	}
	State.ViewModelClass = ViewModelClass;

	if (bHasPlayer && !Declaration.ResolverPath.IsEmpty())
	{
		UClass* ResolverClass = FSoftClassPath(Declaration.ResolverPath).TryLoadClass<UMVVMViewModelContextResolver>();
		if (!ResolverClass || !ResolverClass->IsChildOf(UMVVMViewModelContextResolver::StaticClass()) || ResolverClass->HasAnyClassFlags(CLASS_Abstract))
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: view model '%ls' has resolver '%ls', which is not a loadable concrete UMVVMViewModelContextResolver",
				*Declaration.Name, *Declaration.ResolverPath);
			return false;
		}

		UMVVMViewModelContextResolver* Resolver = NewObject<UMVVMViewModelContextResolver>(GetTransientPackage(), ResolverClass);
		for (const TPair<FString, FString>& Property : Declaration.ResolverProperties)
		{
			FProperty* ResolverProperty = ResolverClass->FindPropertyByName(FName(*Property.Key));
			if (!ResolverProperty)
			{
				UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: view model '%ls' sets unknown resolver property '%ls'", *Declaration.Name, *Property.Key);
				continue;
			}
			if (!ResolverProperty->ImportText_InContainer(*Property.Value, static_cast<void*>(Resolver), Resolver, PPF_None))
			{
				UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: view model '%ls' could not import '%ls' into resolver property '%ls'", *Declaration.Name, *Property.Value,
					*Property.Key);
			}
		}
		State.Resolver = Resolver;
	}

	State.bValid = true;
	return true;
}

void FArcRmlModelBinding::StartPreviewDeclaration(FArcRmlUIThread& UIThread, FDeclarationState& State)
{
	const FArcRmlViewModelDeclaration& Declaration = State.Declaration;
	FArcRmlValue Snapshot;
	const bool bLoaded = ArcRmlPreviewData::LoadModel(Declaration.PreviewPath, PreviewVariant, Declaration.Name, Snapshot);
	if (Snapshot.MemberNames.Contains(ArcRmlModelBindingPrivate::ResolvedFieldName))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: preview model '%ls' (%ls) has a field named '%ls', which collides with the resolved flag; the flag is not bound",
			*Declaration.Name, *Declaration.PreviewPath, ArcRmlModelBindingPrivate::ResolvedFieldName);
	}
	else
	{
		FArcRmlValue& ResolvedValue = Snapshot.Children.AddDefaulted_GetRef();
		ResolvedValue.Kind = EArcRmlValueKind::Bool;
		ResolvedValue.bBool = bLoaded && ArcRmlPreviewData::ShowsPreviewData();
		Snapshot.MemberNames.Add(ArcRmlModelBindingPrivate::ResolvedFieldName);
		State.bPreviewResolvedBound = true;
		State.bSentPreviewResolved = ResolvedValue.bBool;
	}
	AddEmbeddedField(State, Snapshot, Declaration.PreviewPath);
	UIThread.BindModel(ViewId, Declaration.Name, MoveTemp(Snapshot));
	State.bBound = true;
}

void FArcRmlModelBinding::AddEmbeddedField(FDeclarationState& State, FArcRmlValue& Snapshot, const FString& Source)
{
	if (!bPublishesEmbedded)
	{
		return;
	}

	if (Snapshot.MemberNames.Contains(ArcRmlModelBindingPrivate::EmbeddedFieldName))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: view model '%ls' (%ls) has a field named '%ls', which collides with the embedded document path; the path is not bound",
			*State.Declaration.Name, *Source, ArcRmlModelBindingPrivate::EmbeddedFieldName);
		return;
	}

	FArcRmlValue& EmbeddedValue = Snapshot.Children.AddDefaulted_GetRef();
	EmbeddedValue.Kind = EArcRmlValueKind::String;
	EmbeddedValue.String = EmbeddedPath;
	Snapshot.MemberNames.Add(ArcRmlModelBindingPrivate::EmbeddedFieldName);
	State.bEmbeddedFieldBound = true;
}

void FArcRmlModelBinding::ReloadPreviewDeclaration(FArcRmlUIThread& UIThread, FDeclarationState& State)
{
	const FArcRmlViewModelDeclaration& Declaration = State.Declaration;
	FArcRmlValue Snapshot;
	const bool bLoaded = ArcRmlPreviewData::LoadModel(Declaration.PreviewPath, PreviewVariant, Declaration.Name, Snapshot);
	for (int32 Index = 0; Index < Snapshot.MemberNames.Num(); ++Index)
	{
		TArray<FArcRmlPathEntry> Path;
		Path.Add(FArcRmlPathEntry{Snapshot.MemberNames[Index], INDEX_NONE});
		UIThread.ApplyModelDelta(ViewId, Declaration.Name, MoveTemp(Path), MoveTemp(Snapshot.Children[Index]));
	}

	const bool bResolved = bLoaded && ArcRmlPreviewData::ShowsPreviewData();
	if (State.bPreviewResolvedBound && State.bSentPreviewResolved != bResolved)
	{
		State.bSentPreviewResolved = bResolved;
		FArcRmlValue Value;
		Value.Kind = EArcRmlValueKind::Bool;
		Value.bBool = bResolved;
		TArray<FArcRmlPathEntry> Path;
		Path.Add(FArcRmlPathEntry{ArcRmlModelBindingPrivate::ResolvedFieldName, INDEX_NONE});
		UIThread.ApplyModelDelta(ViewId, Declaration.Name, MoveTemp(Path), MoveTemp(Value));
	}
}

bool FArcRmlModelBinding::SetPreviewVariant(const FString& Variant)
{
	check(IsInGameThread());

	const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin();
	if (!PinnedThread.IsValid() || !bStarted)
	{
		return false;
	}

	bool bKnown = false;
	for (const FDeclarationState& State : Declarations)
	{
		if (State.Declaration.IsPreview() && ArcRmlPreviewData::HasVariant(State.Declaration.PreviewPath, Variant))
		{
			bKnown = true;
			break;
		}
	}
	if (!bKnown)
	{
		return false;
	}

	PreviewVariant = Variant;
	for (FDeclarationState& State : Declarations)
	{
		if (State.Declaration.IsPreview())
		{
			ReloadPreviewDeclaration(*PinnedThread, State);
		}
	}
	return true;
}

void FArcRmlModelBinding::SetViewModelObject(const FString& ModelName, UObject* ViewModel)
{
	check(IsInGameThread());
	if (!ensure(!bHasPlayer) || !bStarted)
	{
		return;
	}

	FDeclarationState* State = Declarations.FindByPredicate([&ModelName](const FDeclarationState& Candidate) -> bool
	{
		return Candidate.Declaration.Name == ModelName;
	});
	if (!State || !State->bValid)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: view model object for '%ls' has no valid class declaration to bind to", *ModelName);
		return;
	}
	if (ViewModel && !ViewModel->IsA(State->ViewModelClass.Get()))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: view model object '%ls' is not a '%ls', which '%ls' declares", *ViewModel->GetPathName(),
			*State->ViewModelClass->GetPathName(), *ModelName);
		return;
	}
	SetViewModel(*State, ViewModel);
}

UObject* FArcRmlModelBinding::ResolveViewModel(FDeclarationState& State)
{
	if (!State.bValid)
	{
		return nullptr;
	}

	if (!State.Resolver)
	{
		UMVVMViewModelCollectionObject* CollectionObject = EnsureCollection();
		if (!CollectionObject)
		{
			return nullptr;
		}
		FMVVMViewModelContext Context;
		Context.ContextClass = State.ViewModelClass.Get();
		Context.ContextName = State.Declaration.Context;
		return CollectionObject->FindViewModelInstance(Context);
	}

	EnsureProxyWidget();
	if (!ProxyWidget)
	{
		return nullptr;
	}

	UObject* Result = State.Resolver->CreateInstance(State.ViewModelClass.Get(), ProxyWidget.Get(), nullptr);
	if (Result && !Result->IsA(State.ViewModelClass.Get()))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: resolver of view model '%ls' returned '%ls', which is not a '%ls'", *State.Declaration.Name,
			*Result->GetClass()->GetPathName(), *State.ViewModelClass->GetPathName());
		State.Resolver->DestroyInstance(Result, nullptr);
		return nullptr;
	}
	return Result;
}

void FArcRmlModelBinding::ResolveDeclarations(bool bOnlyUnresolved)
{
	for (FDeclarationState& State : Declarations)
	{
		if (!State.bValid || (bOnlyUnresolved && State.ViewModel))
		{
			continue;
		}
		SetViewModel(State, ResolveViewModel(State));
	}
}

void FArcRmlModelBinding::SetViewModel(FDeclarationState& State, UObject* ViewModel)
{
	if (State.ViewModel == ViewModel || !Bindings)
	{
		return;
	}

	UObject* Previous = State.ViewModel;
	State.ViewModel = ViewModel;
	if (Previous && State.Resolver)
	{
		State.Resolver->DestroyInstance(Previous, nullptr);
	}
	Bindings->SetSource(State.SourceId, ViewModel);
}

UMVVMViewModelCollectionObject* FArcRmlModelBinding::EnsureCollection()
{
	if (UMVVMViewModelCollectionObject* Existing = Collection.Get())
	{
		return Existing;
	}

	ULocalPlayer* Player = LocalPlayer.Get();
	UGameInstance* GameInstance = Player ? Player->GetGameInstance() : nullptr;
	UMVVMGameSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UMVVMGameSubsystem>() : nullptr;
	UMVVMViewModelCollectionObject* CollectionObject = Subsystem ? Subsystem->GetViewModelCollection() : nullptr;
	if (CollectionObject)
	{
		Collection = CollectionObject;
		CollectionChangedHandle = CollectionObject->OnCollectionChanged().AddRaw(this, &FArcRmlModelBinding::HandleCollectionChanged);
	}
	return CollectionObject;
}

void FArcRmlModelBinding::EnsureProxyWidget()
{
	if (ProxyWidget)
	{
		return;
	}

	APlayerController* PlayerController = GetPlayerController();
	if (PlayerController && PlayerController->IsLocalPlayerController() && PlayerController->Player)
	{
		ProxyWidget = CreateWidget<UArcRmlResolverContextWidget>(PlayerController);
	}
}

APlayerController* FArcRmlModelBinding::GetPlayerController() const
{
	const ULocalPlayer* Player = LocalPlayer.Get();
	const UWorld* PlayerWorld = World.Get();
	return Player && PlayerWorld ? Player->GetPlayerController(PlayerWorld) : nullptr;
}

void FArcRmlModelBinding::BuildSource(const FArcRmlSourceId& SourceId, const FString& ModelName, const TArray<FArcRmlPathEntry>& Path, UClass* Class,
	UObject* Object)
{
	Bindings->AddSource(SourceId, Object);
	FSourceInfo& Info = SourceInfos.Add(SourceId);
	Info.ModelName = ModelName;
	Info.Path = Path;
	Info.Class = Class;

	const bool bCanNest = Path.Num() < ArcRmlModelBindingPrivate::MaxDepth;
	ArcRmlModelBindingPrivate::ForEachSourceField(Class,
		[this, &SourceId, &ModelName, &Path, Object, bCanNest](UE::FieldNotification::FFieldId FieldId, const UE::FieldNotification::FFieldVariant& Field)
		{
			Bindings->AddBinding(SourceId, FieldId,
				INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateRaw(this, &FArcRmlModelBinding::HandleFieldChanged, SourceId));
			if (!bCanNest)
			{
				return;
			}
	
			const FName FieldName = FieldId.GetName();
			const FProperty* ValueProperty = ArcRmlModelBindingPrivate::GetValueProperty(Field);
			const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(ValueProperty);
			if (ObjectProperty && ArcRmlModelBindingPrivate::IsViewModelClass(ObjectProperty->PropertyClass))
			{
				const FArcRmlSourceId ChildId = SourceId.MakeChild(FieldName);
				TArray<FArcRmlPathEntry> ChildPath = Path;
				ChildPath.Add(FArcRmlPathEntry{FieldName.ToString(), INDEX_NONE});
				UObject* ChildObject = Object ? EvaluateChildObject(Object, FieldName) : nullptr;
				BuildSource(ChildId, ModelName, ChildPath, ObjectProperty->PropertyClass, ChildObject);
				Bindings->AddDependency(ChildId, SourceId, FieldId,
					FArcRmlViewModelBindings::FEvaluateSourceDelegate::CreateRaw(this, &FArcRmlModelBinding::EvaluateChildObject, FieldName));
				return;
			}
	
			const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(ValueProperty);
			const FObjectPropertyBase* InnerProperty = ArrayProperty ? CastField<FObjectPropertyBase>(ArrayProperty->Inner) : nullptr;
			if (InnerProperty && ArcRmlModelBindingPrivate::IsViewModelClass(InnerProperty->PropertyClass))
			{
				Bindings->AddArrayDependency(SourceId, FieldId,
					FArcRmlViewModelBindings::FEvaluateArrayDelegate::CreateRaw(this, &FArcRmlModelBinding::EvaluateArrayElements, FieldName),
					FArcRmlViewModelBindings::FElementAddedDelegate::CreateRaw(this, &FArcRmlModelBinding::HandleArrayElementAdded, SourceId, FieldName,
						InnerProperty->PropertyClass.Get()));
			}
		});
}

UObject* FArcRmlModelBinding::EvaluateChildObject(UObject* ParentObject, FName FieldName)
{
	if (!ParentObject)
	{
		return nullptr;
	}

	UObject* Result = nullptr;
	const UE::FieldNotification::FFieldVariant Field = ArcRmlModelBindingPrivate::FindField(ParentObject->GetClass(), FieldName);
	ArcRmlModelBindingPrivate::ReadFieldValue(ParentObject, ParentObject->GetClass(), Field, [&Result](const FProperty& Property, const void* ValuePtr)
	{
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(&Property))
		{
			Result = ObjectProperty->GetObjectPropertyValue(ValuePtr);
		}
	});
	return Result;
}

void FArcRmlModelBinding::EvaluateArrayElements(UObject* ParentObject, TArray<UObject*>& OutElements, FName FieldName)
{
	if (!ParentObject)
	{
		return;
	}

	const UE::FieldNotification::FFieldVariant Field = ArcRmlModelBindingPrivate::FindField(ParentObject->GetClass(), FieldName);
	ArcRmlModelBindingPrivate::ReadFieldValue(ParentObject, ParentObject->GetClass(), Field, [&OutElements](const FProperty& Property, const void* ValuePtr)
	{
		const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property);
		const FObjectPropertyBase* InnerProperty = ArrayProperty ? CastField<FObjectPropertyBase>(ArrayProperty->Inner) : nullptr;
		if (!InnerProperty)
		{
			return;
		}
		FScriptArrayHelper Helper(ArrayProperty, ValuePtr);
		OutElements.Reserve(Helper.Num());
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			OutElements.Add(InnerProperty->GetObjectPropertyValue(Helper.GetRawPtr(Index)));
		}
	});
}

void FArcRmlModelBinding::HandleArrayElementAdded(FArcRmlSourceId ElementId, int32 Index, UObject* ElementObject, FArcRmlSourceId ParentId, FName FieldName,
	UClass* ElementClass)
{
	const FSourceInfo* ParentInfo = SourceInfos.Find(ParentId);
	if (!ParentInfo)
	{
		return;
	}

	const FString ModelName = ParentInfo->ModelName;
	TArray<FArcRmlPathEntry> ElementPath = ParentInfo->Path;
	ElementPath.Add(FArcRmlPathEntry{FieldName.ToString(), INDEX_NONE});
	ElementPath.Add(FArcRmlPathEntry{FString(), Index});
	BuildSource(ElementId, ModelName, ElementPath, ElementClass, ElementObject);
}

void FArcRmlModelBinding::HandleSourceRemoved(FArcRmlSourceId SourceId)
{
	SourceInfos.Remove(SourceId);
}

void FArcRmlModelBinding::HandleFieldChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId, FArcRmlSourceId SourceId)
{
	DirtyFields.Add(TPair<FArcRmlSourceId, FName>(SourceId, FieldId.GetName()));
#if !UE_BUILD_SHIPPING
	if (ArcRmlUIPerf::IsEnabled())
	{
		if (const FSourceInfo* Info = SourceInfos.Find(SourceId))
		{
			++ModelPerf.FindOrAdd(Info->ModelName).Notifications;
		}
	}
#endif
}

void FArcRmlModelBinding::HandleCollectionChanged()
{
	ResolveDeclarations(false);
}

FArcRmlValue FArcRmlModelBinding::ReadObject(UObject* Object, UClass* Class, int32 Depth, const FString* ValuePath)
{
	FArcRmlValue Result;
	Result.Kind = EArcRmlValueKind::Struct;
	if (!Class || Depth > ArcRmlModelBindingPrivate::MaxDepth)
	{
		return Result;
	}

	ArcRmlModelBindingPrivate::ForEachSourceField(Class,
		[this, &Result, Object, Class, Depth, ValuePath](UE::FieldNotification::FFieldId FieldId, const UE::FieldNotification::FFieldVariant& Field)
		{
			const FString FieldName = FieldId.GetName().ToString();
			const FString FieldPath = ValuePath ? ArcRmlModelBindingPrivate::AppendPath(*ValuePath, FArcRmlPathEntry{FieldName, INDEX_NONE}) : FString();
			Result.MemberNames.Add(FieldName);
			Result.Children.Add(ReadField(Object, Class, Field, Depth, ValuePath ? &FieldPath : nullptr));
		});
	return Result;
}

FArcRmlValue FArcRmlModelBinding::ReadField(UObject* Object, UClass* Class, const UE::FieldNotification::FFieldVariant& Field, int32 Depth, const FString* ValuePath)
{
	FArcRmlValue Value;
	const FString FieldKey = FString::Printf(TEXT("%s.%s"), *GetNameSafe(Class), *Field.GetFName().ToString());
	ArcRmlModelBindingPrivate::ReadFieldValue(Object, Class, Field, [this, &Value, Depth, &FieldKey, ValuePath](const FProperty& Property, const void* ValuePtr)
	{
		Value = ConvertValue(Property, ValuePtr, Depth, FieldKey, ValuePath);
	});
	return Value;
}

FArcRmlValue FArcRmlModelBinding::ConvertValue(const FProperty& Property, const void* ValuePtr, int32 Depth, const FString& FieldKey, const FString* ValuePath)
{
	FArcRmlValue Value;

	if (const FBoolProperty* BoolProperty = CastField<FBoolProperty>(&Property))
	{
		Value.Kind = EArcRmlValueKind::Bool;
		Value.bBool = BoolProperty->GetPropertyValue(ValuePtr);
		return Value;
	}

	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(&Property))
	{
		Value.Kind = EArcRmlValueKind::String;
		Value.String = EnumProperty->GetEnum()->GetNameStringByValue(EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr));
		return Value;
	}

	if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(&Property))
	{
		if (const UEnum* Enum = NumericProperty->GetIntPropertyEnum())
		{
			Value.Kind = EArcRmlValueKind::String;
			Value.String = Enum->GetNameStringByValue(NumericProperty->GetSignedIntPropertyValue(ValuePtr));
		}
		else if (NumericProperty->IsFloatingPoint())
		{
			Value.Kind = EArcRmlValueKind::Float;
			Value.Float = NumericProperty->GetFloatingPointPropertyValue(ValuePtr);
		}
		else
		{
			Value.Kind = EArcRmlValueKind::Int;
			Value.Int = NumericProperty->GetSignedIntPropertyValue(ValuePtr);
		}
		return Value;
	}

	if (const FStrProperty* StringProperty = CastField<FStrProperty>(&Property))
	{
		Value.Kind = EArcRmlValueKind::String;
		Value.String = StringProperty->GetPropertyValue(ValuePtr);
		return Value;
	}

	if (const FNameProperty* NameProperty = CastField<FNameProperty>(&Property))
	{
		Value.Kind = EArcRmlValueKind::String;
		Value.String = NameProperty->GetPropertyValue(ValuePtr).ToString();
		return Value;
	}

	if (const FTextProperty* TextProperty = CastField<FTextProperty>(&Property))
	{
		Value.Kind = EArcRmlValueKind::String;
		Value.String = TextProperty->GetPropertyValue(ValuePtr).ToString();
		return Value;
	}

	if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
	{
		const UScriptStruct* Struct = StructProperty->Struct;
		if (Struct == FGameplayTag::StaticStruct())
		{
			const FGameplayTag& Tag = *static_cast<const FGameplayTag*>(ValuePtr);
			Value.Kind = EArcRmlValueKind::String;
			Value.String = Tag.IsValid() ? Tag.ToString() : FString();
			return Value;
		}
		if (Struct == FGameplayTagContainer::StaticStruct())
		{
			const FGameplayTagContainer& Tags = *static_cast<const FGameplayTagContainer*>(ValuePtr);
			Value.Kind = EArcRmlValueKind::Array;
			for (const FGameplayTag& Tag : Tags.GetGameplayTagArray())
			{
				FArcRmlValue& Element = Value.Children.AddDefaulted_GetRef();
				Element.Kind = EArcRmlValueKind::String;
				Element.String = Tag.ToString();
			}
			return Value;
		}
		if (Struct == TBaseStructure<FLinearColor>::Get())
		{
			Value.Kind = EArcRmlValueKind::Colour;
			Value.Colour = static_cast<const FLinearColor*>(ValuePtr)->ToFColorSRGB();
			return Value;
		}
		if (Struct == TBaseStructure<FColor>::Get())
		{
			Value.Kind = EArcRmlValueKind::Colour;
			Value.Colour = *static_cast<const FColor*>(ValuePtr);
			return Value;
		}

		if (Struct == FArcRmlStructList::StaticStruct())
		{
			return ConvertStructList(*static_cast<const FArcRmlStructList*>(ValuePtr), Depth, FieldKey, ValuePath);
		}
		if (Struct == TBaseStructure<FSoftObjectPath>::Get())
		{
			Value.Kind = EArcRmlValueKind::String;
			const FSoftObjectPath& Path = *static_cast<const FSoftObjectPath*>(ValuePtr);
			Value.String = Path.IsNull() ? FString() : TEXT("asset:") + Path.ToString();
			return Value;
		}

		return ConvertStruct(*Struct, ValuePtr, Depth, FieldKey);
	}

	if (const FSoftObjectProperty* SoftProperty = CastField<FSoftObjectProperty>(&Property))
	{
		Value.Kind = EArcRmlValueKind::String;
		const FSoftObjectPtr& SoftPtr = SoftProperty->GetPropertyValue(ValuePtr);
		Value.String = SoftPtr.IsNull() ? FString() : TEXT("asset:") + SoftPtr.ToString();
		return Value;
	}

	if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(&Property))
	{
		if (ArcRmlModelBindingPrivate::IsViewModelClass(ObjectProperty->PropertyClass))
		{
			return ReadObject(ObjectProperty->GetObjectPropertyValue(ValuePtr), ObjectProperty->PropertyClass, Depth + 1, ValuePath);
		}
		const UTexture* Texture = Cast<UTexture>(ObjectProperty->GetObjectPropertyValue(ValuePtr));
		if (Texture != nullptr || ObjectProperty->PropertyClass->IsChildOf(UTexture::StaticClass()))
		{
			Value.Kind = EArcRmlValueKind::String;
			if (Texture != nullptr)
			{
				Value.String = TEXT("asset:") + FSoftObjectPath(Texture).ToString();
			}
			return Value;
		}
	}

	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(&Property))
	{
		Value.Kind = EArcRmlValueKind::Array;
		if (Depth < ArcRmlModelBindingPrivate::MaxDepth)
		{
			FScriptArrayHelper Helper(ArrayProperty, ValuePtr);
			Value.Children.Reserve(Helper.Num());
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				const FString ElementPath = ValuePath ? ArcRmlModelBindingPrivate::AppendPath(*ValuePath, FArcRmlPathEntry{FString(), Index}) : FString();
				Value.Children.Add(ConvertValue(*ArrayProperty->Inner, Helper.GetRawPtr(Index), Depth + 1, FieldKey, ValuePath ? &ElementPath : nullptr));
			}
		}
		return Value;
	}

	if (!WarnedFields.Contains(FieldKey))
	{
		WarnedFields.Add(FieldKey);
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: field '%ls' has unsupported type '%ls'; it binds as empty", *FieldKey, *Property.GetCPPType());
	}
	return Value;
}

FArcRmlValue FArcRmlModelBinding::ConvertStruct(const UScriptStruct& Struct, const void* ValuePtr, int32 Depth, const FString& FieldKey)
{
	FArcRmlValue Value;
	Value.Kind = EArcRmlValueKind::Struct;
	if (Depth < ArcRmlModelBindingPrivate::MaxDepth)
	{
		for (TFieldIterator<FProperty> MemberIt(&Struct); MemberIt; ++MemberIt)
		{
			Value.MemberNames.Add(MemberIt->GetFName().ToString());
			Value.Children.Add(ConvertValue(**MemberIt, MemberIt->ContainerPtrToValuePtr<void>(ValuePtr), Depth + 1, FieldKey, nullptr));
		}
	}
	return Value;
}

FArcRmlValue FArcRmlModelBinding::ConvertListRow(const FInstancedStruct& Row, int32 Depth, const FString& FieldKey)
{
	++ConvertedRows;
	return ConvertStruct(*Row.GetScriptStruct(), Row.GetMemory(), Depth + 1, FieldKey);
}

FArcRmlValue FArcRmlModelBinding::ConvertStructList(const FArcRmlStructList& List, int32 Depth, const FString& FieldKey, const FString* ValuePath)
{
	FArcRmlValue Value;
	Value.Kind = EArcRmlValueKind::Array;
	if (Depth >= ArcRmlModelBindingPrivate::MaxDepth)
	{
		if (ValuePath)
		{
			ListMirrors.Remove(*ValuePath);
		}
		return Value;
	}

	const TArray<FInstancedStruct>& Rows = List.GetRows();
	Value.Children.Reserve(Rows.Num());
	for (const FInstancedStruct& Row : Rows)
	{
		Value.Children.Add(ConvertListRow(Row, Depth, FieldKey));
	}

	if (ValuePath)
	{
		FListMirror& Mirror = ListMirrors.FindOrAdd(*ValuePath);
		Mirror.Keys = List.GetKeys();
		Mirror.Versions = List.GetRowVersions();
		Mirror.StructureVersion = List.GetStructureVersion();
	}
	return Value;
}

TArray<FArcRmlListOp> FArcRmlModelBinding::DiffStructList(FListMirror& Mirror, const FArcRmlStructList& List, int32 Depth, const FString& FieldKey)
{
	TArray<FArcRmlListOp> Ops;
	const TArray<FString>& NewKeys = List.GetKeys();
	const TArray<uint64>& NewVersions = List.GetRowVersions();
	const TArray<FInstancedStruct>& Rows = List.GetRows();
	const int32 NewCount = NewKeys.Num();

	if (Mirror.StructureVersion == List.GetStructureVersion() && Mirror.Keys.Num() == NewCount)
	{
		for (int32 Index = 0; Index < NewCount; ++Index)
		{
			if (Mirror.Versions[Index] != NewVersions[Index])
			{
				FArcRmlListOp& Op = Ops.AddDefaulted_GetRef();
				Op.Type = EArcRmlListOpType::Update;
				Op.Index = Index;
				Op.Value = ConvertListRow(Rows[Index], Depth, FieldKey);
				Mirror.Versions[Index] = NewVersions[Index];
			}
		}
		return Ops;
	}

	TMap<FString, int32> NewIndexByKey;
	NewIndexByKey.Reserve(NewCount);
	for (int32 Index = 0; Index < NewCount; ++Index)
	{
		NewIndexByKey.Add(NewKeys[Index], Index);
	}

	TArray<int32> Working;
	Working.Reserve(NewCount);
	TArray<uint64> OldVersions;
	OldVersions.Init(0, NewCount);
	TBitArray<> Existed(false, NewCount);
	TArray<int32> RemovedIndices;
	for (int32 OldIndex = 0; OldIndex < Mirror.Keys.Num(); ++OldIndex)
	{
		if (const int32* NewIndex = NewIndexByKey.Find(Mirror.Keys[OldIndex]))
		{
			Working.Add(*NewIndex);
			OldVersions[*NewIndex] = Mirror.Versions[OldIndex];
			Existed[*NewIndex] = true;
		}
		else
		{
			RemovedIndices.Add(OldIndex);
		}
	}
	for (int32 RemovedIt = RemovedIndices.Num() - 1; RemovedIt >= 0; --RemovedIt)
	{
		FArcRmlListOp& Op = Ops.AddDefaulted_GetRef();
		Op.Type = EArcRmlListOpType::Remove;
		Op.Index = RemovedIndices[RemovedIt];
	}

	const TBitArray<> Stays = ArcRmlModelBindingPrivate::FindLongestIncreasing(Working, NewCount);
	for (int32 NewIndex = NewCount - 1; NewIndex >= 0; --NewIndex)
	{
		const bool bExisted = Existed[NewIndex];
		if (bExisted && Stays[NewIndex])
		{
			continue;
		}

		const int32 AnchorPosition = NewIndex == NewCount - 1 ? Working.Num() : Working.IndexOfByKey(NewIndex + 1);
		if (!bExisted)
		{
			FArcRmlListOp& Op = Ops.AddDefaulted_GetRef();
			Op.Type = EArcRmlListOpType::Insert;
			Op.Index = AnchorPosition;
			Op.Value = ConvertListRow(Rows[NewIndex], Depth, FieldKey);
			Working.Insert(NewIndex, AnchorPosition);
			continue;
		}

		const int32 From = Working.IndexOfByKey(NewIndex);
		const int32 To = From < AnchorPosition ? AnchorPosition - 1 : AnchorPosition;
		if (From != To)
		{
			FArcRmlListOp& Op = Ops.AddDefaulted_GetRef();
			Op.Type = EArcRmlListOpType::Move;
			Op.Index = From;
			Op.ToIndex = To;
			Working.RemoveAt(From);
			Working.Insert(NewIndex, To);
		}
	}

	for (int32 NewIndex = 0; NewIndex < NewCount; ++NewIndex)
	{
		if (Existed[NewIndex] && OldVersions[NewIndex] != NewVersions[NewIndex])
		{
			FArcRmlListOp& Op = Ops.AddDefaulted_GetRef();
			Op.Type = EArcRmlListOpType::Update;
			Op.Index = NewIndex;
			Op.Value = ConvertListRow(Rows[NewIndex], Depth, FieldKey);
		}
	}

	Mirror.Keys = NewKeys;
	Mirror.Versions = NewVersions;
	Mirror.StructureVersion = List.GetStructureVersion();
	return Ops;
}

void FArcRmlModelBinding::SetOnDocumentAction(FArcRmlOnDocumentAction&& InOnDocumentAction)
{
	OnDocumentAction = MoveTemp(InOnDocumentAction);
}

void FArcRmlModelBinding::SetEmbeddedPath(const FString& Path)
{
	check(IsInGameThread());
	bPublishesEmbedded = true;
	EmbeddedPath = Path;
	if (!bStarted)
	{
		return;
	}

	const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin();
	if (!PinnedThread.IsValid())
	{
		return;
	}

	for (const FDeclarationState& State : Declarations)
	{
		if (!State.bEmbeddedFieldBound)
		{
			continue;
		}

		FArcRmlValue Value;
		Value.Kind = EArcRmlValueKind::String;
		Value.String = EmbeddedPath;
		TArray<FArcRmlPathEntry> FieldPath;
		FieldPath.Add(FArcRmlPathEntry{ArcRmlModelBindingPrivate::EmbeddedFieldName, INDEX_NONE});
		PinnedThread->ApplyModelDelta(ViewId, State.Declaration.Name, MoveTemp(FieldPath), MoveTemp(Value));
	}
}

bool FArcRmlModelBinding::DeclaresModel(const FString& ModelName) const
{
	return Declarations.ContainsByPredicate([&ModelName](const FDeclarationState& Candidate) -> bool
	{
		return Candidate.Declaration.Name == ModelName;
	});
}

bool FArcRmlModelBinding::HandleDocumentAction(const FArcRmlViewModelCall& Call)
{
	if (Call.Arguments.Num() != 1 || Call.Arguments[0].Kind != EArcRmlValueKind::String)
	{
		return false;
	}

	const FString& Action = Call.Arguments[0].String;
	EArcRmlDocumentAction DocumentAction = EArcRmlDocumentAction::Close;
	FString Target;
	if (Action.StartsWith(ArcRmlModelBindingPrivate::OpenActionPrefix, ESearchCase::CaseSensitive))
	{
		DocumentAction = EArcRmlDocumentAction::Open;
		Target = Action.RightChop(FCString::Strlen(ArcRmlModelBindingPrivate::OpenActionPrefix)).TrimStartAndEnd();
		if (Target.IsEmpty())
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: action '%ls' on '%ls' names no document", *Action, *Call.ModelName);
			return true;
		}
	}
	else if (!Action.Equals(ArcRmlModelBindingPrivate::CloseActionName, ESearchCase::CaseSensitive))
	{
		return false;
	}

	if (!OnDocumentAction.ExecuteIfBound(DocumentAction, Target))
	{
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: action '%ls' on '%ls' has no document host", *Action, *Call.ModelName);
	}
	return true;
}

void FArcRmlModelBinding::HandleViewModelCall(const FArcRmlViewModelCall& Call)
{
	check(IsInGameThread());
	if (Call.bRoutedAction && HandleDocumentAction(Call))
	{
		return;
	}

	const FDeclarationState* State = Declarations.FindByPredicate([&Call](const FDeclarationState& Candidate) -> bool
	{
		return Candidate.Declaration.Name == Call.ModelName;
	});
	if (State && (State->Declaration.IsPreview() || !bHasPlayer))
	{
		if (Call.bRoutedAction && Call.Arguments.Num() == 1)
		{
			UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: [preview] action %ls on '%ls'", *Call.Arguments[0].String, *Call.ModelName);
			return;
		}
		LogPreviewCall(Call);
		return;
	}
	if (!State || !State->bValid || !Bindings)
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: call('%ls', '%ls') targets unknown view model '%ls'", *Call.ChildPath, *Call.FunctionName, *Call.ModelName);
		return;
	}

	const FArcRmlSourceId TargetId = Call.ChildPath.IsEmpty()
		? State->SourceId
		: FArcRmlSourceId::Make(FString::Printf(TEXT("%s.%s"), *State->Declaration.Name, *Call.ChildPath));
	UObject* Target = Bindings->GetSourceObject(TargetId);
	if (!Target)
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: call('%ls', '%ls') on '%ls' has no live view model", *Call.ChildPath, *Call.FunctionName, *Call.ModelName);
		return;
	}

	UFunction* Function = Target->FindFunction(FName(*Call.FunctionName));
	if (!Function || !UE::MVVM::BindingHelper::IsValidForEventBinding(Function))
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: '%ls' is not a callable event function on '%ls'", *Call.FunctionName, *Target->GetClass()->GetName());
		return;
	}

	uint8* Params = static_cast<uint8*>(FMemory::Malloc(FMath::Max<int32>(Function->ParmsSize, 1), Function->GetMinAlignment()));
	Function->InitializeStruct(Params);
	TArray<FString> ResolvedArguments;
	if (FillParameters(*Function, Params, Call, ResolvedArguments))
	{
		UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI: call('%ls', '%ls') on '%ls' dispatched to %ls with [%ls]", *Call.ChildPath, *Call.FunctionName, *Call.ModelName,
			*Target->GetPathName(), *FString::Join(ResolvedArguments, TEXT(", ")));
		Target->ProcessEvent(Function, Params);
	}
	Function->DestroyStruct(Params);
	FMemory::Free(Params);
}

void FArcRmlModelBinding::LogPreviewCall(const FArcRmlViewModelCall& Call) const
{
	TArray<FString> Parts;
	Parts.Add(FString::Printf(TEXT("'%s'"), *Call.ChildPath));
	Parts.Add(FString::Printf(TEXT("'%s'"), *Call.FunctionName));
	for (const FArcRmlValue& Argument : Call.Arguments)
	{
		FArcRmlValue Resolved;
		if (!ResolveArgument(Argument, Call, Resolved))
		{
			Resolved = Argument;
		}
		const FString Text = ArcRmlModelBindingPrivate::ArgumentToString(Resolved);
		Parts.Add(Resolved.Kind == EArcRmlValueKind::String ? FString::Printf(TEXT("'%s'"), *Text) : Text);
	}
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: [preview] call(%ls) on '%ls'", *FString::Join(Parts, TEXT(", ")), *Call.ModelName);
}

bool FArcRmlModelBinding::FillParameters(UFunction& Function, uint8* Params, const FArcRmlViewModelCall& Call, TArray<FString>& OutResolvedArguments)
{
	int32 ArgumentIndex = 0;
	for (TFieldIterator<FProperty> It(&Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Parameter = *It;
		if (Parameter->HasAnyPropertyFlags(CPF_ReturnParm) || (Parameter->HasAnyPropertyFlags(CPF_OutParm) && !Parameter->HasAnyPropertyFlags(CPF_ReferenceParm)))
		{
			continue;
		}

		void* ValuePtr = Parameter->ContainerPtrToValuePtr<void>(Params);
		if (const FObjectPropertyBase* ObjectParameter = CastField<FObjectPropertyBase>(Parameter))
		{
			if (!FillObjectParameter(*ObjectParameter, ValuePtr, Call, ArgumentIndex, OutResolvedArguments))
			{
				return false;
			}
			continue;
		}

		FArcRmlValue Argument;
		if (ArgumentIndex >= Call.Arguments.Num() || !ResolveArgument(Call.Arguments[ArgumentIndex], Call, Argument))
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: '%ls' has no usable argument for parameter '%ls'", *Call.FunctionName, *Parameter->GetName());
			return false;
		}
		++ArgumentIndex;
		OutResolvedArguments.Add(ArcRmlModelBindingPrivate::ArgumentToString(Argument));

		if (Argument.Kind == EArcRmlValueKind::String && Argument.String.StartsWith(ArcRmlModelBindingPrivate::ViewModelArgumentPrefix, ESearchCase::CaseSensitive))
		{
			UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' argument '%ls' is a view model, but parameter '%ls' is not an object", *Call.FunctionName,
				*Argument.String, *Parameter->GetName());
			return false;
		}

		if (!ArcRmlModelBindingPrivate::ConvertArgument(*Parameter, ValuePtr, Argument))
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: '%ls' argument '%ls' does not convert to parameter '%ls'", *Call.FunctionName,
				*ArcRmlModelBindingPrivate::ArgumentToString(Argument), *Parameter->GetName());
			return false;
		}
	}

	if (ArgumentIndex != Call.Arguments.Num())
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: '%ls' received %d arguments but takes %d", *Call.FunctionName, Call.Arguments.Num(), ArgumentIndex);
		return false;
	}
	return true;
}

bool FArcRmlModelBinding::ResolveArgument(const FArcRmlValue& Argument, const FArcRmlViewModelCall& Call, FArcRmlValue& OutArgument) const
{
	OutArgument = Argument;
	if (Argument.Kind != EArcRmlValueKind::String)
	{
		return true;
	}

	const TMap<FString, FString>* Source = nullptr;
	FString Key;
	if (Argument.String.StartsWith(ArcRmlModelBindingPrivate::DragArgumentPrefix, ESearchCase::CaseSensitive))
	{
		Source = &Call.DragData;
		Key = Argument.String.RightChop(FCString::Strlen(ArcRmlModelBindingPrivate::DragArgumentPrefix));
	}
	else if (Argument.String.StartsWith(ArcRmlModelBindingPrivate::TargetArgumentPrefix, ESearchCase::CaseSensitive))
	{
		Source = &Call.ElementData;
		Key = Argument.String.RightChop(FCString::Strlen(ArcRmlModelBindingPrivate::TargetArgumentPrefix));
	}
	else
	{
		return true;
	}

	const FString* Found = Source->Find(Key);
	if (!Found)
	{
		return false;
	}
	OutArgument.String = *Found;
	return true;
}

bool FArcRmlModelBinding::FillObjectParameter(const FObjectPropertyBase& Parameter, void* ValuePtr, const FArcRmlViewModelCall& Call, int32& InOutArgumentIndex,
	TArray<FString>& OutResolvedArguments)
{
	if (Parameter.GetFName() == ArcRmlModelBindingPrivate::WorldContextParameterName)
	{
		APlayerController* PlayerController = GetPlayerController();
		if (!PlayerController || !PlayerController->IsA(Parameter.PropertyClass))
		{
			UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: '%ls' parameter '%ls' is a world context, which this view cannot supply", *Call.FunctionName,
				*Parameter.GetName());
			return false;
		}
		Parameter.SetObjectPropertyValue(ValuePtr, PlayerController);
		return true;
	}

	FArcRmlValue Argument;
	if (InOutArgumentIndex >= Call.Arguments.Num() || !ResolveArgument(Call.Arguments[InOutArgumentIndex], Call, Argument)
		|| Argument.Kind != EArcRmlValueKind::String || !Argument.String.StartsWith(ArcRmlModelBindingPrivate::ViewModelArgumentPrefix, ESearchCase::CaseSensitive))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' object parameter '%ls' takes a 'vm:<path>' argument", *Call.FunctionName, *Parameter.GetName());
		return false;
	}
	++InOutArgumentIndex;

	const FString Path = Argument.String.RightChop(FCString::Strlen(ArcRmlModelBindingPrivate::ViewModelArgumentPrefix));
	if (Path.IsEmpty())
	{
		Parameter.SetObjectPropertyValue(ValuePtr, nullptr);
		OutResolvedArguments.Add(TEXT("null"));
		return true;
	}

	UObject* Object = Bindings->GetSourceObject(FArcRmlSourceId::Make(FString::Printf(TEXT("%s.%s"), *Call.ModelName, *Path)));
	if (!Object)
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' argument '%ls' resolves to no live view model in '%ls'", *Call.FunctionName, *Argument.String,
			*Call.ModelName);
		return false;
	}
	if (!Object->IsA(Parameter.PropertyClass))
	{
		UE_LOGF(LogArcRmlUI, Error, "ArcRmlUI: '%ls' argument '%ls' is a '%ls', but parameter '%ls' takes '%ls'", *Call.FunctionName, *Argument.String,
			*Object->GetClass()->GetName(), *Parameter.GetName(), *Parameter.PropertyClass->GetName());
		return false;
	}
	Parameter.SetObjectPropertyValue(ValuePtr, Object);
	OutResolvedArguments.Add(Object->GetPathName());
	return true;
}

void FArcRmlModelBinding::SetPaused(bool bInPaused)
{
	bPaused = bInPaused;
	if (!bPaused)
	{
		Flush();
	}
}

bool FArcRmlModelBinding::Tick(float DeltaTime)
{
	if (!bPaused)
	{
		Flush();
	}
	ARCRMLUI_PERF(ReportPerf());

	RetryElapsed += DeltaTime;
	if (bHasPlayer && RetryElapsed >= ArcRmlModelBindingPrivate::RetryInterval)
	{
		RetryElapsed = 0.0f;
		EnsureCollection();
		ResolveDeclarations(true);
		if (!bPaused)
		{
			Flush();
		}
	}
	return true;
}

void FArcRmlModelBinding::Flush()
{
	if (!Bindings)
	{
		return;
	}

	const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin();
	if (!PinnedThread.IsValid())
	{
		DirtyFields.Reset();
		return;
	}

	FlushResolvedFlags(*PinnedThread, false);

	TSet<TPair<FArcRmlSourceId, FName>> Pending = MoveTemp(DirtyFields);
	DirtyFields.Reset();
	for (const TPair<FArcRmlSourceId, FName>& Entry : Pending)
	{
		const FSourceInfo* Info = SourceInfos.Find(Entry.Key);
		if (!Info)
		{
			continue;
		}

		ARCRMLUI_PERF(const uint64 ConvertStartCycles = FPlatformTime::Cycles64());
		const UE::FieldNotification::FFieldVariant Field = ArcRmlModelBindingPrivate::FindField(Info->Class, Entry.Value);
		if (!Field.IsValid())
		{
			continue;
		}

		UObject* Object = Bindings->GetSourceObject(Entry.Key);
		TArray<FArcRmlPathEntry> Path = Info->Path;
		Path.Add(FArcRmlPathEntry{Entry.Value.ToString(), INDEX_NONE});
		const FString ValuePath = ArcRmlModelBindingPrivate::MakePath(Info->ModelName, Path);
		ConvertedRows = 0;

		FListMirror* Mirror = ArcRmlModelBindingPrivate::IsStructList(ArcRmlModelBindingPrivate::GetValueProperty(Field)) ? ListMirrors.Find(ValuePath) : nullptr;
		TArray<FArcRmlListOp> Ops;
		FArcRmlValue Value;
		if (Mirror)
		{
			const FString FieldKey = FString::Printf(TEXT("%s.%s"), *GetNameSafe(Info->Class), *Field.GetFName().ToString());
			const int32 Depth = Info->Path.Num();
			ArcRmlModelBindingPrivate::ReadFieldValue(Object, Info->Class, Field, [this, Mirror, &Ops, Depth, &FieldKey](const FProperty& Property, const void* ValuePtr)
			{
				Ops = DiffStructList(*Mirror, *static_cast<const FArcRmlStructList*>(ValuePtr), Depth, FieldKey);
			});
		}
		else
		{
			Value = ReadField(Object, Info->Class, Field, Info->Path.Num(), &ValuePath);
		}
#if !UE_BUILD_SHIPPING
		if (ArcRmlUIPerf::IsEnabled())
		{
			FModelPerf& Perf = ModelPerf.FindOrAdd(Info->ModelName);
			Perf.Convert.Add(ArcRmlUIPerf::SinceMs(ConvertStartCycles));
			Perf.ListOps += Ops.Num();
			Perf.ConvertedRows += ConvertedRows;
			if (!Mirror || Ops.Num() > 0)
			{
				++Perf.Deltas;
			}
		}
#endif
		if (!Mirror)
		{
			PinnedThread->ApplyModelDelta(ViewId, Info->ModelName, MoveTemp(Path), MoveTemp(Value));
		}
		else if (Ops.Num() > 0)
		{
			PinnedThread->ApplyModelListOps(ViewId, Info->ModelName, MoveTemp(Path), MoveTemp(Ops));
		}
	}

	FlushResolvedFlags(*PinnedThread, true);
}

void FArcRmlModelBinding::FlushResolvedFlags(FArcRmlUIThread& UIThread, bool bResolved)
{
	for (FDeclarationState& State : Declarations)
	{
		const bool bIsResolved = State.ViewModel != nullptr;
		if (!State.bResolvedFlagBound || bIsResolved != bResolved || State.bSentResolved == bIsResolved)
		{
			continue;
		}

		State.bSentResolved = bIsResolved;
		FArcRmlValue Value;
		Value.Kind = EArcRmlValueKind::Bool;
		Value.bBool = bIsResolved;
		TArray<FArcRmlPathEntry> Path;
		Path.Add(FArcRmlPathEntry{ArcRmlModelBindingPrivate::ResolvedFieldName, INDEX_NONE});
		UIThread.ApplyModelDelta(ViewId, State.Declaration.Name, MoveTemp(Path), MoveTemp(Value));
#if !UE_BUILD_SHIPPING
		if (ArcRmlUIPerf::IsEnabled())
		{
			++ModelPerf.FindOrAdd(State.Declaration.Name).Deltas;
		}
#endif
	}
}

#if !UE_BUILD_SHIPPING
void FArcRmlModelBinding::ReportPerf()
{
	double ElapsedSeconds = 0.0;
	if (!PerfClock.ShouldReport(ElapsedSeconds))
	{
		return;
	}

	if (ArcRmlUIPerf::IsEnabled())
	{
		for (const TPair<FString, FModelPerf>& Pair : ModelPerf)
		{
			const FModelPerf& Perf = Pair.Value;
			if (Perf.Notifications == 0 && Perf.Deltas == 0)
			{
				continue;
			}
			UE_LOGF(LogArcRmlUI, Verbose, "ArcRmlUI perf: view %u '%ls' model '%ls' %.1f s: notifications %d, deltas %d, list ops %d, rows converted %d, convert total %.2f ms avg %.3f max %.3f ms",
				ViewId, *DocumentPath, *Pair.Key, ElapsedSeconds, Perf.Notifications, Perf.Deltas, Perf.ListOps, Perf.ConvertedRows, Perf.Convert.TotalMs,
				Perf.Convert.GetAverageMs(), Perf.Convert.MaxMs);
		}
	}
	ModelPerf.Reset();
}
#endif
