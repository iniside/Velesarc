// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyBindingBindableStructDescriptor.h"
#include "PropertyBindingBinding.h"
#include "PropertyBindingBindingCollection.h"

#include "ArcTacticsBindings.generated.h"

struct FPropertyBindingDataView;

UENUM()
enum class EArcTacticsBindingEndpointKind : uint8
{
	Invalid,
	Context,
	Node,
	DecisionSettings,
	GlobalParameters,
	BranchParameters,
	BranchParameterBag,
	Function
};

UENUM()
enum class EArcTacticsBindingOwner : uint8
{
	None,
	Root,
	GlobalTask,
	Action,
	Selector
};

UENUM()
enum class EArcTacticsBindingPhase : uint8
{
	Invalid,
	Context,
	GlobalInitialization,
	GlobalTask,
	DecisionSettings,
	BranchInitialization,
	Consideration,
	Action,
	Selector,
	Step
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsBindingEndpoint
{
	GENERATED_BODY()

	FArcTacticsBindingEndpoint() = default;
	FArcTacticsBindingEndpoint(
		const FGuid InID,
		const EArcTacticsBindingEndpointKind InKind,
		const EArcTacticsBindingOwner InOwner,
		const int32 InOwnerIndex,
		const EArcTacticsBindingPhase InPhase,
		const int32 InExecutionIndex = INDEX_NONE)
		: ID(InID)
		, Kind(InKind)
		, Owner(InOwner)
		, OwnerIndex(InOwnerIndex)
		, Phase(InPhase)
		, ExecutionIndex(InExecutionIndex)
		, RootID(InID)
		, RootKind(InKind)
	{
	}

	bool IsValid() const { return ID.IsValid() && Kind != EArcTacticsBindingEndpointKind::Invalid; }

	UPROPERTY()
	FGuid ID;

	UPROPERTY()
	EArcTacticsBindingEndpointKind Kind = EArcTacticsBindingEndpointKind::Invalid;

	UPROPERTY()
	EArcTacticsBindingOwner Owner = EArcTacticsBindingOwner::None;

	UPROPERTY()
	int32 OwnerIndex = INDEX_NONE;

	UPROPERTY()
	EArcTacticsBindingPhase Phase = EArcTacticsBindingPhase::Invalid;

	UPROPERTY()
	int32 ExecutionIndex = INDEX_NONE;

	UPROPERTY(meta = (ToolTip = "Endpoint whose data the final copy writes: the endpoint itself, or for a function the node, parameters or settings its output finally feeds."))
	FGuid RootID;

	UPROPERTY()
	EArcTacticsBindingEndpointKind RootKind = EArcTacticsBindingEndpointKind::Invalid;
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsNodeHandle
{
	GENERATED_BODY()

	static constexpr uint16 InvalidValue = MAX_uint16;

	FArcTacticsNodeHandle() = default;
	explicit FArcTacticsNodeHandle(const int32 InIndex)
		: Index(InIndex == INDEX_NONE ? InvalidValue : static_cast<uint16>(InIndex))
	{
		check(InIndex == INDEX_NONE || (InIndex >= 0 && InIndex < static_cast<int32>(InvalidValue)));
	}

	bool IsValid() const { return Index != InvalidValue; }
	int32 Get() const { return IsValid() ? static_cast<int32>(Index) : INDEX_NONE; }

	friend bool operator==(const FArcTacticsNodeHandle&, const FArcTacticsNodeHandle&) = default;

private:
	UPROPERTY()
	uint16 Index = InvalidValue;
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsDataHandle
{
	GENERATED_BODY()

	FArcTacticsDataHandle() = default;
	FArcTacticsDataHandle(const EArcTacticsBindingEndpointKind InKind, const int32 InIndex = INDEX_NONE)
		: Kind(InKind), Index(InIndex == INDEX_NONE ? MAX_uint16 : static_cast<uint16>(InIndex))
	{
		check(InIndex == INDEX_NONE || (InIndex >= 0 && InIndex < static_cast<int32>(MAX_uint16)));
	}

	bool IsValid() const { return Kind != EArcTacticsBindingEndpointKind::Invalid; }
	int32 GetIndex() const { return Index == MAX_uint16 ? INDEX_NONE : static_cast<int32>(Index); }
	EArcTacticsBindingEndpointKind GetKind() const { return Kind; }

	friend bool operator==(const FArcTacticsDataHandle&, const FArcTacticsDataHandle&) = default;

private:
	UPROPERTY()
	EArcTacticsBindingEndpointKind Kind = EArcTacticsBindingEndpointKind::Invalid;

	UPROPERTY()
	uint16 Index = MAX_uint16;
};

USTRUCT()
struct ARCTACTICS_API FArcTacticsBindableStructDescriptor : public FPropertyBindingBindableStructDescriptor
{
	GENERATED_BODY()

	FArcTacticsBindableStructDescriptor() = default;

#if WITH_EDITORONLY_DATA
	FArcTacticsBindableStructDescriptor(
		const FName InName,
		const UStruct* InStruct,
		const FGuid InID,
		const FArcTacticsDataHandle InDataHandle)
		: FPropertyBindingBindableStructDescriptor(InName, InStruct, InID)
		, DataHandle(InDataHandle)
	{
	}
#endif

	UPROPERTY(VisibleAnywhere, Category = "Tactics")
	FArcTacticsDataHandle DataHandle;
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsBinding : public FPropertyBindingBinding
{
	GENERATED_BODY()

	FArcTacticsBinding() = default;
	FArcTacticsBinding(
		const FPropertyBindingPath& InSourcePath,
		const FPropertyBindingPath& InTargetPath,
		const bool bInIsOutputBinding = false)
		: FPropertyBindingBinding(InSourcePath, InTargetPath)
		, bIsOutputBinding(bInIsOutputBinding)
	{
	}

#if WITH_EDITOR
	FArcTacticsBinding(
		const FConstStructView InFunctionNode,
		const FPropertyBindingPath& InSourcePath,
		const FPropertyBindingPath& InTargetPath)
		: FPropertyBindingBinding(InFunctionNode, InSourcePath, InTargetPath)
	{
	}
#endif

	void SetDataHandles(const FArcTacticsDataHandle InSource, const FArcTacticsDataHandle InTarget)
	{
		SourceDataHandle = InSource;
		TargetDataHandle = InTarget;
	}

	const FArcTacticsDataHandle& GetSourceDataHandle() const { return SourceDataHandle; }
	const FArcTacticsDataHandle& GetTargetDataHandle() const { return TargetDataHandle; }
	void SetIsOutputBinding(const bool bInIsOutputBinding) { bIsOutputBinding = bInIsOutputBinding; }
	bool IsOutputBinding() const { return bIsOutputBinding; }

protected:
	virtual FConstStructView GetSourceDataHandleStruct() const override
	{
		return FConstStructView::Make(SourceDataHandle);
	}

	UPROPERTY(VisibleAnywhere, Category = "Tactics")
	FArcTacticsDataHandle SourceDataHandle;

	UPROPERTY(VisibleAnywhere, Category = "Tactics")
	FArcTacticsDataHandle TargetDataHandle;

	UPROPERTY(VisibleAnywhere, Category = "Tactics")
	bool bIsOutputBinding = false;
};

#if WITH_EDITOR
enum class EArcTacticsBindingValidationResult : uint8
{
	Valid,
	InvalidTarget,
	InvalidScope,
	InvalidSourcePath,
	InvalidTargetPath,
	InvalidRole,
	SourceNotOutput,
	TargetNotInput,
	InvalidOutputTarget,
	LiteralOnlyTarget,
	IncompatibleTypes
};
#endif

namespace ArcTacticsBindings
{
	ARCTACTICS_API bool UpdatePathSegments(FPropertyBindingPath& Path, const FPropertyBindingDataView& DataView);
}

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsBindingCollection : public FPropertyBindingBindingCollection
{
	GENERATED_BODY()

	FArcTacticsBindableStructDescriptor& AddBindableStruct(const FArcTacticsBindableStructDescriptor& Descriptor);
	FArcTacticsBinding& AddCompiledBinding(FArcTacticsBinding&& Binding);
	const TArray<FArcTacticsBinding>& GetBindings() const { return Bindings; }
	const FArcTacticsBinding& GetBinding(const int32 Index) const { return Bindings[Index]; }
	const FArcTacticsBindableStructDescriptor* FindBindableStruct(const FArcTacticsDataHandle Handle) const;
	FArcTacticsBindableStructDescriptor* FindMutableBindableStruct(const FArcTacticsDataHandle Handle);

	virtual int32 GetNumBindableStructDescriptors() const override;
	virtual const FPropertyBindingBindableStructDescriptor* GetBindableStructDescriptorFromHandle(FConstStructView InSourceHandleView) const override;
	virtual int32 GetNumBindings() const override;
	virtual void ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const override;
	virtual void ForEachBinding(FPropertyBindingIndex16 InBegin, FPropertyBindingIndex16 InEnd, TFunctionRef<void(const FPropertyBindingBinding& Binding, int32 BindingIndex)> InFunction) const override;
	virtual void ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction) override;
	virtual void VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const override;
	virtual void VisitBindings(FPropertyBindingIndex16 InBegin, FPropertyBindingIndex16 InEnd, TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding, int32 BindingIndex)> InFunction) const override;
	virtual void VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction) override;

#if WITH_EDITOR
	EArcTacticsBindingValidationResult ValidateEditorBinding(
		const FArcTacticsBinding& Binding,
		const UStruct* SourceStruct,
		const FArcTacticsBindingEndpoint& SourceEndpoint,
		const UStruct* TargetStruct,
		const FArcTacticsBindingEndpoint& TargetEndpoint,
		bool bSourceAccessible,
		FString* OutPathError = nullptr);
#endif

protected:
	virtual void VisitSourceStructDescriptorInternal(TFunctionRef<EVisitResult(const FPropertyBindingBindableStructDescriptor& Descriptor)> InFunction) const override;
	virtual bool ResolveBindingCopyInfo(
		const FPropertyBindingBinding& InResolvedBinding,
		const FPropertyBindingPathIndirection& InBindingSourceLeafIndirection,
		const FPropertyBindingPathIndirection& InBindingTargetLeafIndirection,
		FPropertyBindingCopyInfo& OutCopyInfo) override;
	virtual void OnReset() override;

#if WITH_EDITOR
	virtual FPropertyBindingBinding* AddBindingInternal(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath) override;
	virtual void RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate) override;
	virtual bool HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
	virtual const FPropertyBindingBinding* FindBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
#endif

private:
	UPROPERTY(VisibleAnywhere, Category = "Tactics")
	TArray<FArcTacticsBindableStructDescriptor> BindableStructs;

	UPROPERTY(VisibleAnywhere, Category = "Tactics")
	TArray<FArcTacticsBinding> Bindings;
};
