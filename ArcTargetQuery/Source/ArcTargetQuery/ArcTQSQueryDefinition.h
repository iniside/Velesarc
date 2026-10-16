// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSStep.h"
#include "ArcTQSBindings.h"
#include "ArcTQSPreviewTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"
#include "StructUtils/PropertyBag.h"
#include "Engine/DataAsset.h"
#include "PropertyBindingBindingCollectionOwner.h"
#include "ArcTQSQueryDefinition.generated.h"

class FObjectPreSaveContext;
class UArcTQSQueryDefinition;

#if WITH_EDITOR
struct FArcTQSCompilerDiagnostic;

DECLARE_MULTICAST_DELEGATE_OneParam(FArcTQSOnDefinitionCompiled, const UArcTQSQueryDefinition&);

namespace ArcTQS::Delegates
{
	extern ARCTARGETQUERY_API FArcTQSOnDefinitionCompiled OnPostCompile;
	extern ARCTARGETQUERY_API FArcTQSOnDefinitionCompiled OnPreviewSetupChanged;
}
#endif

/**
 * Data asset that defines a target query.
 * Contains parameters, a generator, and an ordered pipeline of steps.
 * Callers reference it through FArcTQSQueryReference.
 */
UCLASS(BlueprintType, Blueprintable)
class ARCTARGETQUERY_API UArcTQSQueryDefinition : public UDataAsset, public IPropertyBindingBindingCollectionOwner
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid ParametersID;

	UPROPERTY()
	FGuid QuerierID;

	UPROPERTY()
	FGuid ItemID;
#endif

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (ChildRowFeatures = "Extended"))
	FInstancedPropertyBag Parameters;

	// The generator that produces the initial target pool
	UPROPERTY(EditAnywhere, Category = "Generator", meta = (BaseStruct = "/Script/ArcTargetQuery.ArcTQSGenerator"))
	FInstancedStruct Generator;

	// Ordered pipeline of filter/score steps, executed sequentially
	UPROPERTY(EditAnywhere, Category = "Steps", meta = (BaseStruct = "/Script/ArcTargetQuery.ArcTQSStep"))
	TArray<FInstancedStruct> Steps;

	// How to select from the final scored pool
	UPROPERTY(EditAnywhere, Category = "Selection")
	EArcTQSSelectionMode SelectionMode = EArcTQSSelectionMode::HighestScore;

	// For TopN mode: how many results to return
	UPROPERTY(EditAnywhere, Category = "Selection", meta = (EditCondition = "SelectionMode == EArcTQSSelectionMode::TopN", ClampMin = 1))
	int32 TopN = 5;

	// For AllPassing mode: minimum score threshold
	UPROPERTY(EditAnywhere, Category = "Selection", meta = (EditCondition = "SelectionMode == EArcTQSSelectionMode::AllPassing"))
	float MinPassingScore = 0.0f;

	// For custom top-percent modes: what percentage of the top items to consider
	UPROPERTY(EditAnywhere, Category = "Selection",
		meta = (EditCondition = "SelectionMode == EArcTQSSelectionMode::RandomFromTopPercent || SelectionMode == EArcTQSSelectionMode::WeightedRandomFromTopPercent",
			ClampMin = 1.0, ClampMax = 100.0))
	float TopPercent = 25.0f;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FArcTQSBindingCollection EditorBindings;

	UPROPERTY(Transient)
	FArcTQSQuerierInputs EditorQuerierInputs;

	UPROPERTY(Transient)
	FArcTQSItemInputs EditorItemInputs;

	UPROPERTY(EditAnywhere, Category = "Preview")
	FArcTQSPreviewSetup PreviewSetup;
#endif

	UPROPERTY(VisibleAnywhere, Category = "Compiled")
	FArcTQSCompiledData CompiledData;

	bool IsExecutable() const
	{
		return CompiledData.IsExecutable();
	}

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditImport() override;
	virtual void PostEditUndo() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

	void RepairNodeIDs();
	bool CompileEditorData(TArray<FArcTQSCompilerDiagnostic>& OutDiagnostics);
	FArcTQSDataHandle ResolveEndpoint(const FGuid& ID) const;
	FName GetEndpointName(const FGuid& ID) const;
	FConstStructView FindNode(const FGuid& ID) const;
	const FArcTQSBinding* FindFunctionBinding(const FGuid& FunctionID) const;
	EArcTQSDataKind ResolveRootNodeKind(const FGuid& ID) const;

	virtual bool GetBindingDataView(
		const FPropertyBindingBinding& InBinding,
		EBindingSide InSide,
		FPropertyBindingDataView& OutDataView) override;
	virtual void GetBindableStructs(
		FGuid InTargetStructID,
		TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const override;
	virtual bool GetBindableStructByID(
		FGuid InStructID,
		TInstancedStruct<FPropertyBindingBindableStructDescriptor>& OutStructDesc) const override;
	virtual bool GetBindingDataViewByID(FGuid InStructID, FPropertyBindingDataView& OutDataView) const override;
	virtual FPropertyBindingBindingCollection* GetEditorPropertyBindings() override;
	virtual const FPropertyBindingBindingCollection* GetEditorPropertyBindings() const override;
	virtual bool CanCreateParameter(FGuid InStructID) const override;
	virtual void CreateParametersForStruct(
		FGuid InStructID,
		TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs) override;
	virtual FGuid GetFallbackStructID() const override;
	virtual void AppendBindablePropertyFunctionStructs(
		TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& InOutStructs) const override;
	virtual void OnPropertyBindingChanged(
		const FPropertyBindingPath& InSourcePath,
		const FPropertyBindingPath& InTargetPath) override;
#endif

private:
#if WITH_EDITOR
	void InitializeEditorBindingsOwner();
	void RepairAndCompile();
#endif
};
