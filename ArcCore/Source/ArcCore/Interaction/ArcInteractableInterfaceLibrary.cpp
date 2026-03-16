// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInteractableInterfaceLibrary.h"

bool UArcInteractableInterfaceLibrary::GetInteractionTargetConfiguration(const FInteractionQueryResults& Item
							   , UScriptStruct* InConfigType
							   , int32& OutFragment)
{
	checkNoEntry();
	return false;
}

DEFINE_FUNCTION(UArcInteractableInterfaceLibrary::execGetInteractionTargetConfiguration)
{
	P_GET_STRUCT(FInteractionQueryResults, Item);
	P_GET_OBJECT(UScriptStruct, InConfigType);

	Stack.MostRecentPropertyAddress = nullptr;
	Stack.StepCompiledIn<FStructProperty>(nullptr);

	void* OutItemDataPtr = Stack.MostRecentPropertyAddress;
	FStructProperty* OutItemProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;
	bool bSuccess = true;

	P_NATIVE_BEGIN;
	for (const FInstancedStruct& Interaction : Item.AvailableInteractions)
	{
		if (Interaction.GetScriptStruct() && Interaction.GetScriptStruct() == InConfigType)
		{
			OutItemProp->Struct->CopyScriptStruct(OutItemDataPtr, Interaction.GetMemory());
			bSuccess = true;
			break;
		}
	}
	P_NATIVE_END;

	*(bool*)RESULT_PARAM = bSuccess;
}
