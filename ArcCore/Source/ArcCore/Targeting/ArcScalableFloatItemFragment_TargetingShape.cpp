// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcScalableFloatItemFragment_TargetingShape.h"

#if WITH_EDITOR
#include "ArcScalableFloatItemFragment_TargetingTrace.h"
#include "Items/ArcItemDefinition.h"
#include "Misc/DataValidation.h"

EDataValidationResult FArcScalableFloatItemFragment_TargetingShape::IsDataValid(const UArcItemDefinition* ItemDefinition, FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!ItemDefinition->GetScalableFloatFragment<FArcScalableFloatItemFragment_TargetingTrace>())
	{
		Context.AddError(FText::FromString(TEXT("Targeting Shape Stats requires Targeting Trace Stats fragment on the same item.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
