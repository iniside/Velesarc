// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "PropertyBindingBindableStructDescriptor.h"
#include "PropertyBindingBinding.h"
#include "PropertyBindingBindingCollection.h"
#include "PropertyBindingPath.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcTQSBindings.generated.h"

UENUM()
enum class EArcTQSDataKind : uint8
{
	Invalid,
	Parameters,
	Querier,
	Item,
	Generator,
	Step,
	Function
};

USTRUCT(BlueprintType)
struct ARCTARGETQUERY_API FArcTQSQuerierInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Querier")
	FMassEntityHandle Entity;

	UPROPERTY(EditAnywhere, Category = "Querier")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Querier")
	FVector Forward = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, Category = "Querier")
	TWeakObjectPtr<AActor> Actor;
};

USTRUCT(BlueprintType)
struct ARCTARGETQUERY_API FArcTQSItemInputs
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Item")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Item")
	FMassEntityHandle Entity;
};

USTRUCT()
struct ARCTARGETQUERY_API FArcTQSDataHandle
{
	GENERATED_BODY()

	FArcTQSDataHandle() = default;
	FArcTQSDataHandle(const EArcTQSDataKind InKind, const int32 InIndex = INDEX_NONE)
		: Kind(InKind), Index(InIndex == INDEX_NONE ? MAX_uint16 : static_cast<uint16>(InIndex))
	{
		check(InIndex == INDEX_NONE || (InIndex >= 0 && InIndex < static_cast<int32>(MAX_uint16)));
	}

	bool IsValid() const { return Kind != EArcTQSDataKind::Invalid; }
	EArcTQSDataKind GetKind() const { return Kind; }
	int32 GetIndex() const { return Index == MAX_uint16 ? INDEX_NONE : static_cast<int32>(Index); }

	friend bool operator==(const FArcTQSDataHandle&, const FArcTQSDataHandle&) = default;

private:
	UPROPERTY()
	EArcTQSDataKind Kind = EArcTQSDataKind::Invalid;

	UPROPERTY()
	uint16 Index = MAX_uint16;
};

USTRUCT()
struct ARCTARGETQUERY_API FArcTQSBindableStructDescriptor : public FPropertyBindingBindableStructDescriptor
{
	GENERATED_BODY()

	FArcTQSBindableStructDescriptor() = default;

#if WITH_EDITORONLY_DATA
	FArcTQSBindableStructDescriptor(const FName InName, const UStruct* InStruct, const FGuid InID, const FArcTQSDataHandle InDataHandle)
		: FPropertyBindingBindableStructDescriptor(InName, InStruct, InID)
		, DataHandle(InDataHandle)
	{
	}
#endif

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	FArcTQSDataHandle DataHandle;
};

USTRUCT()
struct ARCTARGETQUERY_API FArcTQSBinding : public FPropertyBindingBinding
{
	GENERATED_BODY()

	FArcTQSBinding() = default;
	FArcTQSBinding(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath)
		: FPropertyBindingBinding(InSourcePath, InTargetPath)
	{
	}

#if WITH_EDITOR
	FArcTQSBinding(const FConstStructView InFunctionNode, const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath)
		: FPropertyBindingBinding(InFunctionNode, InSourcePath, InTargetPath)
	{
	}
#endif

	void SetDataHandles(const FArcTQSDataHandle InSource, const FArcTQSDataHandle InTarget)
	{
		SourceDataHandle = InSource;
		TargetDataHandle = InTarget;
	}

	const FArcTQSDataHandle& GetSourceDataHandle() const { return SourceDataHandle; }
	const FArcTQSDataHandle& GetTargetDataHandle() const { return TargetDataHandle; }

protected:
	virtual FConstStructView GetSourceDataHandleStruct() const override
	{
		return FConstStructView::Make(SourceDataHandle);
	}

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	FArcTQSDataHandle SourceDataHandle;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	FArcTQSDataHandle TargetDataHandle;
};

USTRUCT()
struct ARCTARGETQUERY_API FArcTQSBindingCollection : public FPropertyBindingBindingCollection
{
	GENERATED_BODY()

	FArcTQSBindableStructDescriptor& AddBindableStruct(const FArcTQSBindableStructDescriptor& Descriptor);
	FArcTQSBinding& AddCompiledBinding(FArcTQSBinding&& Binding);
	const TArray<FArcTQSBinding>& GetBindings() const { return Bindings; }
	const FArcTQSBinding& GetBinding(const int32 Index) const { return Bindings[Index]; }
	const FArcTQSBindableStructDescriptor* FindBindableStruct(const FArcTQSDataHandle Handle) const;
	FArcTQSBindableStructDescriptor* FindMutableBindableStruct(const FArcTQSDataHandle Handle);

	virtual int32 GetNumBindableStructDescriptors() const override;
	virtual const FPropertyBindingBindableStructDescriptor* GetBindableStructDescriptorFromHandle(FConstStructView InSourceHandleView) const override;
	virtual int32 GetNumBindings() const override;
	virtual void ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const override;
	virtual void ForEachBinding(FPropertyBindingIndex16 InBegin, FPropertyBindingIndex16 InEnd, TFunctionRef<void(const FPropertyBindingBinding& Binding, int32 BindingIndex)> InFunction) const override;
	virtual void ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction) override;
	virtual void VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const override;
	virtual void VisitBindings(FPropertyBindingIndex16 InBegin, FPropertyBindingIndex16 InEnd, TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding, int32 BindingIndex)> InFunction) const override;
	virtual void VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction) override;

protected:
	virtual void VisitSourceStructDescriptorInternal(TFunctionRef<EVisitResult(const FPropertyBindingBindableStructDescriptor& Descriptor)> InFunction) const override;
	virtual void OnReset() override;

#if WITH_EDITOR
	virtual FPropertyBindingBinding* AddBindingInternal(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath) override;
	virtual void RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate) override;
	virtual bool HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
	virtual const FPropertyBindingBinding* FindBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
#endif

private:
	UPROPERTY(VisibleAnywhere, Category = "TQS")
	TArray<FArcTQSBindableStructDescriptor> BindableStructs;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	TArray<FArcTQSBinding> Bindings;
};

USTRUCT()
struct ARCTARGETQUERY_API FArcTQSCompiledFunction
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "TQS", meta = (BaseStruct = "/Script/ArcTargetQuery.ArcTQSPropertyFunction"))
	FInstancedStruct Function;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	FPropertyBindingIndex16 InputBatch;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	bool bPerItem = false;
};

USTRUCT()
struct ARCTARGETQUERY_API FArcTQSCompiledData
{
	GENERATED_BODY()

	static constexpr uint32 CurrentVersion = 3;

	void Reset();
	bool ResolvePaths(const UStruct* ParametersStruct);
	bool IsExecutable() const;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	uint32 Version = 0;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	uint32 Revision = 0;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	bool bIsValid = false;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	FPropertyBindingIndex16 GeneratorBatch;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	TArray<FPropertyBindingIndex16> StepBatches;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	TArray<FPropertyBindingIndex16> StepPerItemBatches;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	TArray<FArcTQSCompiledFunction> Functions;

	UPROPERTY(VisibleAnywhere, Category = "TQS")
	FArcTQSBindingCollection Bindings;
};

namespace ArcTQS
{
	ARCTARGETQUERY_API bool IsBindingSourceAccessible(EArcTQSDataKind Source, EArcTQSDataKind Target);

#if WITH_EDITOR
	ARCTARGETQUERY_API bool IsLiteralOnlyProperty(const FProperty& Property);
	ARCTARGETQUERY_API bool IsBindableProperty(const FProperty& Property);
	ARCTARGETQUERY_API bool IsRequiredBinding(const FProperty& Property);

	ARCTARGETQUERY_API void ForEachMemoryKey(
		const UStruct& Struct,
		const void* Memory,
		TFunctionRef<void(const FProperty& Property, FGameplayTag Key)> Function);
#endif
}
