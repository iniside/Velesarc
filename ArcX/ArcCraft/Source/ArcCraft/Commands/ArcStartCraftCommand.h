// Copyright Lukasz Baran. All Rights Reserved.

#pragma once
#include "Commands/ArcReplicatedCommand.h"

#include "ArcStartCraftCommand.generated.h"

class UArcCraftComponent;
class UArcItemDefinition;

/**
 * 
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcStartCraftCommand : public FArcReplicatedCommand
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcCraftComponent> CraftComponent = nullptr;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UArcItemDefinition> RecipeItem = nullptr;

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


	FArcStartCraftCommand()
		: CraftComponent(nullptr)
		, RecipeItem{nullptr}
		, Instigator{nullptr}
		, Amount(1)
		, Priority{0}
	{}
	
	FArcStartCraftCommand(UArcCraftComponent* InCraftComponent
		, UArcItemDefinition* InRecipeItem
		, UObject* InInstigator
		, int32 InAmount = 1
		, int32 InPriority = 0)
		: CraftComponent(InCraftComponent)
		, RecipeItem(InRecipeItem)
		, Instigator(InInstigator)
		, Amount(InAmount)
		, Priority(InPriority)
	{

	}
	
	virtual UScriptStruct* GetScriptStruct() const override
	{
		return FArcStartCraftCommand::StaticStruct();
	}
	virtual ~FArcStartCraftCommand() override = default;
};
