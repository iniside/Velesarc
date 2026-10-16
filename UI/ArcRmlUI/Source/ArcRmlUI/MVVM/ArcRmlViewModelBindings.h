// Copyright Epic Games, Inc. All Rights Reserved.
// Adapted from Engine/Plugins/Experimental/SlateModelViewViewModel/Source/SlateMVVM/Public/SlateViewModelBindings.h.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "FieldNotificationId.h"
#include "INotifyFieldValueChanged.h"
#include "Templates/PimplPtr.h"
#include "UObject/ScriptInterface.h"

namespace ArcRmlMVVM
{
	struct FViewModelBindingsImpl;
}

struct FArcRmlSourceId
{
public:
	FArcRmlSourceId() = default;

	static FArcRmlSourceId Make(const FString& InPath)
	{
		FArcRmlSourceId Result;
		Result.Path = InPath;
		return Result;
	}

	FArcRmlSourceId MakeChild(FName FieldName) const
	{
		return Make(FString::Printf(TEXT("%s.%s"), *Path, *FieldName.ToString()));
	}

	FArcRmlSourceId MakeElement(FName FieldName, int32 Index) const
	{
		return Make(FString::Printf(TEXT("%s.%s[%d]"), *Path, *FieldName.ToString(), Index));
	}

	const FString& GetPath() const
	{
		return Path;
	}

	bool IsValid() const
	{
		return !Path.IsEmpty();
	}

	explicit operator bool() const
	{
		return IsValid();
	}

	bool operator==(const FArcRmlSourceId& Other) const
	{
		return Path.Equals(Other.Path, ESearchCase::CaseSensitive);
	}

	friend uint32 GetTypeHash(const FArcRmlSourceId& Key)
	{
		return GetTypeHash(Key.Path);
	}

private:
	FString Path;
};

class FArcRmlViewModelBindings
{
public:
	using FEvaluateSourceDelegate = TDelegate<UObject*(UObject* ParentObject)>;
	using FEvaluateArrayDelegate = TDelegate<void(UObject* ParentObject, TArray<UObject*>& OutElements)>;
	using FElementAddedDelegate = TDelegate<void(FArcRmlSourceId ElementId, int32 Index, UObject* ElementObject)>;
	using FSourceRemovedDelegate = TDelegate<void(FArcRmlSourceId SourceId)>;

	struct FBuilder
	{
		FBuilder(ArcRmlMVVM::FViewModelBindingsImpl& InInstance, FArcRmlSourceId InId);
		FBuilder(const FBuilder&) = delete;
		FBuilder& operator=(const FBuilder&) = delete;

		FBuilder& AddBinding(UE::FieldNotification::FFieldId WhenFieldId, INotifyFieldValueChanged::FFieldValueChangedDelegate Delegate);
		FBuilder& AddBinding(UE::FieldNotification::FFieldId WhenFieldId, FSimpleDelegate Delegate);
		FBuilder& AddDependency(FArcRmlSourceId WhenChanged, FEvaluateSourceDelegate EvaluateDelegate);
		FBuilder& AddDependency(FArcRmlSourceId WhenChanged, UE::FieldNotification::FFieldId WhenFieldId, FEvaluateSourceDelegate EvaluateDelegate);

		FArcRmlSourceId GetId() const
		{
			return Id;
		}

	private:
		ArcRmlMVVM::FViewModelBindingsImpl& Instance;
		FArcRmlSourceId Id;
	};

	FArcRmlViewModelBindings();
	FArcRmlViewModelBindings(const FArcRmlViewModelBindings&) = delete;
	FArcRmlViewModelBindings& operator=(const FArcRmlViewModelBindings&) = delete;

	FBuilder AddSource(FArcRmlSourceId Source, TScriptInterface<INotifyFieldValueChanged> Value);
	void RemoveSource(FArcRmlSourceId Source);
	void SetSource(FArcRmlSourceId Source, TScriptInterface<INotifyFieldValueChanged> Value);
	bool HasSource(FArcRmlSourceId Source) const;
	UObject* GetSourceObject(FArcRmlSourceId Source) const;

	void AddBinding(FArcRmlSourceId Source, UE::FieldNotification::FFieldId WhenFieldId, INotifyFieldValueChanged::FFieldValueChangedDelegate Delegate);
	void AddBinding(FArcRmlSourceId Source, UE::FieldNotification::FFieldId WhenFieldId, FSimpleDelegate Delegate);
	void RemoveAllBindings(FArcRmlSourceId Source);

	void AddDependency(FArcRmlSourceId ToEvaluate, FArcRmlSourceId WhenChanged, FEvaluateSourceDelegate EvaluateDelegate);
	void AddDependency(FArcRmlSourceId ToEvaluate, FArcRmlSourceId WhenChanged, UE::FieldNotification::FFieldId WhenFieldId, FEvaluateSourceDelegate EvaluateDelegate);
	void AddArrayDependency(FArcRmlSourceId WhenChanged, UE::FieldNotification::FFieldId WhenFieldId, FEvaluateArrayDelegate EvaluateDelegate,
		FElementAddedDelegate ElementAddedDelegate);

	void SetOnSourceRemoved(FSourceRemovedDelegate Delegate);

	void Execute();
	void Execute(FArcRmlSourceId Source);

private:
	TPimplPtr<ArcRmlMVVM::FViewModelBindingsImpl> Impl;
};
