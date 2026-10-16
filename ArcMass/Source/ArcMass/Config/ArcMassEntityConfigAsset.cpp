// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Config/ArcMassEntityConfigAsset.h"

#include "ArcMass/Config/ArcMassConfigTraitAccess.h"
#include "ArcMass/Config/ArcMassEntityInitializer.h"
#include "Engine/AssetManager.h"
#include "MassAssortedFragmentsTrait.h"
#include "Misc/DataValidation.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectWriter.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/UObjectGlobals.h"

namespace ArcMassConfigMerge_Private
{
	/** Access the protected Fragments array on UMassAssortedFragmentsTrait via reflection. */
	static TArray<FInstancedStruct>* GetAssortedFragments(UMassEntityTraitBase* Trait)
	{
		FProperty* Prop = UMassAssortedFragmentsTrait::StaticClass()->FindPropertyByName(TEXT("Fragments"));
		return Prop ? Prop->ContainerPtrToValuePtr<TArray<FInstancedStruct>>(Trait) : nullptr;
	}

	/** Access the protected Tags array on UMassAssortedFragmentsTrait via reflection. */
	static TArray<FInstancedStruct>* GetAssortedTags(UMassEntityTraitBase* Trait)
	{
		FProperty* Prop = UMassAssortedFragmentsTrait::StaticClass()->FindPropertyByName(TEXT("Tags"));
		return Prop ? Prop->ContainerPtrToValuePtr<TArray<FInstancedStruct>>(Trait) : nullptr;
	}

	/**
	 * Byte-equivalence test for two FInstancedStructs, mirroring TraitsEquivalent's
	 * FObjectWriter approach (NOT FInstancedStruct::Identical, whose semantics differ).
	 * Both-null is equivalent; differing script structs are not.
	 */
	static bool StructsEquivalent(const FInstancedStruct& A, const FInstancedStruct& B)
	{
		const UScriptStruct* StructA = A.GetScriptStruct();
		const UScriptStruct* StructB = B.GetScriptStruct();
		if (StructA != StructB)
		{
			return false;
		}
		if (!StructA)
		{
			// Both null (equal scripts, both nullptr) → equivalent.
			return true;
		}
		TArray<uint8> BufA;
		TArray<uint8> BufB;
		FMemoryWriter WrA(BufA);
		FMemoryWriter WrB(BufB);
		const_cast<UScriptStruct*>(StructA)->SerializeItem(WrA, const_cast<void*>(static_cast<const void*>(A.GetMemory())), nullptr);
		const_cast<UScriptStruct*>(StructB)->SerializeItem(WrB, const_cast<void*>(static_cast<const void*>(B.GetMemory())), nullptr);
		return BufA == BufB;
	}

	/**
	 * Three-way merge for a single FInstancedStruct list (Fragments or Tags).
	 * VarList is mutated in place.
	 */
	static void MergeInstancedStructList(
		TArray<FInstancedStruct>* VarList,
		TArray<FInstancedStruct>* TmplList,
		TArray<FInstancedStruct>* SnapList)
	{
		if (!VarList || !TmplList || !SnapList)
		{
			return;
		}

		TMap<const UScriptStruct*, FInstancedStruct*> TmplByStruct;
		TMap<const UScriptStruct*, FInstancedStruct*> SnapByStruct;
		TMap<const UScriptStruct*, FInstancedStruct*> VarByStruct;

		// First-wins on duplicate structs so merges on already-corrupted lists are
		// deterministic and visible. Repair is a separate pass.
		for (FInstancedStruct& Entry : *TmplList)
		{
			const UScriptStruct* Struct = Entry.GetScriptStruct();
			if (!Struct)
			{
				continue;
			}
			if (TmplByStruct.Contains(Struct))
			{
				UE_LOGF(LogTemp, Warning,"ArcMassConfig: template trait carries duplicate fragment/tag struct %ls; using the first.", *Struct->GetName());
				continue;
			}
			TmplByStruct.Add(Struct, &Entry);
		}
		for (FInstancedStruct& Entry : *SnapList)
		{
			const UScriptStruct* Struct = Entry.GetScriptStruct();
			if (Struct && !SnapByStruct.Contains(Struct))
			{
				SnapByStruct.Add(Struct, &Entry);
			}
		}
		for (FInstancedStruct& Entry : *VarList)
		{
			const UScriptStruct* Struct = Entry.GetScriptStruct();
			if (Struct && !VarByStruct.Contains(Struct))
			{
				VarByStruct.Add(Struct, &Entry);
			}
		}

		TArray<FInstancedStruct> MergedList;

		// Process template entries
		for (TPair<const UScriptStruct*, FInstancedStruct*>& TmplPair : TmplByStruct)
		{
			const UScriptStruct* Struct     = TmplPair.Key;
			FInstancedStruct* TmplEntry     = TmplPair.Value;
			FInstancedStruct** SnapEntryPtr = SnapByStruct.Find(Struct);
			FInstancedStruct** VarEntryPtr  = VarByStruct.Find(Struct);

			if (!SnapEntryPtr)
			{
				// New from template — add
				MergedList.Add(*TmplEntry);
			}
			else if (VarEntryPtr)
			{
				// Both snapshot and variant have it
				if (StructsEquivalent(**VarEntryPtr, **SnapEntryPtr))
				{
					// Variant didn't override — take template version
					MergedList.Add(*TmplEntry);
				}
				else
				{
					// Variant overrode — keep variant's version
					MergedList.Add(**VarEntryPtr);
				}
			}
			// else: in snapshot but NOT in variant — variant deleted it, skip
		}

		// Variant-only additions (not in template, not in snapshot)
		for (TPair<const UScriptStruct*, FInstancedStruct*>& VarPair : VarByStruct)
		{
			const UScriptStruct* Struct = VarPair.Key;
			if (!TmplByStruct.Contains(Struct) && !SnapByStruct.Contains(Struct))
			{
				MergedList.Add(*VarPair.Value);
			}
		}

		*VarList = MergedList;
	}

	/** Walk one entry array (StaticMeshes/SkinnedMeshes) and append the named soft mesh path per entry. */
	static void AppendSoftPathsFromEntryArray(const UMassEntityTraitBase* Trait, const TCHAR* ArrayName, const TCHAR* MeshPropName, TArray<FSoftObjectPath>& OutPaths)
	{
		FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(Trait->GetClass(), ArrayName);
		if (!ArrayProp)
		{
			return;
		}
		FStructProperty* EntryProp = CastField<FStructProperty>(ArrayProp->Inner);
		if (!EntryProp)
		{
			return;
		}
		FSoftObjectProperty* MeshProp = FindFProperty<FSoftObjectProperty>(EntryProp->Struct, MeshPropName);
		if (!MeshProp)
		{
			return;
		}

		FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Trait));
		for (int32 EntryIdx = 0; EntryIdx < ArrayHelper.Num(); ++EntryIdx)
		{
			const void* EntryPtr = ArrayHelper.GetElementPtr(EntryIdx);
			const FSoftObjectPtr& SoftPtr = MeshProp->GetPropertyValue_InContainer(EntryPtr);
			if (!SoftPtr.IsNull())
			{
				OutPaths.Add(SoftPtr.ToSoftObjectPath());
			}
		}
	}

	/**
	 * Append soft object paths from a UArcUnifiedMeshVisualizationTrait's StaticMeshes and
	 * SkinnedMeshes arrays to OutPaths (via reflection). Skips null paths.
	 * Returns false if the trait is not the unified vis trait.
	 */
	static bool AppendUnifiedMeshAssets(const UMassEntityTraitBase* Trait, TArray<FSoftObjectPath>& OutPaths)
	{
		if (!Trait || Trait->GetClass()->GetFName() != TEXT("ArcUnifiedMeshVisualizationTrait"))
		{
			return false;
		}
		AppendSoftPathsFromEntryArray(Trait, TEXT("StaticMeshes"), TEXT("StaticMesh"), OutPaths);
		AppendSoftPathsFromEntryArray(Trait, TEXT("SkinnedMeshes"), TEXT("SkinnedAsset"), OutPaths);
		return true;
	}

	/** Build a comma-joined string of package paths from the given soft object path list. */
	static FString SerializeSoftPathList(TConstArrayView<FSoftObjectPath> Paths)
	{
		FString Result;
		for (const FSoftObjectPath& Path : Paths)
		{
			if (Path.IsNull())
			{
				continue;
			}
			if (!Result.IsEmpty())
			{
				Result += TEXT(",");
			}
			Result += Path.ToString();
		}
		return Result;
	}
} // namespace ArcMassConfigMerge_Private

// ── Static FName definitions ──────────────────────────────────────────────────

const FName UArcMassEntityConfigAsset::TagName_SourceTemplate = TEXT("ArcMassConfig.SourceTemplate");
const FName UArcMassEntityConfigAsset::TagName_TraitClasses   = TEXT("ArcMassConfig.TraitClasses");
const FName UArcMassEntityConfigAsset::TagName_MeshPaths      = TEXT("ArcMassConfig.MeshPaths");

// ── UArcMassEntityConfigAsset ─────────────────────────────────────────────────

FPrimaryAssetId UArcMassEntityConfigAsset::GetPrimaryAssetId() const
{
	if (!ConfigAssetId.IsValid())
	{
		return FPrimaryAssetId();
	}
	return FPrimaryAssetId(TEXT("ArcMassEntityConfig"), *ConfigAssetId.ToString());
}

void UArcMassEntityConfigAsset::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	if (!ConfigAssetId.IsValid())
	{
		ConfigAssetId = FGuid::NewGuid();
		MarkPackageDirty();
	}
#endif
#if WITH_EDITORONLY_DATA
	// Migrate the legacy hard pointer to the primary-asset-id reference. The hard
	// pointer guarantees the template is loaded; ConditionalPostLoad makes sure its
	// TemplateAssetId has been generated before we read it.
	if (!SourceTemplateId.IsValid() && SourceTemplate_DEPRECATED)
	{
		SourceTemplate_DEPRECATED->ConditionalPostLoad();
		const FPrimaryAssetId TemplateId = SourceTemplate_DEPRECATED->GetPrimaryAssetId();
		if (TemplateId.IsValid())
		{
			SourceTemplateId = TemplateId;
			SourceTemplate_DEPRECATED = nullptr;
			MarkPackageDirty();
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void UArcMassEntityConfigAsset::RegenerateConfigAssetId()
{
	ConfigAssetId = FGuid::NewGuid();
	MarkPackageDirty();
}

#if WITH_EDITORONLY_DATA
UArcMassEntityConfigTemplate* UArcMassEntityConfigAsset::ResolveSourceTemplate() const
{
	if (!SourceTemplateId.IsValid() || !UAssetManager::IsInitialized())
	{
		return nullptr;
	}
	const FSoftObjectPath TemplatePath = UAssetManager::Get().GetPrimaryAssetPath(SourceTemplateId);
	if (!TemplatePath.IsValid())
	{
		return nullptr;
	}
	return Cast<UArcMassEntityConfigTemplate>(TemplatePath.TryLoad());
}

void UArcMassEntityConfigAsset::UpdateAssetBundleData()
{
	if (!UAssetManager::IsInitialized())
	{
		return;
	}
	UAssetManager& AM = UAssetManager::Get();
	AssetBundleData.Reset();
	AM.InitializeAssetBundlesFromMetadata(this, AssetBundleData);

	TConstArrayView<UMassEntityTraitBase*> Traits = GetConfig().GetTraits();
	for (UMassEntityTraitBase* Trait : Traits)
	{
		if (Trait)
		{
			AM.InitializeAssetBundlesFromMetadata(Trait, AssetBundleData);
		}
	}

	for (const FInstancedStruct& IS : InitialSparseFragments)
	{
		if (IS.IsValid())
		{
			AM.InitializeAssetBundlesFromMetadata(IS.GetScriptStruct(), IS.GetMemory(), AssetBundleData, IS.GetScriptStruct()->GetFName());
		}
	}

	for (const TObjectPtr<UArcMassEntityInitializer>& Init : PostCommitInitializers)
	{
		if (Init)
		{
			AM.InitializeAssetBundlesFromMetadata(Init.Get(), AssetBundleData);
		}
	}
}
#endif // WITH_EDITORONLY_DATA

void UArcMassEntityConfigAsset::PreSave(FObjectPreSaveContext SaveContext)
{
	if (!ConfigAssetId.IsValid())
	{
		ConfigAssetId = FGuid::NewGuid();
	}
#if WITH_EDITORONLY_DATA
	UpdateAssetBundleData();
	if (UAssetManager::IsInitialized())
	{
		UAssetManager::Get().RefreshAssetData(this);
	}
#endif
	Super::PreSave(SaveContext);
}

void UArcMassEntityConfigAsset::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	// 1. SourceTemplate — primary asset id of the linked template, or empty.
#if WITH_EDITORONLY_DATA
	const FString SourceTemplateStr = SourceTemplateId.IsValid() ? SourceTemplateId.ToString() : FString();
#else
	const FString SourceTemplateStr;
#endif // WITH_EDITORONLY_DATA
	Context.AddTag(UObject::FAssetRegistryTag(TagName_SourceTemplate, SourceTemplateStr,
		UObject::FAssetRegistryTag::TT_Hidden));

	// 2. TraitClasses — comma-joined class paths of every trait on this config.
	FString TraitClassesStr;
	TConstArrayView<UMassEntityTraitBase*> Traits = GetConfig().GetTraits();
	for (int32 Idx = 0; Idx < Traits.Num(); ++Idx)
	{
		if (!Traits[Idx])
		{
			continue;
		}
		if (!TraitClassesStr.IsEmpty())
		{
			TraitClassesStr += TEXT(",");
		}
		TraitClassesStr += Traits[Idx]->GetClass()->GetPathName();
	}
	Context.AddTag(UObject::FAssetRegistryTag(TagName_TraitClasses, TraitClassesStr,
		UObject::FAssetRegistryTag::TT_Hidden));

	// 3. MeshPaths — comma-joined package paths of mesh assets from visualization traits.
	TArray<FSoftObjectPath> UnifiedMeshPaths;
	for (UMassEntityTraitBase* Trait : Traits)
	{
		if (!Trait)
		{
			continue;
		}
		ArcMassConfigMerge_Private::AppendUnifiedMeshAssets(Trait, UnifiedMeshPaths);
	}
	FString MeshPathsStr = ArcMassConfigMerge_Private::SerializeSoftPathList(UnifiedMeshPaths);
	Context.AddTag(UObject::FAssetRegistryTag(TagName_MeshPaths, MeshPathsStr,
		UObject::FAssetRegistryTag::TT_Hidden));
}

#if WITH_EDITOR
void UArcMassEntityConfigAsset::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (bDuplicateForPIE)
	{
		return;
	}

	ConfigAssetId = FGuid::NewGuid();

	// DuplicateObject already deep-copies BaseSnapshot's Instanced trait subobjects,
	// so the duplicate inherits a faithful snapshot — do NOT rebuild it from the
	// template's current state (that would merge the preview/Ctrl-D copy against the
	// wrong baseline and discard pending-update tracking). Super (Config.PostDuplicate)
	// already regenerates the Mass ConfigGuid for the non-PIE case.
	MarkPackageDirty();
}

EDataValidationResult UArcMassEntityConfigAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// One trait per class is the authoring invariant. More than one trait of the same
	// class is template-merge corruption. This is DETECTION only (Warning), surfaced for
	// a separate repair pass — we intentionally do NOT combine EDataValidationResult::Invalid
	// into Result. In this engine branch the validator subsystem flags an asset as FAILED
	// whenever it returns Invalid (UEditorValidatorSubsystem::ValidateAssetsInternal bumps
	// NumInvalid; LogAssetValidationSummary sets bFailed = NumInvalid > 0 || Result != Valid),
	// severity-agnostic — so returning Invalid would block the cook on known-corrupt assets.
	// Keeping Result as-is (the Warning message alone) is the non-cook-blocking path.
	TSet<UClass*> Seen;
	TSet<UClass*> Warned;
	TConstArrayView<UMassEntityTraitBase*> Traits = GetConfig().GetTraits();
	for (UMassEntityTraitBase* Trait : Traits)
	{
		if (!Trait)
		{
			continue;
		}
		UClass* TraitClass = Trait->GetClass();
		if (Seen.Contains(TraitClass))
		{
			// Warn only once per duplicated class so three-of-a-kind doesn't spam.
			if (!Warned.Contains(TraitClass))
			{
				Warned.Add(TraitClass);
				Context.AddMessage(EMessageSeverity::Warning, FText::Format(
					NSLOCTEXT("ArcMassConfig", "DuplicateTraitClass", "Config carries more than one trait of class '{0}'. One trait per class is the authoring invariant; this is likely template-merge corruption and should be repaired."),
					FText::FromString(TraitClass->GetName())));
			}
		}
		else
		{
			Seen.Add(TraitClass);
		}
	}

	return Result;
}
#endif // WITH_EDITOR

// ── UArcMassEntityConfigTemplate ─────────────────────────────────────────────

FPrimaryAssetId UArcMassEntityConfigTemplate::GetPrimaryAssetId() const
{
	if (!TemplateAssetId.IsValid())
	{
		return FPrimaryAssetId();
	}
	return FPrimaryAssetId(TEXT("ArcMassEntityConfigTemplate"), *TemplateAssetId.ToString());
}

void UArcMassEntityConfigTemplate::PostLoad()
{
	Super::PostLoad();
#if WITH_EDITOR
	if (!TemplateAssetId.IsValid())
	{
		TemplateAssetId = FGuid::NewGuid();
		MarkPackageDirty();
	}
#endif
}

void UArcMassEntityConfigTemplate::PreSave(FObjectPreSaveContext SaveContext)
{
	if (!TemplateAssetId.IsValid())
	{
		TemplateAssetId = FGuid::NewGuid();
	}
#if WITH_EDITORONLY_DATA
	if (UAssetManager::IsInitialized())
	{
		UAssetManager::Get().RefreshAssetData(this);
	}
#endif
	Super::PreSave(SaveContext);
}

FPrimaryAssetId UArcMassEntityConfigTemplate::EnsurePrimaryAssetId()
{
	if (!TemplateAssetId.IsValid())
	{
		TemplateAssetId = FGuid::NewGuid();
		MarkPackageDirty();
	}
	return GetPrimaryAssetId();
}

void UArcMassEntityConfigTemplate::RegenerateTemplateAssetId()
{
	TemplateAssetId = FGuid::NewGuid();
	MarkPackageDirty();
}

#if WITH_EDITOR
void UArcMassEntityConfigTemplate::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if (!bDuplicateForPIE)
	{
		TemplateAssetId = FGuid::NewGuid();
		MarkPackageDirty();
	}
}
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA

void UArcMassEntityConfigTemplate::CloneTraitsInto(
	TConstArrayView<TObjectPtr<UMassEntityTraitBase>> SourceTraits,
	TArray<TObjectPtr<UMassEntityTraitBase>>& DestTraits,
	UObject* DestOuter)
{
	DestTraits.Reset();
	DestTraits.Reserve(SourceTraits.Num());
	for (const TObjectPtr<UMassEntityTraitBase>& SrcTrait : SourceTraits)
	{
		if (!SrcTrait)
		{
			continue;
		}
		UMassEntityTraitBase* Clone = DuplicateObject<UMassEntityTraitBase>(SrcTrait.Get(), DestOuter);
		DestTraits.Add(Clone);
	}
}

bool UArcMassEntityConfigTemplate::TraitsEquivalent(const UMassEntityTraitBase* A, const UMassEntityTraitBase* B)
{
	if (!A || !B)
	{
		return A == B;
	}
	if (A->GetClass() != B->GetClass())
	{
		return false;
	}
	TArray<uint8> BufA;
	TArray<uint8> BufB;
	FObjectWriter WrA(const_cast<UMassEntityTraitBase*>(A), BufA);
	FObjectWriter WrB(const_cast<UMassEntityTraitBase*>(B), BufB);
	return BufA == BufB;
}

UMassEntityTraitBase* UArcMassEntityConfigTemplate::MergeAssortedFragmentsTrait(
	UMassEntityTraitBase* VarTrait,
	const UMassEntityTraitBase* TmplTrait,
	const UMassEntityTraitBase* SnapTrait,
	UObject* DestOuter)
{
	if (!VarTrait || !TmplTrait || !SnapTrait)
	{
		// No variant to merge into — return fresh clone of template
		if (TmplTrait)
		{
			return DuplicateObject<UMassEntityTraitBase>(const_cast<UMassEntityTraitBase*>(TmplTrait), DestOuter);
		}
		return nullptr;
	}

	TArray<FInstancedStruct>* VarFragments  = ArcMassConfigMerge_Private::GetAssortedFragments(VarTrait);
	TArray<FInstancedStruct>* TmplFragments = ArcMassConfigMerge_Private::GetAssortedFragments(const_cast<UMassEntityTraitBase*>(TmplTrait));
	TArray<FInstancedStruct>* SnapFragments = ArcMassConfigMerge_Private::GetAssortedFragments(const_cast<UMassEntityTraitBase*>(SnapTrait));

	ArcMassConfigMerge_Private::MergeInstancedStructList(VarFragments, TmplFragments, SnapFragments);

	TArray<FInstancedStruct>* VarTags  = ArcMassConfigMerge_Private::GetAssortedTags(VarTrait);
	TArray<FInstancedStruct>* TmplTags = ArcMassConfigMerge_Private::GetAssortedTags(const_cast<UMassEntityTraitBase*>(TmplTrait));
	TArray<FInstancedStruct>* SnapTags = ArcMassConfigMerge_Private::GetAssortedTags(const_cast<UMassEntityTraitBase*>(SnapTrait));

	ArcMassConfigMerge_Private::MergeInstancedStructList(VarTags, TmplTags, SnapTags);

	return VarTrait;
}

void UArcMassEntityConfigTemplate::SetNewOrReplaceTemplate(UArcMassEntityConfigAsset* Target)
{
	if (!Target)
	{
		return;
	}

	// Classes that came from the PREVIOUS template (still tracked in BaseSnapshot).
	TSet<UClass*> PriorTemplateClasses;
	for (const TObjectPtr<UMassEntityTraitBase>& T : Target->BaseSnapshot)
	{
		if (T)
		{
			PriorTemplateClasses.Add(T->GetClass());
		}
	}

	// Classes carried by the NEW template's current traits. A variant trait whose
	// class collides with the new template must NOT be kept (template wins on collision).
	TConstArrayView<UMassEntityTraitBase*> TmplTraits = GetConfig().GetTraits();
	TSet<UClass*> NewTemplateClasses;
	for (UMassEntityTraitBase* TmplTrait : TmplTraits)
	{
		if (TmplTrait)
		{
			NewTemplateClasses.Add(TmplTrait->GetClass());
		}
	}

	// Collect variant-only traits = traits whose class is in neither the prior-template
	// set NOR the new-template set (so overlapping classes are dropped, no dupes).
	TArray<TObjectPtr<UMassEntityTraitBase>> KeepList;
	TArray<TObjectPtr<UMassEntityTraitBase>>* VariantTraits =
		ArcMassConfig::GetMutableTraitsArray(Target->GetMutableConfig());
	if (!VariantTraits)
	{
		return;
	}

	for (const TObjectPtr<UMassEntityTraitBase>& T : *VariantTraits)
	{
		if (T && !PriorTemplateClasses.Contains(T->GetClass()) && !NewTemplateClasses.Contains(T->GetClass()))
		{
			KeepList.Add(T);
		}
	}

	// Rebuild: new template traits + variant-only kept traits. Dedup the template side
	// too (first-wins) in case the template itself carries two traits of the same class.
	VariantTraits->Reset();
	TSet<UClass*> StampedClasses;
	for (UMassEntityTraitBase* TmplTrait : TmplTraits)
	{
		if (!TmplTrait)
		{
			continue;
		}
		if (StampedClasses.Contains(TmplTrait->GetClass()))
		{
			UE_LOGF(LogTemp, Warning,"ArcMassConfig: template '%ls' carries duplicate trait class %ls; stamping only the first.", *GetName(), *TmplTrait->GetClass()->GetName());
			continue;
		}
		StampedClasses.Add(TmplTrait->GetClass());
		VariantTraits->Add(DuplicateObject<UMassEntityTraitBase>(TmplTrait, Target));
	}
	VariantTraits->Append(KeepList);

	// Rebase snapshot to current template state.
	TArray<TObjectPtr<UMassEntityTraitBase>>* TmplTraitsArray =
		ArcMassConfig::GetMutableTraitsArray(const_cast<FMassEntityConfig&>(GetConfig()));
	if (TmplTraitsArray)
	{
		CloneTraitsInto(*TmplTraitsArray, Target->BaseSnapshot, Target);
	}

	Target->SourceTemplateId = EnsurePrimaryAssetId();
	Target->GetMutableConfig().ForceRegenerateGUID();
}

void UArcMassEntityConfigTemplate::SetTemplate(UArcMassEntityConfigAsset* Target)
{
	if (!Target)
	{
		return;
	}

	TArray<TObjectPtr<UMassEntityTraitBase>>* VariantTraits =
		ArcMassConfig::GetMutableTraitsArray(Target->GetMutableConfig());
	if (!VariantTraits)
	{
		return;
	}

	// Build set of classes already present in target
	TSet<UClass*> ExistingClasses;
	for (const TObjectPtr<UMassEntityTraitBase>& T : *VariantTraits)
	{
		if (T)
		{
			ExistingClasses.Add(T->GetClass());
		}
	}

	// Additive: only append traits whose class is not already present
	TConstArrayView<UMassEntityTraitBase*> TmplTraits = GetConfig().GetTraits();
	for (UMassEntityTraitBase* TmplTrait : TmplTraits)
	{
		if (!TmplTrait)
		{
			continue;
		}
		if (!ExistingClasses.Contains(TmplTrait->GetClass()))
		{
			VariantTraits->Add(DuplicateObject<UMassEntityTraitBase>(TmplTrait, Target));
			// Track the newly appended class so a second template trait of the same
			// class (template-side duplicate) is skipped instead of appended twice.
			ExistingClasses.Add(TmplTrait->GetClass());
		}
	}

	// Rebase snapshot and set source
	TArray<TObjectPtr<UMassEntityTraitBase>>* TmplTraitsArray =
		ArcMassConfig::GetMutableTraitsArray(const_cast<FMassEntityConfig&>(GetConfig()));
	if (TmplTraitsArray)
	{
		CloneTraitsInto(*TmplTraitsArray, Target->BaseSnapshot, Target);
	}

	Target->SourceTemplateId = EnsurePrimaryAssetId();
	Target->GetMutableConfig().ForceRegenerateGUID();
}

void UArcMassEntityConfigTemplate::UpdateFromTemplate(UArcMassEntityConfigAsset* Target)
{
	if (!Target)
	{
		return;
	}

	if (Target->SourceTemplateId.IsValid() && Target->SourceTemplateId != EnsurePrimaryAssetId())
	{
		UE_LOGF(LogTemp, Warning,"ArcMassConfig: UpdateFromTemplate called on '%ls' which is linked to a different template (%ls) than '%ls'; skipping.",
			*Target->GetName(), *Target->SourceTemplateId.ToString(), *GetName());
		return;
	}

	TArray<TObjectPtr<UMassEntityTraitBase>>* VariantTraits =
		ArcMassConfig::GetMutableTraitsArray(Target->GetMutableConfig());
	if (!VariantTraits)
	{
		return;
	}

	// Build class-keyed LOOKUP maps (first-wins) plus a class set for the template.
	// The merged list is assembled by iterating the template's traits array IN ORDER
	// (below), not by iterating these maps — map iteration is hash-ordered and would
	// produce non-deterministic output trait order (→ spurious asset dirtying).
	// Keep pre-rebase snapshot alive until after sub-merge (step 7 in plan).
	TMap<UClass*, UMassEntityTraitBase*> SnapClasses;
	TMap<UClass*, UMassEntityTraitBase*> VarClasses;
	TSet<UClass*> TmplClassSet;

	for (const TObjectPtr<UMassEntityTraitBase>& T : Target->BaseSnapshot)
	{
		if (!T)
		{
			continue;
		}
		if (SnapClasses.Contains(T->GetClass()))
		{
			UE_LOGF(LogTemp, Warning,"ArcMassConfig: '%ls' carries duplicate trait class %ls; using the first.", *Target->GetName(), *T->GetClass()->GetName());
			continue;
		}
		SnapClasses.Add(T->GetClass(), T.Get());
	}

	TConstArrayView<UMassEntityTraitBase*> TmplTraitsView = GetConfig().GetTraits();
	for (UMassEntityTraitBase* T : TmplTraitsView)
	{
		if (T)
		{
			TmplClassSet.Add(T->GetClass());
		}
	}

	for (const TObjectPtr<UMassEntityTraitBase>& T : *VariantTraits)
	{
		if (!T)
		{
			continue;
		}
		if (VarClasses.Contains(T->GetClass()))
		{
			UE_LOGF(LogTemp, Warning,"ArcMassConfig: '%ls' carries duplicate trait class %ls; using the first.", *Target->GetName(), *T->GetClass()->GetName());
			continue;
		}
		VarClasses.Add(T->GetClass(), T.Get());
	}

	TArray<TObjectPtr<UMassEntityTraitBase>> Merged;

	// Process template traits IN TEMPLATE ORDER (steps 3a/3b/3c from plan). Iterating
	// the array (not TmplClassSet) makes the merged order deterministic = template order.
	for (UMassEntityTraitBase* TmplTrait : TmplTraitsView)
	{
		if (!TmplTrait)
		{
			continue;
		}
		UClass* Class = TmplTrait->GetClass();

		UMassEntityTraitBase** SnapTrait = SnapClasses.Find(Class);
		UMassEntityTraitBase** VarTrait  = VarClasses.Find(Class);

		if (!SnapTrait)
		{
			// (a) Newly added to template — clone it in
			Merged.Add(DuplicateObject<UMassEntityTraitBase>(TmplTrait, Target));
		}
		else if (VarTrait)
		{
			// (b) In both snapshot and variant
			if (TraitsEquivalent(*VarTrait, *SnapTrait))
			{
				// Variant has not overridden — take new template version
				Merged.Add(DuplicateObject<UMassEntityTraitBase>(TmplTrait, Target));
			}
			else
			{
				// Variant overrode the trait
				if (TmplTrait->IsA<UMassAssortedFragmentsTrait>())
				{
					// Sub-merge AssortedFragments/Tags lists
					UMassEntityTraitBase* MergedTrait = MergeAssortedFragmentsTrait(*VarTrait, TmplTrait, *SnapTrait, Target);
					Merged.Add(MergedTrait);
				}
				else
				{
					// Keep variant unchanged (do NOT clone — keep existing pointer).
					// GC-safe: Target owns these objects and Merged holds strong refs across
					// the *VariantTraits = Merged array copy, so there is no collection window.
					Merged.Add(*VarTrait);
				}
			}
		}
		// (c) In SnapClasses but not VarClasses — variant deleted it, skip
	}

	// Step 4: Variant-only additions — in variant, not in template, not in snapshot.
	// Iterate *VariantTraits IN ORDER (not VarClasses map) for deterministic append order.
	for (const TObjectPtr<UMassEntityTraitBase>& VarTrait : *VariantTraits)
	{
		if (!VarTrait)
		{
			continue;
		}
		UClass* Class = VarTrait->GetClass();
		// Guard against an earlier duplicate of this class already appended (first-wins).
		if (VarClasses.FindRef(Class) != VarTrait.Get())
		{
			continue;
		}
		if (!TmplClassSet.Contains(Class) && !SnapClasses.Contains(Class))
		{
			Merged.Add(VarTrait);
		}
	}

	// Step 5: Write merged list back
	*VariantTraits = Merged;

	// Step 6: Regenerate GUID
	Target->GetMutableConfig().ForceRegenerateGUID();

	// Step 7: Rebase snapshot — overwrites old snapshot
	TArray<TObjectPtr<UMassEntityTraitBase>>* TmplTraitsArray =
		ArcMassConfig::GetMutableTraitsArray(const_cast<FMassEntityConfig&>(GetConfig()));
	if (TmplTraitsArray)
	{
		CloneTraitsInto(*TmplTraitsArray, Target->BaseSnapshot, Target);
	}
}

void UArcMassEntityConfigTemplate::PushFromVariant(UArcMassEntityConfigAsset* Source)
{
	if (!Source)
	{
		return;
	}

	TArray<TObjectPtr<UMassEntityTraitBase>>* TemplateTraits =
		ArcMassConfig::GetMutableTraitsArray(GetMutableConfig());
	if (!TemplateTraits)
	{
		return;
	}

	TArray<TObjectPtr<UMassEntityTraitBase>>* SourceVariantTraits =
		ArcMassConfig::GetMutableTraitsArray(Source->GetMutableConfig());
	if (!SourceVariantTraits)
	{
		return;
	}

	CloneTraitsInto(*SourceVariantTraits, *TemplateTraits, this);
	GetMutableConfig().ForceRegenerateGUID();

	// Rebase source's snapshot — overrides flatten by definition.
	CloneTraitsInto(*SourceVariantTraits, Source->BaseSnapshot, Source);
	Source->GetMutableConfig().ForceRegenerateGUID();
}

void UArcMassEntityConfigTemplate::DEBUG_StampInto(UArcMassEntityConfigAsset* Target)
{
	if (!Target)
	{
		return;
	}
	Target->Modify();
	SetNewOrReplaceTemplate(Target);
	Target->MarkPackageDirty();
}

void UArcMassEntityConfigTemplate::DEBUG_UpdateFrom(UArcMassEntityConfigAsset* Target)
{
	if (!Target)
	{
		return;
	}
	Target->Modify();
	UpdateFromTemplate(Target);
	Target->MarkPackageDirty();
}

#endif // WITH_EDITORONLY_DATA
