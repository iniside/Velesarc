// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcBodyUtilityConsiderations.h"

#include "Fragments/ArcMassItemStoreFragment.h"
#include "Fragments/ArcNeedFragment.h"
#include "Items/ArcItemData.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Sim/ArcBodyFragments.h"
#include "Sim/ArcBodyUtilityFragments.h"
#include "Sim/ArcWorldSimTags.h"
#include "UtilityAI/Considerations/ArcUtilityConsideration_Need.h"

namespace ArcBodyUtilityConsiderationsPrivate
{
bool HasPhysicalCargo(FMassEntityManager& EntityManager, const FMassEntityHandle Entity)
{
	if (!EntityManager.IsEntityValid(Entity))
	{
		return false;
	}

	const FMassEntityView EntityView(EntityManager, Entity);
	const FArcMassItemStoreFragment* Store = EntityView.GetSparseFragmentDataPtr<FArcMassItemStoreFragment>();
	if (!Store)
	{
		return false;
	}

	for (const FArcMassReplicatedItem& Item : Store->ReplicatedItems.Items)
	{
		const FArcItemData* Data = Item.ToItem();
		if (Data && Data->GetItemDefinitionId().IsValid() && Data->GetStacks() > 0)
		{
			return true;
		}
	}
	return false;
}

void AuthorLinearCurve(FArcResponseCurve& ResponseCurve, TConstArrayView<FVector2f> Points)
{
	ResponseCurve.Type = EArcResponseCurveType::Custom;
	FRichCurve* Curve = ResponseCurve.CurveInfo.GetRichCurve();
	Curve->Reset();
	for (const FVector2f& Point : Points)
	{
		const FKeyHandle Handle = Curve->AddKey(Point.X, Point.Y);
		Curve->SetKeyInterpMode(Handle, RCIM_Linear);
	}
}

FArcBodyUtilityOption MakeContinueOption()
{
	FArcUtilityConsideration_BodyContinuation Consideration;

	FArcBodyUtilityOption Option;
	Option.GoalTag = ArcWorldSimTags::Need_Continue;
	Option.Entry.Weight = 1.0f;
	Option.Entry.Considerations.Add(
		FInstancedStruct::Make<FArcUtilityConsideration_BodyContinuation>(Consideration));
	return Option;
}

FArcBodyUtilityOption MakeNeedOption(
	const FGameplayTag GoalTag,
	UScriptStruct* NeedType,
	TConstArrayView<FVector2f> CurvePoints)
{
	FArcUtilityConsideration_OwnerNeed Consideration;
	Consideration.NeedType = NeedType;
	Consideration.MaxNeedValue = 100.0f;
	AuthorLinearCurve(Consideration.ResponseCurve, CurvePoints);

	FArcBodyUtilityOption Option;
	Option.GoalTag = GoalTag;
	Option.Entry.Weight = 1.0f;
	Option.Entry.Considerations.Add(FInstancedStruct::Make<FArcUtilityConsideration_OwnerNeed>(Consideration));
	return Option;
}
}

FArcBodyUtilityConfigFragment::FArcBodyUtilityConfigFragment()
{
	const FVector2f HungerCurve[] = {
		FVector2f(0.0f, 0.0f),
		FVector2f(0.30f, 0.02f),
		FVector2f(0.60f, 0.40f),
		FVector2f(0.80f, 0.85f),
		FVector2f(1.0f, 1.0f)
	};
	const FVector2f FatigueCurve[] = {
		FVector2f(0.0f, 0.0f),
		FVector2f(0.20f, 0.01f),
		FVector2f(0.60f, 0.35f),
		FVector2f(0.80f, 0.90f),
		FVector2f(1.0f, 1.0f)
	};

	Options.Add(ArcBodyUtilityConsiderationsPrivate::MakeContinueOption());
	Options.Add(ArcBodyUtilityConsiderationsPrivate::MakeNeedOption(
		ArcWorldSimTags::Need_Hunger, FArcHungerNeedFragment::StaticStruct(), HungerCurve));
	Options.Add(ArcBodyUtilityConsiderationsPrivate::MakeNeedOption(
		ArcWorldSimTags::Need_Fatigue, FArcFatigueNeedFragment::StaticStruct(), FatigueCurve));
}

FArcUtilityConsiderationResult FArcUtilityConsideration_BodyContinuation::Score(
	const FArcUtilityTarget& Target,
	const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Context.EntityManager->IsEntityValid(Context.QuerierEntity))
	{
		return FArcUtilityConsiderationResult::Accept(0.05f);
	}

	if (ArcBodyUtilityConsiderationsPrivate::HasPhysicalCargo(*Context.EntityManager, Context.QuerierEntity))
	{
		return FArcUtilityConsiderationResult::Accept(1.0f);
	}

	const FArcBodyFragment* Body = Context.EntityManager->GetFragmentDataPtr<FArcBodyFragment>(Context.QuerierEntity);
	if (!Body || !Body->Command.IsActive())
	{
		return FArcUtilityConsiderationResult::Accept(0.05f);
	}
	return FArcUtilityConsiderationResult::Accept(Body->Command.bInterruptible ? 0.30f : 1.0f);
}
