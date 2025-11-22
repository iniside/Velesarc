// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcStartCraftCommand.h"

#include "ArcCraft/ArcCraftComponent.h"

bool FArcStartCraftCommand::CanSendCommand() const
{
	// TODO:: Add checks for resources etc.
	return true;
}

void FArcStartCraftCommand::PreSendCommand()
{
	
}

bool FArcStartCraftCommand::Execute()
{
	if (CraftComponent)
	{
		CraftComponent->CraftItem(RecipeItem, Instigator, Amount, Priority);
		return true;
	}

	return false;
}
