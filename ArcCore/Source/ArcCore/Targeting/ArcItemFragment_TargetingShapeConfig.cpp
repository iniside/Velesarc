// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcItemFragment_TargetingShapeConfig.h"

#if WITH_EDITOR
#include "ArcScalableFloatItemFragment_TargetingShape.h"
#include "Items/ArcItemDefinition.h"
#include "Misc/DataValidation.h"

EDataValidationResult FArcItemFragment_TargetingShapeConfig::IsDataValid(const UArcItemDefinition* ItemDefinition, FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!ItemDefinition->GetScalableFloatFragment<FArcScalableFloatItemFragment_TargetingShape>())
	{
		Context.AddError(FText::FromString(TEXT("Targeting Shape Config requires Targeting Shape Stats scalable float fragment on the same item.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
