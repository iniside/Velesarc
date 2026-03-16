// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "InteractionTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcInteractableInterfaceLibrary.generated.h"

UCLASS()
class ARCCORE_API UArcInteractableInterfaceLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, CustomThunk, BlueprintInternalUseOnly, Category = "Arc Core", meta = (CustomStructureParam = "InteractionTargetConfiguration", ExpandBoolAsExecs = "ReturnValue"))
	static bool GetInteractionTargetConfiguration(const FInteractionQueryResults& Item
													  , UPARAM(meta = (MetaStruct = "/Script/InteractableInterface.InteractionTargetConfiguration")) UScriptStruct* InConfigType
													  , int32& InteractionTargetConfiguration);

	DECLARE_FUNCTION(execGetInteractionTargetConfiguration);
};
