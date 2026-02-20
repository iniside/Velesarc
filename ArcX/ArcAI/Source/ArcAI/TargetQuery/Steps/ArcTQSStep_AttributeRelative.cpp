// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_AttributeRelative.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"

namespace ArcTQSAttributeRelativeHelper
{
	static UAbilitySystemComponent* GetASCFromActor(AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Actor);
		if (!ASI)
		{
			return nullptr;
		}

		return ASI->GetAbilitySystemComponent();
	}

	static bool GetNormalizedAttribute(
		UAbilitySystemComponent* ASC,
		const FGameplayAttribute& Attribute,
		bool bUseMaxAttribute,
		const FGameplayAttribute& MaxAttribute,
		float ManualMaxValue,
		float& OutNormalized)
	{
		if (!ASC || !Attribute.IsValid())
		{
			return false;
		}

		const float Value = ASC->GetNumericAttribute(Attribute);

		float MaxValue;
		if (bUseMaxAttribute && MaxAttribute.IsValid())
		{
			MaxValue = ASC->GetNumericAttribute(MaxAttribute);
			if (MaxValue <= 0.0f)
			{
				return false;
			}
		}
		else
		{
			MaxValue = ManualMaxValue;
		}

		OutNormalized = FMath::Clamp(Value / MaxValue, 0.0f, 1.0f);
		return true;
	}
}

float FArcTQSStep_AttributeRelative::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	using namespace ArcTQSAttributeRelativeHelper;

	// Resolve target ASC
	AActor* TargetActor = const_cast<FArcTQSTargetItem&>(Item).GetActor(QueryContext.EntityManager);
	UAbilitySystemComponent* TargetASC = GetASCFromActor(TargetActor);
	if (!TargetASC)
	{
		return bFilterItemsWithoutAttribute ? 0.0f : DefaultScore;
	}

	// Get target normalized value
	float TargetNorm = 0.0f;
	if (!GetNormalizedAttribute(TargetASC, TargetAttribute, bUseTargetMaxAttribute,
		TargetMaxAttribute, TargetManualMaxValue, TargetNorm))
	{
		return bFilterItemsWithoutAttribute ? 0.0f : DefaultScore;
	}

	// Resolve querier ASC
	AActor* QuerierActor = QueryContext.QuerierActor.Get();
	UAbilitySystemComponent* QuerierASC = GetASCFromActor(QuerierActor);
	if (!QuerierASC)
	{
		return bFilterItemsWithoutAttribute ? 0.0f : DefaultScore;
	}

	// Get querier normalized value
	float QuerierNorm = 0.0f;
	if (!GetNormalizedAttribute(QuerierASC, QuerierAttribute, bUseQuerierMaxAttribute,
		QuerierMaxAttribute, QuerierManualMaxValue, QuerierNorm))
	{
		return bFilterItemsWithoutAttribute ? 0.0f : DefaultScore;
	}

	// Combine values based on comparison mode
	float RawScore;
	switch (CompareMode)
	{
	case EArcTQSAttributeCompareMode::Ratio:
	{
		const float Sum = TargetNorm + QuerierNorm;
		RawScore = (Sum > KINDA_SMALL_NUMBER) ? (TargetNorm / Sum) : 0.5f;
		break;
	}
	case EArcTQSAttributeCompareMode::Difference:
	{
		// Maps difference from [-1,1] to [0,1]
		RawScore = (TargetNorm - QuerierNorm + 1.0f) * 0.5f;
		break;
	}
	default:
		RawScore = 0.5f;
		break;
	}

	return ResponseCurve.Evaluate(RawScore);
}
