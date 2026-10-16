// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from Engine/Plugins/Experimental/SlateModelViewViewModel/Source/SlateMVVM/Private/SlateViewModelBindings.cpp.

#include "MVVM/ArcRmlViewModelBindings.h"

namespace ArcRmlMVVM
{
	using FFieldId = UE::FieldNotification::FFieldId;

	class FBinding
	{
	public:
		FBinding(FFieldId WhenChanged, INotifyFieldValueChanged::FFieldValueChangedDelegate Delegate)
			: CompleteDelegate(MoveTemp(Delegate))
			, FieldId(WhenChanged)
		{
		}

		FBinding(FFieldId WhenChanged, FSimpleDelegate Delegate)
			: SimpleDelegate(MoveTemp(Delegate))
			, FieldId(WhenChanged)
		{
		}

		FFieldId GetFieldId() const
		{
			return FieldId;
		}

		void Execute(UObject* Object) const
		{
			SimpleDelegate.ExecuteIfBound();
			CompleteDelegate.ExecuteIfBound(Object, FieldId);
		}

	private:
		INotifyFieldValueChanged::FFieldValueChangedDelegate CompleteDelegate;
		FSimpleDelegate SimpleDelegate;
		FFieldId FieldId;
	};

	class FSource
	{
	public:
		explicit FSource(UObject* InSource)
			: Source(InSource)
		{
		}

		TScriptInterface<INotifyFieldValueChanged> GetSource() const
		{
			UObject* FoundObject = Source.Get();
			return FoundObject ? TScriptInterface<INotifyFieldValueChanged>(FoundObject) : TScriptInterface<INotifyFieldValueChanged>();
		}

		FWeakObjectPtr Source;
		FArcRmlSourceId ParentId;
		int32 Depth = 0;
		TArray<TUniquePtr<FBinding>> Bindings;
		TMap<FFieldId, FDelegateHandle> Delegates;
	};

	struct FDependency
	{
		FFieldId ParentFieldId;
		FArcRmlSourceId ParentId;
		FArcRmlSourceId ChildId;
		FArcRmlViewModelBindings::FEvaluateSourceDelegate SourceDelegate;
	};

	struct FArrayDependency
	{
		FFieldId ParentFieldId;
		FArcRmlSourceId ParentId;
		FArcRmlViewModelBindings::FEvaluateArrayDelegate EvaluateDelegate;
		FArcRmlViewModelBindings::FElementAddedDelegate ElementAddedDelegate;
		int32 NumElements = 0;
	};

	struct FViewModelBindingsImpl
	{
		~FViewModelBindingsImpl();

		FSource* FindSource(const FArcRmlSourceId& SourceId) const;
		FArrayDependency* FindArrayDependency(const FArcRmlSourceId& ParentId, FFieldId FieldId) const;
		void Test_ValidFieldId(TScriptInterface<INotifyFieldValueChanged> Interface, FFieldId WhenField) const;

		FArcRmlViewModelBindings::FBuilder AddSource(FArcRmlSourceId SourceId, TScriptInterface<INotifyFieldValueChanged> Value);
		void RemoveSource(FArcRmlSourceId SourceId);
		void SetSource(FArcRmlSourceId SourceId, TScriptInterface<INotifyFieldValueChanged> Value);
		void AddBinding(FArcRmlSourceId SourceId, FFieldId WhenFieldId, INotifyFieldValueChanged::FFieldValueChangedDelegate Delegate);
		void AddBinding(FArcRmlSourceId SourceId, FFieldId WhenFieldId, FSimpleDelegate Delegate);
		void RemoveAllBindings(FArcRmlSourceId SourceId);
		void AddDependency(FArcRmlSourceId ToEvaluate, FArcRmlSourceId WhenChanged, FArcRmlViewModelBindings::FEvaluateSourceDelegate EvaluateDelegate);
		void AddDependency(FArcRmlSourceId ToEvaluate, FArcRmlSourceId WhenChanged, FFieldId WhenField, FArcRmlViewModelBindings::FEvaluateSourceDelegate EvaluateDelegate);
		void AddArrayDependency(FArcRmlSourceId WhenChanged, FFieldId WhenField, FArcRmlViewModelBindings::FEvaluateArrayDelegate EvaluateDelegate,
			FArcRmlViewModelBindings::FElementAddedDelegate ElementAddedDelegate);
		void SortDependencies();
		void Execute();
		void Execute(FArcRmlSourceId SourceId);

		FArcRmlViewModelBindings::FSourceRemovedDelegate OnSourceRemoved;

	private:
		void AddBindingImpl(FArcRmlSourceId SourceId, TUniquePtr<FBinding>&& Binding);
		void UnbindBindingsImpl(FSource& Source);
		void BindFieldDelegates(const FArcRmlSourceId& SourceId, FSource& Source);
		void RegisterFieldDelegate(const FArcRmlSourceId& SourceId, FSource& Source, FFieldId FieldId);
		void EvaluateDependents(const FArcRmlSourceId& SourceId, UObject* ParentObject, FFieldId ChangedField);
		void EvaluateArrayDependency(const FArcRmlSourceId& ParentId, FFieldId FieldId, UObject* ParentObject);
		void ExecuteBindings(const FArcRmlSourceId& SourceId, TOptional<FFieldId> OnlyField);
		void HandleFieldChanged(UObject* SourceObject, FFieldId FieldId, FArcRmlSourceId SourceId);

		TMap<FArcRmlSourceId, TUniquePtr<FSource>> Sources;
		TArray<TUniquePtr<FDependency>> Dependencies;
		TArray<TUniquePtr<FArrayDependency>> ArrayDependencies;
		bool bDependenciesDirty = true;
	};

	FViewModelBindingsImpl::~FViewModelBindingsImpl()
	{
		for (TPair<FArcRmlSourceId, TUniquePtr<FSource>>& SourcePair : Sources)
		{
			UnbindBindingsImpl(*SourcePair.Value);
		}
	}

	FSource* FViewModelBindingsImpl::FindSource(const FArcRmlSourceId& SourceId) const
	{
		const TUniquePtr<FSource>* Source = Sources.Find(SourceId);
		return Source ? Source->Get() : nullptr;
	}

	FArrayDependency* FViewModelBindingsImpl::FindArrayDependency(const FArcRmlSourceId& ParentId, FFieldId FieldId) const
	{
		for (const TUniquePtr<FArrayDependency>& Dependency : ArrayDependencies)
		{
			if (Dependency->ParentId == ParentId && Dependency->ParentFieldId == FieldId)
			{
				return Dependency.Get();
			}
		}
		return nullptr;
	}

	void FViewModelBindingsImpl::Test_ValidFieldId(TScriptInterface<INotifyFieldValueChanged> Interface, FFieldId WhenField) const
	{
#if DO_CHECK
		if (Interface.GetInterface() && Interface.GetObject())
		{
			const FFieldId FoundFieldId = Interface->GetFieldNotificationDescriptor().GetField(Interface.GetObject()->GetClass(), WhenField.GetName());
			checkf(FoundFieldId.IsValid() && FoundFieldId == WhenField, TEXT("The FieldId is not part of that class."));
		}
#endif
	}

	FArcRmlViewModelBindings::FBuilder FViewModelBindingsImpl::AddSource(FArcRmlSourceId SourceId, TScriptInterface<INotifyFieldValueChanged> Value)
	{
		if (!SourceId.IsValid())
		{
			return FArcRmlViewModelBindings::FBuilder(*this, FArcRmlSourceId());
		}

		if (FindSource(SourceId))
		{
			SetSource(SourceId, Value);
			return FArcRmlViewModelBindings::FBuilder(*this, SourceId);
		}

		UObject* Object = Value.GetObject() && Value.GetInterface() ? Value.GetObject() : nullptr;
		Sources.Emplace(SourceId, MakeUnique<FSource>(Object));
		bDependenciesDirty = true;
		return FArcRmlViewModelBindings::FBuilder(*this, SourceId);
	}

	void FViewModelBindingsImpl::RemoveSource(FArcRmlSourceId SourceId)
	{
		TUniquePtr<FSource> Removed;
		if (!Sources.RemoveAndCopyValue(SourceId, Removed))
		{
			return;
		}
		UnbindBindingsImpl(*Removed);
		bDependenciesDirty = true;

		TArray<FArcRmlSourceId> ChildIds;
		for (const TUniquePtr<FDependency>& Dependency : Dependencies)
		{
			if (Dependency->ParentId == SourceId)
			{
				ChildIds.Add(Dependency->ChildId);
			}
		}
		for (const TUniquePtr<FArrayDependency>& Dependency : ArrayDependencies)
		{
			if (Dependency->ParentId == SourceId)
			{
				for (int32 Index = 0; Index < Dependency->NumElements; ++Index)
				{
					ChildIds.Add(SourceId.MakeElement(Dependency->ParentFieldId.GetName(), Index));
				}
			}
		}

		Dependencies.RemoveAll([&SourceId](const TUniquePtr<FDependency>& Dependency) -> bool
		{
			return Dependency->ParentId == SourceId || Dependency->ChildId == SourceId;
		});
		ArrayDependencies.RemoveAll([&SourceId](const TUniquePtr<FArrayDependency>& Dependency) -> bool
		{
			return Dependency->ParentId == SourceId;
		});

		OnSourceRemoved.ExecuteIfBound(SourceId);

		for (const FArcRmlSourceId& ChildId : ChildIds)
		{
			RemoveSource(ChildId);
		}
	}

	void FViewModelBindingsImpl::SetSource(FArcRmlSourceId SourceId, TScriptInterface<INotifyFieldValueChanged> NewValue)
	{
		FSource* FoundSource = FindSource(SourceId);
		if (!FoundSource)
		{
			return;
		}

		UObject* NewSourceObject = NewValue.GetObject() && NewValue.GetInterface() ? NewValue.GetObject() : nullptr;
		if (FoundSource->Source.Get() == NewSourceObject && (NewSourceObject != nullptr || FoundSource->Delegates.Num() == 0))
		{
			return;
		}

		UnbindBindingsImpl(*FoundSource);
		FoundSource->Source = NewSourceObject;
		BindFieldDelegates(SourceId, *FoundSource);

		EvaluateDependents(SourceId, NewSourceObject, FFieldId());
		ExecuteBindings(SourceId, TOptional<FFieldId>());
	}

	void FViewModelBindingsImpl::UnbindBindingsImpl(FSource& Source)
	{
		TScriptInterface<INotifyFieldValueChanged> Interface = Source.GetSource();
		if (Interface.GetObject() && Interface.GetInterface())
		{
			for (const TPair<FFieldId, FDelegateHandle>& DelegatePair : Source.Delegates)
			{
				Interface->RemoveFieldValueChangedDelegate(DelegatePair.Key, DelegatePair.Value);
			}
		}
		Source.Delegates.Reset();
	}

	void FViewModelBindingsImpl::BindFieldDelegates(const FArcRmlSourceId& SourceId, FSource& Source)
	{
		for (const TUniquePtr<FBinding>& Binding : Source.Bindings)
		{
			RegisterFieldDelegate(SourceId, Source, Binding->GetFieldId());
		}
		for (const TUniquePtr<FDependency>& Dependency : Dependencies)
		{
			if (Dependency->ParentId == SourceId && Dependency->ParentFieldId.IsValid())
			{
				RegisterFieldDelegate(SourceId, Source, Dependency->ParentFieldId);
			}
		}
		for (const TUniquePtr<FArrayDependency>& Dependency : ArrayDependencies)
		{
			if (Dependency->ParentId == SourceId)
			{
				RegisterFieldDelegate(SourceId, Source, Dependency->ParentFieldId);
			}
		}
	}

	void FViewModelBindingsImpl::RegisterFieldDelegate(const FArcRmlSourceId& SourceId, FSource& Source, FFieldId FieldId)
	{
		if (Source.Delegates.Contains(FieldId))
		{
			return;
		}

		TScriptInterface<INotifyFieldValueChanged> Interface = Source.GetSource();
		if (!Interface.GetObject() || !Interface.GetInterface())
		{
			return;
		}

		// Delegates carry the stable source id, so a notification routes to the source slot even after the object in it was swapped.
		const FDelegateHandle Handle = Interface->AddFieldValueChangedDelegate(FieldId,
			INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateRaw(this, &FViewModelBindingsImpl::HandleFieldChanged, SourceId));
		Source.Delegates.Emplace(FieldId, Handle);
	}

	void FViewModelBindingsImpl::AddBindingImpl(FArcRmlSourceId SourceId, TUniquePtr<FBinding>&& Binding)
	{
		FSource* Source = FindSource(SourceId);
		if (!ensure(Source))
		{
			return;
		}

		const FFieldId FieldId = Binding->GetFieldId();
		Test_ValidFieldId(Source->GetSource(), FieldId);
		Source->Bindings.Add(MoveTemp(Binding));
		RegisterFieldDelegate(SourceId, *Source, FieldId);
	}

	void FViewModelBindingsImpl::AddBinding(FArcRmlSourceId SourceId, FFieldId WhenFieldId, INotifyFieldValueChanged::FFieldValueChangedDelegate Delegate)
	{
		AddBindingImpl(SourceId, MakeUnique<FBinding>(WhenFieldId, MoveTemp(Delegate)));
	}

	void FViewModelBindingsImpl::AddBinding(FArcRmlSourceId SourceId, FFieldId WhenFieldId, FSimpleDelegate Delegate)
	{
		AddBindingImpl(SourceId, MakeUnique<FBinding>(WhenFieldId, MoveTemp(Delegate)));
	}

	void FViewModelBindingsImpl::RemoveAllBindings(FArcRmlSourceId SourceId)
	{
		if (FSource* FoundSource = FindSource(SourceId))
		{
			UnbindBindingsImpl(*FoundSource);
			FoundSource->Bindings.Empty();
			BindFieldDelegates(SourceId, *FoundSource);
		}
	}

	void FViewModelBindingsImpl::AddDependency(FArcRmlSourceId ToEvaluate, FArcRmlSourceId WhenChanged,
		FArcRmlViewModelBindings::FEvaluateSourceDelegate EvaluateDelegate)
	{
		AddDependency(ToEvaluate, WhenChanged, FFieldId(), MoveTemp(EvaluateDelegate));
	}

	void FViewModelBindingsImpl::AddDependency(FArcRmlSourceId ToEvaluate, FArcRmlSourceId WhenChanged, FFieldId WhenField,
		FArcRmlViewModelBindings::FEvaluateSourceDelegate EvaluateDelegate)
	{
		if (!ToEvaluate.IsValid() || !WhenChanged.IsValid())
		{
			return;
		}

		TUniquePtr<FDependency> Dependency = MakeUnique<FDependency>();
		Dependency->ParentFieldId = WhenField;
		Dependency->ParentId = WhenChanged;
		Dependency->ChildId = ToEvaluate;
		Dependency->SourceDelegate = MoveTemp(EvaluateDelegate);
		Dependencies.Add(MoveTemp(Dependency));
		bDependenciesDirty = true;

		if (FSource* Child = FindSource(ToEvaluate))
		{
			Child->ParentId = WhenChanged;
		}
		if (WhenField.IsValid())
		{
			if (FSource* Parent = FindSource(WhenChanged))
			{
				RegisterFieldDelegate(WhenChanged, *Parent, WhenField);
			}
		}
	}

	void FViewModelBindingsImpl::AddArrayDependency(FArcRmlSourceId WhenChanged, FFieldId WhenField,
		FArcRmlViewModelBindings::FEvaluateArrayDelegate EvaluateDelegate, FArcRmlViewModelBindings::FElementAddedDelegate ElementAddedDelegate)
	{
		FSource* Parent = FindSource(WhenChanged);
		if (!Parent || !WhenField.IsValid() || FindArrayDependency(WhenChanged, WhenField))
		{
			return;
		}

		TUniquePtr<FArrayDependency> Dependency = MakeUnique<FArrayDependency>();
		Dependency->ParentFieldId = WhenField;
		Dependency->ParentId = WhenChanged;
		Dependency->EvaluateDelegate = MoveTemp(EvaluateDelegate);
		Dependency->ElementAddedDelegate = MoveTemp(ElementAddedDelegate);
		ArrayDependencies.Add(MoveTemp(Dependency));
		bDependenciesDirty = true;

		RegisterFieldDelegate(WhenChanged, *Parent, WhenField);
		EvaluateArrayDependency(WhenChanged, WhenField, Parent->Source.Get());
	}

	void FViewModelBindingsImpl::SortDependencies()
	{
		if (!bDependenciesDirty)
		{
			return;
		}
		bDependenciesDirty = false;

		for (TPair<FArcRmlSourceId, TUniquePtr<FSource>>& SourcePair : Sources)
		{
			int32 Depth = 0;
			const FSource* Current = SourcePair.Value.Get();
			while (Current && Current->ParentId.IsValid() && Depth <= Sources.Num())
			{
				Current = FindSource(Current->ParentId);
				++Depth;
			}
			SourcePair.Value->Depth = Depth;
		}

		Dependencies.StableSort([this](const TUniquePtr<FDependency>& A, const TUniquePtr<FDependency>& B) -> bool
		{
			const FSource* ParentA = FindSource(A->ParentId);
			const FSource* ParentB = FindSource(B->ParentId);
			return (ParentA ? ParentA->Depth : 0) < (ParentB ? ParentB->Depth : 0);
		});
	}

	void FViewModelBindingsImpl::EvaluateDependents(const FArcRmlSourceId& SourceId, UObject* ParentObject, FFieldId ChangedField)
	{
		SortDependencies();

		TArray<TPair<FArcRmlSourceId, FArcRmlViewModelBindings::FEvaluateSourceDelegate>> ToEvaluate;
		for (const TUniquePtr<FDependency>& Dependency : Dependencies)
		{
			if (Dependency->ParentId == SourceId && (!ChangedField.IsValid() || Dependency->ParentFieldId == ChangedField))
			{
				ToEvaluate.Emplace(Dependency->ChildId, Dependency->SourceDelegate);
			}
		}

		TArray<FFieldId> ArrayFields;
		for (const TUniquePtr<FArrayDependency>& Dependency : ArrayDependencies)
		{
			if (Dependency->ParentId == SourceId && (!ChangedField.IsValid() || Dependency->ParentFieldId == ChangedField))
			{
				ArrayFields.Add(Dependency->ParentFieldId);
			}
		}

		for (const TPair<FArcRmlSourceId, FArcRmlViewModelBindings::FEvaluateSourceDelegate>& Entry : ToEvaluate)
		{
			UObject* ChildObject = ParentObject && Entry.Value.IsBound() ? Entry.Value.Execute(ParentObject) : nullptr;
			SetSource(Entry.Key, ChildObject);
		}

		for (const FFieldId& ArrayField : ArrayFields)
		{
			EvaluateArrayDependency(SourceId, ArrayField, ParentObject);
		}
	}

	void FViewModelBindingsImpl::EvaluateArrayDependency(const FArcRmlSourceId& ParentId, FFieldId FieldId, UObject* ParentObject)
	{
		FArrayDependency* Dependency = FindArrayDependency(ParentId, FieldId);
		if (!Dependency)
		{
			return;
		}

		TArray<UObject*> Elements;
		if (ParentObject && Dependency->EvaluateDelegate.IsBound())
		{
			Dependency->EvaluateDelegate.Execute(ParentObject, Elements);
		}

		const int32 OldNum = Dependency->NumElements;
		const int32 NewNum = Elements.Num();
		const FArcRmlViewModelBindings::FElementAddedDelegate ElementAdded = Dependency->ElementAddedDelegate;
		Dependency->NumElements = NewNum;

		for (int32 Index = NewNum; Index < OldNum; ++Index)
		{
			RemoveSource(ParentId.MakeElement(FieldId.GetName(), Index));
		}

		for (int32 Index = 0; Index < NewNum; ++Index)
		{
			const FArcRmlSourceId ElementId = ParentId.MakeElement(FieldId.GetName(), Index);
			if (FindSource(ElementId))
			{
				SetSource(ElementId, Elements[Index]);
				continue;
			}

			ElementAdded.ExecuteIfBound(ElementId, Index, Elements[Index]);
			if (FSource* Element = FindSource(ElementId))
			{
				Element->ParentId = ParentId;
				bDependenciesDirty = true;
			}
		}
	}

	void FViewModelBindingsImpl::Execute()
	{
		SortDependencies();

		TArray<TPair<int32, FArcRmlSourceId>> Ordered;
		Ordered.Reserve(Sources.Num());
		for (const TPair<FArcRmlSourceId, TUniquePtr<FSource>>& SourcePair : Sources)
		{
			Ordered.Emplace(SourcePair.Value->Depth, SourcePair.Key);
		}
		Ordered.StableSort([](const TPair<int32, FArcRmlSourceId>& A, const TPair<int32, FArcRmlSourceId>& B) -> bool
		{
			return A.Key < B.Key;
		});

		for (const TPair<int32, FArcRmlSourceId>& Entry : Ordered)
		{
			ExecuteBindings(Entry.Value, TOptional<FFieldId>());
		}
	}

	void FViewModelBindingsImpl::Execute(FArcRmlSourceId SourceId)
	{
		if (ensure(SourceId.IsValid()))
		{
			SortDependencies();
			ExecuteBindings(SourceId, TOptional<FFieldId>());
		}
	}

	void FViewModelBindingsImpl::ExecuteBindings(const FArcRmlSourceId& SourceId, TOptional<FFieldId> OnlyField)
	{
		const FSource* Source = FindSource(SourceId);
		if (!Source)
		{
			return;
		}

		UObject* Object = Source->Source.Get();
		TArray<FBinding> ToExecute;
		for (const TUniquePtr<FBinding>& Binding : Source->Bindings)
		{
			if (!OnlyField.IsSet() || Binding->GetFieldId() == OnlyField.GetValue())
			{
				ToExecute.Add(*Binding);
			}
		}

		for (const FBinding& Binding : ToExecute)
		{
			Binding.Execute(Object);
		}
	}

	void FViewModelBindingsImpl::HandleFieldChanged(UObject* SourceObject, FFieldId FieldId, FArcRmlSourceId SourceId)
	{
		const FSource* Source = FindSource(SourceId);
		if (!Source || Source->Source.Get() != SourceObject)
		{
			return;
		}

		EvaluateDependents(SourceId, SourceObject, FieldId);
		ExecuteBindings(SourceId, TOptional<FFieldId>(FieldId));
	}
}

FArcRmlViewModelBindings::FBuilder::FBuilder(ArcRmlMVVM::FViewModelBindingsImpl& InInstance, FArcRmlSourceId InId)
	: Instance(InInstance)
	, Id(MoveTemp(InId))
{
}

FArcRmlViewModelBindings::FBuilder& FArcRmlViewModelBindings::FBuilder::AddBinding(UE::FieldNotification::FFieldId WhenField,
	INotifyFieldValueChanged::FFieldValueChangedDelegate Delegate)
{
	Instance.AddBinding(GetId(), WhenField, MoveTemp(Delegate));
	return *this;
}

FArcRmlViewModelBindings::FBuilder& FArcRmlViewModelBindings::FBuilder::AddBinding(UE::FieldNotification::FFieldId WhenField, FSimpleDelegate Delegate)
{
	Instance.AddBinding(GetId(), WhenField, MoveTemp(Delegate));
	return *this;
}

FArcRmlViewModelBindings::FBuilder& FArcRmlViewModelBindings::FBuilder::AddDependency(FArcRmlSourceId WhenChanged, FEvaluateSourceDelegate EvaluateDelegate)
{
	Instance.AddDependency(GetId(), WhenChanged, MoveTemp(EvaluateDelegate));
	return *this;
}

FArcRmlViewModelBindings::FBuilder& FArcRmlViewModelBindings::FBuilder::AddDependency(FArcRmlSourceId WhenChanged,
	UE::FieldNotification::FFieldId WhenField, FEvaluateSourceDelegate EvaluateDelegate)
{
	Instance.AddDependency(GetId(), WhenChanged, WhenField, MoveTemp(EvaluateDelegate));
	return *this;
}

FArcRmlViewModelBindings::FArcRmlViewModelBindings()
{
	Impl = MakePimpl<ArcRmlMVVM::FViewModelBindingsImpl>();
}

FArcRmlViewModelBindings::FBuilder FArcRmlViewModelBindings::AddSource(FArcRmlSourceId Source, TScriptInterface<INotifyFieldValueChanged> Value)
{
	return Impl->AddSource(Source, Value);
}

void FArcRmlViewModelBindings::RemoveSource(FArcRmlSourceId Source)
{
	Impl->RemoveSource(Source);
}

void FArcRmlViewModelBindings::SetSource(FArcRmlSourceId Source, TScriptInterface<INotifyFieldValueChanged> Value)
{
	Impl->SetSource(Source, Value);
}

bool FArcRmlViewModelBindings::HasSource(FArcRmlSourceId Source) const
{
	return Impl->FindSource(Source) != nullptr;
}

UObject* FArcRmlViewModelBindings::GetSourceObject(FArcRmlSourceId Source) const
{
	const ArcRmlMVVM::FSource* Found = Impl->FindSource(Source);
	return Found ? Found->Source.Get() : nullptr;
}

void FArcRmlViewModelBindings::AddBinding(FArcRmlSourceId Source, UE::FieldNotification::FFieldId WhenField,
	INotifyFieldValueChanged::FFieldValueChangedDelegate Delegate)
{
	Impl->AddBinding(Source, WhenField, MoveTemp(Delegate));
}

void FArcRmlViewModelBindings::AddBinding(FArcRmlSourceId Source, UE::FieldNotification::FFieldId WhenField, FSimpleDelegate Delegate)
{
	Impl->AddBinding(Source, WhenField, MoveTemp(Delegate));
}

void FArcRmlViewModelBindings::RemoveAllBindings(FArcRmlSourceId Source)
{
	Impl->RemoveAllBindings(Source);
}

void FArcRmlViewModelBindings::AddDependency(FArcRmlSourceId ToEvaluate, FArcRmlSourceId WhenChanged, FEvaluateSourceDelegate EvaluateDelegate)
{
	Impl->AddDependency(ToEvaluate, WhenChanged, MoveTemp(EvaluateDelegate));
}

void FArcRmlViewModelBindings::AddDependency(FArcRmlSourceId ToEvaluate, FArcRmlSourceId WhenChanged, UE::FieldNotification::FFieldId WhenField,
	FEvaluateSourceDelegate EvaluateDelegate)
{
	Impl->AddDependency(ToEvaluate, WhenChanged, WhenField, MoveTemp(EvaluateDelegate));
}

void FArcRmlViewModelBindings::AddArrayDependency(FArcRmlSourceId WhenChanged, UE::FieldNotification::FFieldId WhenField,
	FEvaluateArrayDelegate EvaluateDelegate, FElementAddedDelegate ElementAddedDelegate)
{
	Impl->AddArrayDependency(WhenChanged, WhenField, MoveTemp(EvaluateDelegate), MoveTemp(ElementAddedDelegate));
}

void FArcRmlViewModelBindings::SetOnSourceRemoved(FSourceRemovedDelegate Delegate)
{
	Impl->OnSourceRemoved = MoveTemp(Delegate);
}

void FArcRmlViewModelBindings::Execute()
{
	Impl->Execute();
}

void FArcRmlViewModelBindings::Execute(FArcRmlSourceId Source)
{
	Impl->Execute(Source);
}
