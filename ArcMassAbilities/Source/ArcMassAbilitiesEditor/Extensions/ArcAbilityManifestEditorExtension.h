// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeEditorDataExtension.h"

namespace UE::StateTree::Compiler
{
	struct FPostInternalContext;
}

class UStateTreeState;
class IDetailLayoutBuilder;
class UInputAction;
class UInputMappingContext;
class UArcEffectDefinition;
class UMassEntityConfigAsset;

#include "ArcAbilityManifestEditorExtension.generated.h"

/**
 * Editor-side StateTree extension that walks all tasks in an ability tree at compile time
 * and bakes a UArcAbilityManifestExtension artifact onto the compiled UStateTree.
 *
 * Drop one into the Extension slot of any UArcAbilityDefinition StateTree asset.
 * Display name intentionally matches the runtime extension for clarity in the editor.
 */
UCLASS(EditInlineNew, DefaultToInstanced, DisplayName = "Ability Manifest (Arc)")
class UArcAbilityManifestEditorExtension : public UStateTreeEditorDataExtension
{
	GENERATED_BODY()

public:
	/** Reset transient validation state before the compiler validation pass begins. */
	virtual void PreValidate() override;

	/**
	 * Walk the compiled state hierarchy, collect manifest entries from each task node,
	 * and write them onto a UArcAbilityManifestExtension baked into the UStateTree.
	 * Emits a compiler warning when more than one gated input task is found.
	 */
	virtual bool HandlePostInternalCompile(const UE::StateTree::Compiler::FPostInternalContext& Context) override;

	/** Show per-state manifest contributions in the StateTree state detail panel. */
	virtual void CustomizeDetails(TNonNullPtr<UStateTreeState> State, IDetailLayoutBuilder& DetailBuilder) override;

	// -------------------------------------------------------------------------
	// Mirror of UArcAbilityManifestExtension — populated after each successful compile.
	// VisibleAnywhere + Transient so the Details panel shows the baked data without
	// the fields being serialised alongside the editor extension object.
	// -------------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, Transient, Category = "Manifest")
	TArray<TObjectPtr<UInputAction>> InputActions;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Manifest")
	TArray<TObjectPtr<UInputMappingContext>> InputMappingContexts;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Manifest")
	FGameplayTagContainer InputTags;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Manifest")
	TArray<TObjectPtr<UArcEffectDefinition>> AppliedEffects;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Manifest")
	TArray<TSubclassOf<AActor>> SpawnedActorClasses;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Manifest")
	TArray<TObjectPtr<UMassEntityConfigAsset>> SpawnedEntityConfigs;

	UPROPERTY(VisibleAnywhere, Transient, Category = "Manifest")
	TArray<TObjectPtr<UScriptStruct>> ClaimedSparseFragments;

	/** Number of gated input tasks found during the compile walk. */
	UPROPERTY(VisibleAnywhere, Transient, Category = "Diagnostics")
	int32 GatedTaskCount = 0;

private:
	/** Set during PreValidate if the tree contains more than one gated input task. */
	bool bMultiGateConflict = false;
};
