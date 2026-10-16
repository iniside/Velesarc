// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanicsEditor/Validation/ArcDamageTypeValidator.h"

#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Combat/ArcCombatFunctions.h"
#include "ArcMassMechanics/Combat/ArcDamageExecutionBase.h"
#include "Effects/ArcEffectDefinition.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ArcDamageTypeValidator"

namespace ArcDamageTypeValidatorPrivate
{
	TArray<const FArcDamageExecutionBase*> FindDamageExecutions(const UArcEffectDefinition& Definition)
	{
		TArray<const FArcDamageExecutionBase*> Found;
		for (const FArcEffectExecutionEntry& Entry : Definition.Executions)
		{
			if (const FArcDamageExecutionBase* Execution = Entry.CustomExecution.GetPtr<FArcDamageExecutionBase>())
			{
				Found.Add(Execution);
			}
		}
		return Found;
	}
}

bool UArcDamageTypeValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	return InObject && InObject->IsA<UArcEffectDefinition>();
}

EDataValidationResult UArcDamageTypeValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	using namespace ArcDamageTypeValidatorPrivate;

	const UArcEffectDefinition* Definition = CastChecked<UArcEffectDefinition>(InAsset);
	const FGameplayTag Root = ArcMassMechanicsTags::DamageType;
	FGameplayTagContainer DamageTypes = Definition->GetEffectTags().Filter(FGameplayTagContainer(Root));
	DamageTypes.RemoveTag(Root);

	const TArray<const FArcDamageExecutionBase*> DamageExecutions = FindDamageExecutions(*Definition);

	bool bFailed = false;
	if (DamageExecutions.Num() > 1)
	{
		AssetFails(InAsset, FText::Format(LOCTEXT("ManyExecutions", "Effect has {0} damage executions. One effect is one hit (D-96); author each extra hit as its own effect."),
			FText::AsNumber(DamageExecutions.Num())));
		bFailed = true;
	}

	if (DamageTypes.Num() > 1)
	{
		AssetFails(InAsset, FText::Format(LOCTEXT("ManyTypes", "Effect Tags carry several damage types ({0}). One damage effect is one damage type; author each extra type as its own effect."),
			FText::FromString(DamageTypes.ToStringSimple())));
		bFailed = true;
	}
	else if (DamageTypes.Num() == 1 && !ArcMechCombat::IsLeafDamageType(DamageTypes.First()))
	{
		AssetFails(InAsset, FText::Format(LOCTEXT("NotLeaf", "Damage type {0} has child types; pick a concrete (leaf) ArcEffect.Damage tag."),
			FText::FromName(DamageTypes.First().GetTagName())));
		bFailed = true;
	}
	else if (DamageTypes.IsEmpty() && !DamageExecutions.IsEmpty())
	{
		AssetFails(InAsset, LOCTEXT("NoType", "A damage execution needs one leaf ArcEffect.Damage tag in Effect Tags; it is the hit's damage type."));
		bFailed = true;
	}

	for (const FArcDamageExecutionBase* Execution : DamageExecutions)
	{
		TArray<FText> Warnings;
		Execution->GatherDataWarnings(Warnings);
		for (const FText& Warning : Warnings)
		{
			AssetWarning(InAsset, Warning);
		}
	}

	if (!bFailed)
	{
		AssetPasses(InAsset);
	}
	return GetValidationResult();
}

#undef LOCTEXT_NAMESPACE
