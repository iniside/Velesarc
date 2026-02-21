/**
 * This file is part of Velesarc
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

#include "Commands/ArcReplicatedCommand.h"
#include "UObject/ObjectPtr.h"

#include "ArcStartRecipeCraftCommand.generated.h"

class UArcCraftComponent;
class UArcRecipeDefinition;

/**
 * Replicated command for initiating recipe-based crafting.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcStartRecipeCraftCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcCraftComponent> CraftComponent = nullptr;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcRecipeDefinition> Recipe = nullptr;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UObject> Instigator = nullptr;

	UPROPERTY(BlueprintReadWrite)
	int32 Amount = 1;

	UPROPERTY(BlueprintReadWrite)
	int32 Priority = 0;

public:
	virtual bool CanSendCommand() const override;
	virtual void PreSendCommand() override;
	virtual bool Execute() override;

	FArcStartRecipeCraftCommand()
		: CraftComponent(nullptr)
		, Recipe(nullptr)
		, Instigator(nullptr)
		, Amount(1)
		, Priority(0)
	{
	}

	FArcStartRecipeCraftCommand(
		UArcCraftComponent* InCraftComponent,
		UArcRecipeDefinition* InRecipe,
		UObject* InInstigator,
		int32 InAmount = 1,
		int32 InPriority = 0)
		: CraftComponent(InCraftComponent)
		, Recipe(InRecipe)
		, Instigator(InInstigator)
		, Amount(InAmount)
		, Priority(InPriority)
	{
	}

	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcStartRecipeCraftCommand::StaticStruct();
	}

	virtual ~FArcStartRecipeCraftCommand() override = default;
};
