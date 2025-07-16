/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */



#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeReference.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimNode_ChooserPlayer.h"
#include "ArcAnimNodeLibrary.generated.h"

USTRUCT(BlueprintType)
struct FChooserPlayerAnimNodeReference : public FAnimNodeReference
{
	GENERATED_BODY()

	typedef FAnimNode_ChooserPlayer FInternalNodeType;
};

/**
 * 
 */
UCLASS()
class ARCCORE_API UArcAnimNodeLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Get a blend stack node context from an anim node context */
	UFUNCTION(BlueprintCallable, Category = "Animation|Chooser", meta = (BlueprintThreadSafe, ExpandEnumAsExecs = "Result"))
	static FChooserPlayerAnimNodeReference ConvertToChoosePlayerNode(const FAnimNodeReference& Node, EAnimNodeReferenceConversionResult& Result);

	UFUNCTION(BlueprintCallable, Category = "Animation|Chooser", meta = (BlueprintThreadSafe))
	static void SetChooser(const FAnimNodeReference& Node, UChooserTable* ChooserTable);
};
