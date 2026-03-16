// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcScalableFloatItemFragment_TargetingTrace.h"

#if WITH_EDITOR
#include "ArcScalableFloatItemFragment_TargetingShape.h"
#include "Items/ArcItemDefinition.h"
#include "Misc/DataValidation.h"

EDataValidationResult FArcScalableFloatItemFragment_TargetingTrace::IsDataValid(const UArcItemDefinition* ItemDefinition, FDataValidationContext& Context) const
{
	EDataValidationResult Result = EDataValidationResult::Valid;

	if (!ItemDefinition->GetScalableFloatFragment<FArcScalableFloatItemFragment_TargetingShape>())
	{
		Context.AddError(FText::FromString(TEXT("Targeting Trace Stats requires Targeting Shape Stats fragment on the same item.")));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
