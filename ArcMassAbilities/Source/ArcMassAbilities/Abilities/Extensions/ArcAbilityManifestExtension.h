// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeExtension.h"
#include "ArcAbilityManifestExtension.generated.h"

class UInputAction;
class UInputMappingContext;
class UArcEffectDefinition;
class UMassEntityConfigAsset;

/**
 * Baked manifest artifact stored on a compiled UStateTree.
 * Populated at editor compile time by UArcAbilityManifestEditorExtension.
 * Read-only at runtime; no runtime references to resolve in v0.
 */
UCLASS(DisplayName = "Ability Manifest (Arc)")
class ARCMASSABILITIES_API UArcAbilityManifestExtension : public UStateTreeExtension
{
	GENERATED_BODY()

public:
	/** Input actions referenced by WaitEnhancedInput* tasks in this tree. */
	UPROPERTY()
	TArray<TObjectPtr<UInputAction>> InputActions;

	/** Input mapping contexts referenced by WaitEnhancedInput* tasks in this tree. */
	UPROPERTY()
	TArray<TObjectPtr<UInputMappingContext>> InputMappingContexts;

	/** Gameplay input tags referenced by ListenInput* tasks in this tree. */
	UPROPERTY()
	FGameplayTagContainer InputTags;

	/** Effect definitions that may be applied by tasks in this tree. */
	UPROPERTY()
	TArray<TObjectPtr<UArcEffectDefinition>> AppliedEffects;

	/** Actor classes that may be spawned by tasks in this tree. */
	UPROPERTY()
	TArray<TSubclassOf<AActor>> SpawnedActorClasses;

	/** Mass entity config assets that may be spawned by tasks in this tree. */
	UPROPERTY()
	TArray<TObjectPtr<UMassEntityConfigAsset>> SpawnedEntityConfigs;

	/** Sparse fragment structs claimed by tasks in this tree. */
	UPROPERTY()
	TArray<TObjectPtr<UScriptStruct>> ClaimedSparseFragments;

	// UStateTreeExtension interface
	virtual bool Link(FStateTreeLinker& Linker) override { return true; }
};
