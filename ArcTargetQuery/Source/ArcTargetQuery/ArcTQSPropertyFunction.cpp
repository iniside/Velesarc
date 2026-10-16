// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSPropertyFunction.h"

#include "UObject/UnrealType.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTQSPropertyFunction)

namespace ArcTQS
{
#if WITH_EDITOR
	bool IsFunctionOutputProperty(const FProperty& Property)
	{
		return Property.HasMetaData(TEXT("ArcTQSOutput"));
	}

	const FProperty* FindFunctionOutputProperty(const UStruct* FunctionStruct, int32* OutNumOutputs)
	{
		const FProperty* Output = nullptr;
		int32 NumOutputs = 0;
		if (FunctionStruct)
		{
			for (TFieldIterator<FProperty> PropertyIt(FunctionStruct); PropertyIt; ++PropertyIt)
			{
				if (IsFunctionOutputProperty(**PropertyIt))
				{
					Output = Output ? Output : *PropertyIt;
					++NumOutputs;
				}
			}
		}
		if (OutNumOutputs)
		{
			*OutNumOutputs = NumOutputs;
		}
		return Output;
	}
#endif
}
