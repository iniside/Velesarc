// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectTypes.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "NativeGameplayTags.h"
#include "StructUtils/SharedStruct.h"
#include "ArcScalableFloat.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SpecTest_Damage, "Test.Spec.Damage");

TEST_CLASS(ArcEffectSpec_Creation, "ArcMassAbilities.EffectSpec")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
	}

	TEST_METHOD(MakeSpec_SimpleMagnitude_ResolvesFromScalableFloat)
	{
		UArcEffectDefinition* Def = NewObject<UArcEffectDefinition>();
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Type = EArcModifierType::Simple;
		Mod.Magnitude = FArcScalableFloat(25.f);
		Def->Modifiers.Add(Mod);

		FArcEffectContext Ctx;
		Ctx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(Def, Ctx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));

		ASSERT_THAT(AreEqual(Spec->ResolvedMagnitudes.Num(), 1));
		ASSERT_THAT(IsNear(Spec->ResolvedMagnitudes[0], 25.f, 0.001f));
	}

	TEST_METHOD(MakeSpec_SetByCaller_MissingTag_ResolvesToZero)
	{
		UArcEffectDefinition* Def = NewObject<UArcEffectDefinition>();
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Type = EArcModifierType::SetByCaller;
		Mod.SetByCallerTag = TAG_SpecTest_Damage;
		Def->Modifiers.Add(Mod);

		FArcEffectContext Ctx;
		Ctx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(Def, Ctx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));

		ASSERT_THAT(IsNear(Spec->ResolvedMagnitudes[0], 0.f, 0.001f));
	}

	TEST_METHOD(MakeSpec_MultipleModifiers_ResolvesAll)
	{
		UArcEffectDefinition* Def = NewObject<UArcEffectDefinition>();

		FArcEffectModifier Mod1;
		Mod1.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod1.Operation = EArcModifierOp::Add;
		Mod1.Type = EArcModifierType::Simple;
		Mod1.Magnitude = FArcScalableFloat(10.f);
		Def->Modifiers.Add(Mod1);

		FArcEffectModifier Mod2;
		Mod2.Attribute = FArcAttributeSet_TestStats::GetArmorAttribute();
		Mod2.Operation = EArcModifierOp::MultiplyCompound;
		Mod2.Type = EArcModifierType::Simple;
		Mod2.Magnitude = FArcScalableFloat(2.f);
		Def->Modifiers.Add(Mod2);

		FArcEffectContext Ctx;
		Ctx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(Def, Ctx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));

		ASSERT_THAT(AreEqual(Spec->ResolvedMagnitudes.Num(), 2));
		ASSERT_THAT(IsNear(Spec->ResolvedMagnitudes[0], 10.f, 0.001f));
		ASSERT_THAT(IsNear(Spec->ResolvedMagnitudes[1], 2.f, 0.001f));
	}
};
