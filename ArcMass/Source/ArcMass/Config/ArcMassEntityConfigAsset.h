// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "StructUtils/InstancedStruct.h"
#include "AssetRegistry/AssetBundleData.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/PrimaryAssetId.h"
#include "ArcMassEntityConfigAsset.generated.h"

class UArcMassEntityConfigTemplate;

UENUM()
enum class EArcMassConfigMergeMode : uint8
{
	Update,           // three-way merge using BaseSnapshot, preserves variant overrides
	SetNewOrReplace,  // wipe template-origin traits (per BaseSnapshot class set), stamp from template, preserve variant-only
	Set,              // additive stamp, keep all existing
};

UCLASS(meta = (LoadBehavior = "LazyOnDemand"))
class ARCMASS_API UArcMassEntityConfigAsset : public UMassEntityConfigAsset
{
	GENERATED_BODY()
public:
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostLoad() override;

	UFUNCTION(BlueprintCallable, CallInEditor)
	void RegenerateConfigAssetId();

	UPROPERTY(VisibleAnywhere, Category = "Arc|Identity")
	FGuid ConfigAssetId;

#if WITH_EDITOR
	// Override to handle BaseSnapshot duplication semantics on asset duplicate.
	// Base class regenerates ConfigGuid; we additionally keep BaseSnapshot
	// consistent with the duplicate's identity.
	virtual void PostDuplicate(bool bDuplicateForPIE) override;

	// Detection (Warning, non-cook-blocking) for the authoring invariant of one trait
	// per class. Flags template-merge corruption; repair is a separate pass.
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	// Loads and returns the linked template via SourceTemplateId. Nullptr when unset
	// or when the asset manager can't map the id to an asset (template deleted, or
	// not yet resaved with its TemplateAssetId registry tag).
	UArcMassEntityConfigTemplate* ResolveSourceTemplate() const;

	// Primary asset id of the linked template (type "ArcMassEntityConfigTemplate").
	UPROPERTY(VisibleAnywhere, Category = "Template")
	FPrimaryAssetId SourceTemplateId;

	// Legacy hard pointer; migrated to SourceTemplateId in PostLoad. Loaded but never saved.
	UPROPERTY()
	TObjectPtr<UArcMassEntityConfigTemplate> SourceTemplate_DEPRECATED;

	// Clone of the template's trait list at last sync. Drives three-way merge.
	// Instanced is REQUIRED so trait subobjects serialize into the package.
	UPROPERTY(VisibleAnywhere, Category = "Template", Instanced)
	TArray<TObjectPtr<UMassEntityTraitBase>> BaseSnapshot;

	UPROPERTY()
	FAssetBundleData AssetBundleData;

	virtual void UpdateAssetBundleData();
#endif // WITH_EDITORONLY_DATA

public:
	// Asset-level sparse fragment overrides. Replaces trait-emitted defaults on type collision.
	// V1: not merged from UArcMassEntityConfigTemplate (variant-only). See plan Open Issue #2.
	UPROPERTY(EditAnywhere, Category="Arc|Sparse",
	          meta=(BaseStruct="/Script/MassCore.MassSparseFragment", ExcludeBaseStruct))
	TArray<FInstancedStruct> InitialSparseFragments;

	// Asset-level post-Commit initializers. Run after FEntityBuilder.Commit returns,
	// outside any creation context. See ArcMassEntityInitializer.h.
	// V1: not merged from UArcMassEntityConfigTemplate (variant-only).
	UPROPERTY(EditAnywhere, Category="Arc|Init", Instanced)
	TArray<TObjectPtr<class UArcMassEntityInitializer>> PostCommitInitializers;

	// Public so external filter code can reference these without string literals.
	static const FName TagName_SourceTemplate;
	static const FName TagName_TraitClasses;
	static const FName TagName_MeshPaths;
};

UCLASS(meta = (LoadBehavior = "LazyOnDemand"))
class ARCMASS_API UArcMassEntityConfigTemplate : public UMassEntityConfigAsset
{
	GENERATED_BODY()
public:
	virtual bool IsEditorOnly() const override { return true; }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual void PostLoad() override;

	// Generates TemplateAssetId when missing, then returns the primary asset id.
	// Use at link-stamping sites where the template may be freshly created.
	FPrimaryAssetId EnsurePrimaryAssetId();

	UFUNCTION(BlueprintCallable, CallInEditor)
	void RegenerateTemplateAssetId();

	UPROPERTY(VisibleAnywhere, Category = "Arc|Identity")
	FGuid TemplateAssetId;

#if WITH_EDITOR
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA
	void SetNewOrReplaceTemplate(UArcMassEntityConfigAsset* Target);
	void SetTemplate(UArcMassEntityConfigAsset* Target);
	void UpdateFromTemplate(UArcMassEntityConfigAsset* Target);
	void PushFromVariant(UArcMassEntityConfigAsset* Source);

	UFUNCTION(CallInEditor, Category = "Debug")
	void DEBUG_StampInto(UArcMassEntityConfigAsset* Target);

	UFUNCTION(CallInEditor, Category = "Debug")
	void DEBUG_UpdateFrom(UArcMassEntityConfigAsset* Target);

private:
	static void CloneTraitsInto(
		TConstArrayView<TObjectPtr<UMassEntityTraitBase>> SourceTraits,
		TArray<TObjectPtr<UMassEntityTraitBase>>& DestTraits,
		UObject* DestOuter);

	static bool TraitsEquivalent(const UMassEntityTraitBase* A, const UMassEntityTraitBase* B);

	// Three-way sub-merge for UMassAssortedFragmentsTrait. Returns the merged trait
	// (which is the variant's instance, modified in place) or a fresh clone of TmplTrait
	// when no variant override exists.
	static UMassEntityTraitBase* MergeAssortedFragmentsTrait(
		UMassEntityTraitBase* VarTrait,        // mutated in place when present
		const UMassEntityTraitBase* TmplTrait, // current template version
		const UMassEntityTraitBase* SnapTrait, // old snapshot (pre-rebase)
		UObject* DestOuter);
#endif // WITH_EDITORONLY_DATA
};
