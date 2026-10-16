// Copyright Lukasz Baran. All Rights Reserved.

/**
 * Regression suite for UArcMassEntityConfigTemplate template-merge dedup and lifecycle.
 *
 * Covers:
 *   - SetTemplate additive dedup (no duplicate classes after stamp)
 *   - SetNewOrReplaceTemplate template-wins dedup (no duplicate classes, correct winner)
 *   - First-assign with empty BaseSnapshot dedup (regression: Bug A)
 *   - UpdateFromTemplate: new-trait propagation, variant-override preservation, deterministic order
 *   - PostDuplicate: BaseSnapshot fidelity + ConfigAssetId regeneration
 *   - UpdateFromTemplate identity guard (wrong-template early-return)
 *   - IsDataValid: warns on duplicate trait class
 *
 * These functions live under WITH_EDITORONLY_DATA; CQTest runs in the editor target only, so
 * all test bodies are guarded by WITH_EDITORONLY_DATA below.
 */

#include "CQTest.h"

#include "ArcMassConfigTestTraits.h"
#include "ArcMass/Config/ArcMassEntityConfigAsset.h"
#include "ArcMass/Config/ArcMassConfigTraitAccess.h"
#include "Misc/DataValidation.h"
#include "UObject/Package.h"

#if WITH_EDITORONLY_DATA

namespace ArcMassConfigMergeTests_Private
{
	/** Count traits of a given class in a config's own trait list. */
	static int32 CountTraitsByClass(UArcMassEntityConfigAsset* Config, UClass* TraitClass)
	{
		int32 Count = 0;
		TConstArrayView<UMassEntityTraitBase*> Traits = Config->GetConfig().GetTraits();
		for (UMassEntityTraitBase* Trait : Traits)
		{
			if (Trait && Trait->GetClass() == TraitClass)
			{
				++Count;
			}
		}
		return Count;
	}

	/** Return true if every trait class appears exactly once in the config. */
	static bool HasNoDuplicateTraitClasses(UArcMassEntityConfigAsset* Config)
	{
		TSet<UClass*> Seen;
		TConstArrayView<UMassEntityTraitBase*> Traits = Config->GetConfig().GetTraits();
		for (UMassEntityTraitBase* Trait : Traits)
		{
			if (!Trait)
			{
				continue;
			}
			if (Seen.Contains(Trait->GetClass()))
			{
				return false;
			}
			Seen.Add(Trait->GetClass());
		}
		return true;
	}

	/** Build a config asset in the transient package with a given set of pre-created traits. */
	static UArcMassEntityConfigAsset* MakeConfig(const TArray<UMassEntityTraitBase*>& InitialTraits)
	{
		UArcMassEntityConfigAsset* Config = NewObject<UArcMassEntityConfigAsset>(GetTransientPackage());
		TArray<TObjectPtr<UMassEntityTraitBase>>* TraitsArray =
			ArcMassConfig::GetMutableTraitsArray(Config->GetMutableConfig());
		check(TraitsArray);
		for (UMassEntityTraitBase* Trait : InitialTraits)
		{
			TraitsArray->Add(Trait);
		}
		return Config;
	}

	/** Build a template asset in the transient package with a given set of pre-created traits. */
	static UArcMassEntityConfigTemplate* MakeTemplate(const TArray<UMassEntityTraitBase*>& InitialTraits)
	{
		UArcMassEntityConfigTemplate* Tmpl = NewObject<UArcMassEntityConfigTemplate>(GetTransientPackage());
		// Assign a GUID so EnsurePrimaryAssetId / GetPrimaryAssetId works without touching the asset manager.
		Tmpl->TemplateAssetId = FGuid::NewGuid();
		TArray<TObjectPtr<UMassEntityTraitBase>>* TraitsArray =
			ArcMassConfig::GetMutableTraitsArray(Tmpl->GetMutableConfig());
		check(TraitsArray);
		for (UMassEntityTraitBase* Trait : InitialTraits)
		{
			TraitsArray->Add(Trait);
		}
		return Tmpl;
	}

	/** Return the current number of traits in a config's own list. */
	static int32 TraitCount(UArcMassEntityConfigAsset* Config)
	{
		return Config->GetConfig().GetTraits().Num();
	}
} // namespace ArcMassConfigMergeTests_Private

// ─────────────────────────────────────────────────────────────────────────────
// Test 1 — SetTemplate additive, no dupes
// ─────────────────────────────────────────────────────────────────────────────

TEST_CLASS(ArcMassConfigMerge_SetTemplate, "ArcMassConfig.Merge")
{
	TEST_METHOD(Additive_NoDupes_VariantTraitAKept_TemplateBAdded)
	{
		// Config starts with TraitA (Value=0).
		UArcMassConfigTestTraitA* VarA = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		VarA->Value = 0;
		UArcMassEntityConfigAsset* Config =
			ArcMassConfigMergeTests_Private::MakeConfig({ VarA });

		// Template has TraitA (Value=1) + TraitB.
		UArcMassConfigTestTraitA* TmplA = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		TmplA->Value = 1;
		UArcMassConfigTestTraitB* TmplB = NewObject<UArcMassConfigTestTraitB>(GetTransientPackage());
		UArcMassEntityConfigTemplate* Tmpl =
			ArcMassConfigMergeTests_Private::MakeTemplate({ TmplA, TmplB });

		Tmpl->SetTemplate(Config);

		// Exactly one TraitA (variant's kept; template's was already-present class, not appended).
		ASSERT_THAT(AreEqual(1, ArcMassConfigMergeTests_Private::CountTraitsByClass(
			Config, UArcMassConfigTestTraitA::StaticClass())));

		// Exactly one TraitB (new from template).
		ASSERT_THAT(AreEqual(1, ArcMassConfigMergeTests_Private::CountTraitsByClass(
			Config, UArcMassConfigTestTraitB::StaticClass())));

		// No class appears twice.
		ASSERT_THAT(IsTrue(ArcMassConfigMergeTests_Private::HasNoDuplicateTraitClasses(Config)));
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Test 2 — SetNewOrReplaceTemplate template-wins, no dupes
// ─────────────────────────────────────────────────────────────────────────────

TEST_CLASS(ArcMassConfigMerge_SetNewOrReplace, "ArcMassConfig.Merge")
{
	TEST_METHOD(TemplateWinsOnCollision_NoDupes_VariantOnlyPreserved)
	{
		// Config starts with TraitA(Value=5) + TraitB.
		UArcMassConfigTestTraitA* VarA = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		VarA->Value = 5;
		UArcMassConfigTestTraitB* VarB = NewObject<UArcMassConfigTestTraitB>(GetTransientPackage());
		UArcMassEntityConfigAsset* Config =
			ArcMassConfigMergeTests_Private::MakeConfig({ VarA, VarB });

		// Empty BaseSnapshot — first assignment scenario.
		// Template has TraitA(Value=9) only.
		UArcMassConfigTestTraitA* TmplA = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		TmplA->Value = 9;
		UArcMassEntityConfigTemplate* Tmpl =
			ArcMassConfigMergeTests_Private::MakeTemplate({ TmplA });

		Tmpl->SetNewOrReplaceTemplate(Config);

		// Exactly one TraitA — the template's (Value=9).
		ASSERT_THAT(AreEqual(1, ArcMassConfigMergeTests_Private::CountTraitsByClass(
			Config, UArcMassConfigTestTraitA::StaticClass())));

		TConstArrayView<UMassEntityTraitBase*> Traits = Config->GetConfig().GetTraits();
		const UArcMassConfigTestTraitA* WinnerA = nullptr;
		for (UMassEntityTraitBase* Trait : Traits)
		{
			if (Trait && Trait->GetClass() == UArcMassConfigTestTraitA::StaticClass())
			{
				WinnerA = Cast<UArcMassConfigTestTraitA>(Trait);
				break;
			}
		}
		ASSERT_THAT(IsNotNull(WinnerA));
		// Template wins — value must be 9.
		ASSERT_THAT(AreEqual(9, WinnerA->Value));

		// TraitB is variant-only (not in new template, not in prior snapshot) — preserved.
		ASSERT_THAT(AreEqual(1, ArcMassConfigMergeTests_Private::CountTraitsByClass(
			Config, UArcMassConfigTestTraitB::StaticClass())));

		// No duplicates.
		ASSERT_THAT(IsTrue(ArcMassConfigMergeTests_Private::HasNoDuplicateTraitClasses(Config)));
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Test 3 — First-assign with empty BaseSnapshot, no dupes (regression: Bug A)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CLASS(ArcMassConfigMerge_FirstAssign, "ArcMassConfig.Merge")
{
	TEST_METHOD(EmptySnapshot_AdditiveSetTemplate_NoDupes)
	{
		// Config has TraitA + TraitB hand-authored; BaseSnapshot is empty (never set by template).
		UArcMassConfigTestTraitA* VarA = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		UArcMassConfigTestTraitB* VarB = NewObject<UArcMassConfigTestTraitB>(GetTransientPackage());
		UArcMassEntityConfigAsset* Config =
			ArcMassConfigMergeTests_Private::MakeConfig({ VarA, VarB });

		// BaseSnapshot is empty by default — no prior template link.
		ASSERT_THAT(AreEqual(0, Config->BaseSnapshot.Num()));

		// Template has TraitA only.
		UArcMassConfigTestTraitA* TmplA = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		UArcMassEntityConfigTemplate* Tmpl =
			ArcMassConfigMergeTests_Private::MakeTemplate({ TmplA });

		// SetTemplate — additive stamp. TraitA already present, so not added again.
		Tmpl->SetTemplate(Config);

		// One TraitA, one TraitB, no dupes — the regression guard for Bug A.
		ASSERT_THAT(AreEqual(1, ArcMassConfigMergeTests_Private::CountTraitsByClass(
			Config, UArcMassConfigTestTraitA::StaticClass())));
		ASSERT_THAT(AreEqual(1, ArcMassConfigMergeTests_Private::CountTraitsByClass(
			Config, UArcMassConfigTestTraitB::StaticClass())));
		ASSERT_THAT(IsTrue(ArcMassConfigMergeTests_Private::HasNoDuplicateTraitClasses(Config)));
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Test 4 — UpdateFromTemplate
// ─────────────────────────────────────────────────────────────────────────────

TEST_CLASS(ArcMassConfigMerge_UpdateFromTemplate, "ArcMassConfig.Merge")
{
	TEST_METHOD(NewTraitInTemplate_PropagatedToConfig)
	{
		// Set up: config linked to template with TraitA only.
		UArcMassConfigTestTraitA* TmplA = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		UArcMassEntityConfigTemplate* Tmpl =
			ArcMassConfigMergeTests_Private::MakeTemplate({ TmplA });

		UArcMassEntityConfigAsset* Config =
			ArcMassConfigMergeTests_Private::MakeConfig({});
		Tmpl->SetTemplate(Config);

		// Config now has TraitA, BaseSnapshot has TraitA.
		ASSERT_THAT(AreEqual(1, ArcMassConfigMergeTests_Private::CountTraitsByClass(
			Config, UArcMassConfigTestTraitA::StaticClass())));

		// Add TraitB to the template.
		UArcMassConfigTestTraitB* TmplB = NewObject<UArcMassConfigTestTraitB>(GetTransientPackage());
		TArray<TObjectPtr<UMassEntityTraitBase>>* TmplTraits =
			ArcMassConfig::GetMutableTraitsArray(Tmpl->GetMutableConfig());
		check(TmplTraits);
		TmplTraits->Add(TmplB);

		Tmpl->UpdateFromTemplate(Config);

		// TraitB propagated to config.
		ASSERT_THAT(AreEqual(1, ArcMassConfigMergeTests_Private::CountTraitsByClass(
			Config, UArcMassConfigTestTraitB::StaticClass())));
		ASSERT_THAT(IsTrue(ArcMassConfigMergeTests_Private::HasNoDuplicateTraitClasses(Config)));
	}

	TEST_METHOD(VariantOverride_Preserved_WhenTemplateChanges)
	{
		// Config has TraitA(Value=7). Template has TraitA(Value=0). Link via SetTemplate.
		UArcMassConfigTestTraitA* TmplA_V0 = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		TmplA_V0->Value = 0;
		UArcMassEntityConfigTemplate* Tmpl =
			ArcMassConfigMergeTests_Private::MakeTemplate({ TmplA_V0 });

		// Config's TraitA starts at Value=7 (variant override).
		UArcMassConfigTestTraitA* VarA = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		VarA->Value = 7;
		UArcMassEntityConfigAsset* Config =
			ArcMassConfigMergeTests_Private::MakeConfig({ VarA });
		// Establish link + snapshot via SetTemplate (additive — TraitA already present, kept).
		Tmpl->SetTemplate(Config);

		// Now change template's TraitA to Value=99.
		TConstArrayView<UMassEntityTraitBase*> TmplTraits = Tmpl->GetConfig().GetTraits();
		UArcMassConfigTestTraitA* MutableTmplA = nullptr;
		for (UMassEntityTraitBase* T : TmplTraits)
		{
			MutableTmplA = Cast<UArcMassConfigTestTraitA>(T);
			if (MutableTmplA)
			{
				break;
			}
		}
		ASSERT_THAT(IsNotNull(MutableTmplA));
		MutableTmplA->Value = 99;

		// Snapshot's TraitA still holds Value=0 (from SetTemplate time).
		// Variant's TraitA is Value=7 — different from snapshot(0) → variant overrode.
		// UpdateFromTemplate must keep variant's value (7), not replace with template's (99).
		Tmpl->UpdateFromTemplate(Config);

		TConstArrayView<UMassEntityTraitBase*> ResultTraits = Config->GetConfig().GetTraits();
		const UArcMassConfigTestTraitA* ResultA = nullptr;
		for (UMassEntityTraitBase* T : ResultTraits)
		{
			ResultA = Cast<UArcMassConfigTestTraitA>(T);
			if (ResultA)
			{
				break;
			}
		}
		ASSERT_THAT(IsNotNull(ResultA));
		// Variant override must be preserved.
		ASSERT_THAT(AreEqual(7, ResultA->Value));
	}

	TEST_METHOD(DeterministicOrder_RunTwice_SameClassOrder)
	{
		// Set up config linked to template with TraitA + TraitB.
		UArcMassConfigTestTraitA* TmplA = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		UArcMassConfigTestTraitB* TmplB = NewObject<UArcMassConfigTestTraitB>(GetTransientPackage());
		UArcMassEntityConfigTemplate* Tmpl =
			ArcMassConfigMergeTests_Private::MakeTemplate({ TmplA, TmplB });

		UArcMassEntityConfigAsset* Config =
			ArcMassConfigMergeTests_Private::MakeConfig({});
		Tmpl->SetTemplate(Config);

		// First UpdateFromTemplate — capture class order.
		Tmpl->UpdateFromTemplate(Config);

		TArray<UClass*> OrderAfterFirst;
		for (UMassEntityTraitBase* Trait : Config->GetConfig().GetTraits())
		{
			if (Trait)
			{
				OrderAfterFirst.Add(Trait->GetClass());
			}
		}

		// Second UpdateFromTemplate — must produce identical class order.
		Tmpl->UpdateFromTemplate(Config);

		TArray<UClass*> OrderAfterSecond;
		for (UMassEntityTraitBase* Trait : Config->GetConfig().GetTraits())
		{
			if (Trait)
			{
				OrderAfterSecond.Add(Trait->GetClass());
			}
		}

		ASSERT_THAT(AreEqual(OrderAfterFirst.Num(), OrderAfterSecond.Num()));
		for (int32 Idx = 0; Idx < OrderAfterFirst.Num(); ++Idx)
		{
			ASSERT_THAT(IsTrue(OrderAfterFirst[Idx] == OrderAfterSecond[Idx]));
		}
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Test 5 — PostDuplicate keeps BaseSnapshot + regenerates ConfigAssetId
// ─────────────────────────────────────────────────────────────────────────────

TEST_CLASS(ArcMassConfigMerge_PostDuplicate, "ArcMassConfig.Merge")
{
	TEST_METHOD(Duplicate_PreservesBaseSnapshot_RegeneratesConfigAssetId)
	{
		// Set up a linked config so BaseSnapshot is non-empty.
		UArcMassConfigTestTraitA* TmplA = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		UArcMassConfigTestTraitB* TmplB = NewObject<UArcMassConfigTestTraitB>(GetTransientPackage());
		UArcMassEntityConfigTemplate* Tmpl =
			ArcMassConfigMergeTests_Private::MakeTemplate({ TmplA, TmplB });

		UArcMassEntityConfigAsset* Config =
			ArcMassConfigMergeTests_Private::MakeConfig({});
		Tmpl->SetTemplate(Config);

		// Ensure ConfigAssetId is assigned (PostLoad does this, but we're in-memory).
		if (!Config->ConfigAssetId.IsValid())
		{
			Config->RegenerateConfigAssetId();
		}

		const int32 SnapshotCount = Config->BaseSnapshot.Num();
		ASSERT_THAT(IsTrue(SnapshotCount > 0));

		const FGuid OriginalId = Config->ConfigAssetId;

		UArcMassEntityConfigAsset* Dupe = DuplicateObject<UArcMassEntityConfigAsset>(
			Config, GetTransientPackage());
		ASSERT_THAT(IsNotNull(Dupe));

		// BaseSnapshot faithful copy — same count.
		ASSERT_THAT(AreEqual(SnapshotCount, Dupe->BaseSnapshot.Num()));

		// ConfigAssetId regenerated — must differ from source.
		// (PostDuplicate regenerates the id for non-PIE duplicates.)
		// Note: DuplicateObject triggers PostDuplicate automatically via UObject machinery.
		ASSERT_THAT(IsTrue(Dupe->ConfigAssetId != OriginalId));
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Test 6 — UpdateFromTemplate identity guard
// ─────────────────────────────────────────────────────────────────────────────

TEST_CLASS(ArcMassConfigMerge_IdentityGuard, "ArcMassConfig.Merge")
{
	TEST_METHOD(WrongTemplate_EarlyReturn_TraitsUnchanged)
	{
		// Link config to Template T1.
		UArcMassConfigTestTraitA* T1A = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		UArcMassEntityConfigTemplate* T1 =
			ArcMassConfigMergeTests_Private::MakeTemplate({ T1A });

		UArcMassEntityConfigAsset* Config =
			ArcMassConfigMergeTests_Private::MakeConfig({});
		T1->SetTemplate(Config);

		const int32 TraitCountBefore = ArcMassConfigMergeTests_Private::TraitCount(Config);

		// Create a different template T2 with TraitB.
		UArcMassConfigTestTraitB* T2B = NewObject<UArcMassConfigTestTraitB>(GetTransientPackage());
		UArcMassEntityConfigTemplate* T2 =
			ArcMassConfigMergeTests_Private::MakeTemplate({ T2B });

		// Calling T2->UpdateFromTemplate(Config) should early-return because
		// Config->SourceTemplateId points to T1, not T2.
		T2->UpdateFromTemplate(Config);

		// Trait list must be unchanged.
		const int32 TraitCountAfter = ArcMassConfigMergeTests_Private::TraitCount(Config);
		ASSERT_THAT(AreEqual(TraitCountBefore, TraitCountAfter));

		// TraitB from T2 must NOT have been added.
		ASSERT_THAT(AreEqual(0, ArcMassConfigMergeTests_Private::CountTraitsByClass(
			Config, UArcMassConfigTestTraitB::StaticClass())));
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Test 7 — IsDataValid flags duplicate trait class
// ─────────────────────────────────────────────────────────────────────────────

TEST_CLASS(ArcMassConfigMerge_IsDataValid, "ArcMassConfig.Merge")
{
	TEST_METHOD(DuplicateTraitClass_ProducesWarning)
	{
		// Hand-build a config with two TraitA entries via the exported seam.
		UArcMassConfigTestTraitA* TraitA1 = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		UArcMassConfigTestTraitA* TraitA2 = NewObject<UArcMassConfigTestTraitA>(GetTransientPackage());
		UArcMassEntityConfigAsset* Config =
			ArcMassConfigMergeTests_Private::MakeConfig({ TraitA1, TraitA2 });

		FDataValidationContext ValidationContext;
		Config->IsDataValid(ValidationContext);

		// At least one warning must be recorded for the duplicate class.
		ASSERT_THAT(IsTrue(ValidationContext.GetNumWarnings() > 0));
	}
};

#endif // WITH_EDITORONLY_DATA
