#include "Items/Fragments/ArcItemMassHelpers.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Player/ArcHeroComponentBase.h"
#include "Engine/World.h"
#include "MassEntitySubsystem.h"
#include "MassAgentComponent.h"

namespace ArcItemMassHelpers
{

bool GetMassEntity(const FArcItemData* InItem,
                   FMassEntityManager*& OutManager,
                   FMassEntityHandle& OutEntity)
{
	OutManager = nullptr;
	OutEntity = FMassEntityHandle();

	if (!InItem)
	{
		return false;
	}

	UArcItemsStoreComponent* Store = InItem->GetItemsStoreComponent();
	if (!Store || !Store->GetOwner())
	{
		return false;
	}

	UArcCoreMassAgentComponent* MassAgent = Store->GetOwner()->FindComponentByClass<UArcCoreMassAgentComponent>();
	if (!MassAgent)
	{
		return false;
	}

	UWorld* World = Store->GetOwner()->GetWorld();
	if (!World)
	{
		return false;
	}

	UMassEntitySubsystem* MassEntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (!MassEntitySubsystem)
	{
		return false;
	}

	FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();
	FMassEntityHandle Entity = MassAgent->GetEntityHandle();

	if (!EntityManager.IsEntityValid(Entity))
	{
		return false;
	}

	OutManager = &EntityManager;
	OutEntity = Entity;
	return true;
}

} // namespace ArcItemMassHelpers
