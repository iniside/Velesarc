// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsCompiledData.h"
#include "Decision/ArcTacticsNode.h"
#include "Engine/DataAsset.h"
#include "PropertyBindingBindingCollectionOwner.h"
#include "PropertyBindingPath.h"
#include "StructUtils/PropertyBag.h"

#include "ArcTacticsConfig.generated.h"

class FObjectPreSaveContext;
#if WITH_EDITOR
struct FArcTacticsCompilerDiagnostic;

enum class EArcTacticsAuthoringChange : uint8
{
	Value,
	Structure
};

DECLARE_MULTICAST_DELEGATE_OneParam(FArcTacticsAuthoringChanged, EArcTacticsAuthoringChange);
#endif

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsActionDefinition
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid ParametersID;

	UPROPERTY()
	FGuid ParameterBagID;
#endif

	UPROPERTY(EditAnywhere, Category = "Action")
	FArcTacticsNodeDefinition Action;

	UPROPERTY(EditAnywhere, Category = "Action")
	TArray<FArcTacticsNodeDefinition> Steps;

	UPROPERTY(EditAnywhere, Category = "Action")
	TArray<FArcTacticsNodeDefinition> ActiveSteps;

	UPROPERTY(EditAnywhere, Category = "Action")
	TArray<FArcTacticsNodeDefinition> Considerations;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FArcTacticsBranchParameters Parameters;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FInstancedPropertyBag ParameterBag;
};

USTRUCT(BlueprintType)
struct ARCTACTICS_API FArcTacticsSelectorDefinition
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid ParametersID;

	UPROPERTY()
	FGuid ParameterBagID;
#endif

	UPROPERTY(EditAnywhere, Category = "Selector")
	FArcTacticsNodeDefinition Selector;

	UPROPERTY(EditAnywhere, Category = "Selector")
	FArcTacticsNodeDefinition Action;

	UPROPERTY(EditAnywhere, Category = "Selector")
	TArray<FArcTacticsNodeDefinition> Steps;

	UPROPERTY(EditAnywhere, Category = "Selector")
	TArray<FArcTacticsNodeDefinition> ActiveSteps;

	UPROPERTY(EditAnywhere, Category = "Selector")
	TArray<FArcTacticsNodeDefinition> Considerations;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FArcTacticsBranchParameters Parameters;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FInstancedPropertyBag ParameterBag;
};

UCLASS(BlueprintType)
class ARCTACTICS_API UArcTacticsConfig : public UDataAsset, public IPropertyBindingBindingCollectionOwner
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid DecisionSettingsID;

	UPROPERTY()
	FGuid GlobalParametersID;

	UPROPERTY(EditAnywhere, Category = "Tactics")
	FArcTacticsDecisionSettingsParameters DecisionSettings;

	UPROPERTY(EditAnywhere, Category = "Global Parameters")
	FInstancedPropertyBag GlobalParameters;

	UPROPERTY()
	FGuid ContextID;

	UPROPERTY(EditAnywhere, Category = "Tactics")
	TArray<FArcTacticsNodeDefinition> GlobalTasks;

	UPROPERTY(EditAnywhere, Category = "Tactics")
	TArray<FArcTacticsActionDefinition> Actions;

	UPROPERTY(EditAnywhere, Category = "Tactics")
	TArray<FArcTacticsSelectorDefinition> Selectors;

	UPROPERTY()
	FArcTacticsBindingCollection EditorBindings;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Context")
	FArcTacticsContextData EditorContextData;
#endif

	UPROPERTY(VisibleAnywhere, Category = "Compiled")
	FArcTacticsCompiledData CompiledData;

	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;

	void EnsureValidNodeIDs();
	void RefreshEditorBindingData();
	bool SynchronizeNode(FArcTacticsNodeDefinition& Definition, EArcTacticsNodeRole ExpectedRole) const;
	FArcTacticsNodeDefinition* FindNodeByID(const FGuid& ID);
	const FArcTacticsNodeDefinition* FindNodeByID(const FGuid& ID) const;
	FArcTacticsSelectorDefinition* FindSelectorByID(const FGuid& ID);
	const FArcTacticsSelectorDefinition* FindSelectorByID(const FGuid& ID) const;
	FArcTacticsSelectorDefinition* FindOwningSelector(const FGuid& ID);
	const FArcTacticsSelectorDefinition* FindOwningSelector(const FGuid& ID) const;
	const FArcTacticsBinding* FindFunctionBinding(const FGuid& FunctionID) const;
	FGuid ResolveFunctionRoot(const FGuid& ID) const;
	static void AddOwnedFunctionIDs(TConstArrayView<FArcTacticsBinding> Bindings, TSet<FGuid>& InOutTargetIDs);
	void AddRemappedBindings(TConstArrayView<FArcTacticsBinding> Bindings, const TMap<FGuid, FGuid>& Remap);
	bool ResolveBindingEndpoint(const FGuid& ID, FArcTacticsBindingEndpoint& OutEndpoint) const;
	bool IsBindingSourceAccessible(
		const FGuid& SourceID,
		const FGuid& TargetID,
		bool bOutputBinding = false) const;
	bool IsBindingSourceAccessible(
		const FArcTacticsBindingEndpoint& Source,
		const FArcTacticsBindingEndpoint& Target,
		bool bOutputBinding = false) const;
	TMap<FGuid, FGuid> DuplicateAuthoringData(FArcTacticsNodeDefinition& Definition);
	TMap<FGuid, FGuid> DuplicateAuthoringData(FArcTacticsActionDefinition& Definition);
	TMap<FGuid, FGuid> DuplicateAuthoringData(FArcTacticsSelectorDefinition& Definition);
	void NotifyAuthoringChanged(EArcTacticsAuthoringChange Change);
	FArcTacticsAuthoringChanged& OnAuthoringChanged() { return AuthoringChanged; }
	bool CompileEditorData(TArray<FArcTacticsCompilerDiagnostic>& OutDiagnostics);
	FText GetAuthoringName(const FGuid ID) const;

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
	FArcTacticsAuthoringChanged AuthoringChanged;
#endif
};
