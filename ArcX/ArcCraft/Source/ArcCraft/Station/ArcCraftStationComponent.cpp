/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "ArcCraft/Station/ArcCraftStationComponent.h"

#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftVisEntityComponent.h"
#include "ArcCraft/Station/ArcRecipeLookup.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Net/UnrealNetwork.h"
#include "ArcCraft/Recipe/ArcCraftOutputBuilder.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcCraftStation, Log, All);
DEFINE_LOG_CATEGORY(LogArcCraftStation);

// -------------------------------------------------------------------
// Constructor
// -------------------------------------------------------------------

UArcCraftStationComponent::UArcCraftStationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_DuringPhysics;
	PrimaryComponentTick.TickInterval = 0.5f;

	SetIsReplicatedByDefault(true);
}

// -------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------

void UArcCraftStationComponent::BeginPlay()
{
	Super::BeginPlay();

	PrimaryComponentTick.TickInterval = TickInterval;

	// Validate that the owning actor has UArcCraftVisEntityComponent for Mass entity access
	if (GetOwner())
	{
		UArcCraftVisEntityComponent* CraftVisComp =
			GetOwner()->FindComponentByClass<UArcCraftVisEntityComponent>();

		if (!CraftVisComp)
		{
			UE_LOG(LogArcCraftStation, Error,
				TEXT("UArcCraftStationComponent on %s requires UArcCraftVisEntityComponent "
					 "for Mass entity access. Entity-backed features will not work."),
				*GetOwner()->GetName());
		}
		else
		{
			CachedVisComponent = CraftVisComp;
		}
	}
}

void UArcCraftStationComponent::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UArcCraftStationComponent, CraftQueue);
}

// -------------------------------------------------------------------
// Public API: Queue a recipe
// -------------------------------------------------------------------

bool UArcCraftStationComponent::QueueRecipe(
	const UArcRecipeDefinition* Recipe,
	UObject* Instigator,
	int32 Amount)
{
	if (!Recipe || Amount <= 0)
	{
		return false;
	}

	// Check station tag requirements
	if (Recipe->RequiredStationTags.Num() > 0 && !StationTags.HasAll(Recipe->RequiredStationTags))
	{
		UE_LOG(LogArcCraftStation, Warning,
			TEXT("Station does not have required tags for recipe %s"), *Recipe->GetName());
		return false;
	}

	// Check queue capacity
	if (MaxQueueSize > 0 && CraftQueue.Entries.Num() >= MaxQueueSize)
	{
		UE_LOG(LogArcCraftStation, Warning,
			TEXT("Craft queue is full (%d/%d)"), CraftQueue.Entries.Num(), MaxQueueSize);
		return false;
	}

	// Check ingredient availability
	const FArcCraftItemSource* Source = ItemSource.GetPtr<FArcCraftItemSource>();
	if (!Source)
	{
		UE_LOG(LogArcCraftStation, Log, TEXT("No item source configured on craft station"));
		return false;
	}

	if (!Source->CanSatisfyRecipe(this, Recipe, Instigator))
	{
		UE_LOG(LogArcCraftStation, Warning,
			TEXT("Ingredients not available for recipe %s"), *Recipe->GetName());
		return false;
	}

	// Consume ingredients
	TArray<FArcItemSpec> MatchedItems;
	TArray<float> QualityMults;
	if (!Source->ConsumeIngredients(
		this, Recipe, Instigator,
		MatchedItems, QualityMults))
	{
		UE_LOG(LogArcCraftStation, Warning,
			TEXT("Failed to consume ingredients for recipe %s"), *Recipe->GetName());
		return false;
	}

	// Determine priority — avoid duplicates
	int32 NewPriority = 0;
	for (const FArcCraftQueueEntry& Entry : CraftQueue.Entries)
	{
		if (Entry.Priority >= NewPriority)
		{
			NewPriority = Entry.Priority + 1;
		}
	}

	// Create queue entry
	FArcCraftQueueEntry NewEntry;
	NewEntry.EntryId = FGuid::NewGuid();
	NewEntry.Recipe = Recipe;
	NewEntry.Amount = Amount;
	NewEntry.CompletedAmount = 0;
	NewEntry.Priority = NewPriority;
	NewEntry.StartTimestamp = FDateTime::UtcNow().ToUnixTimestamp();
	NewEntry.ElapsedTickTime = 0.0f;
	NewEntry.TimeMode = TimeMode;

	CurrentInstigator = Instigator;

	// Write to entity queue fragment if entity-backed
	const bool bIsEntityBacked = CachedVisComponent.IsValid();
	if (bIsEntityBacked)
	{
		FMassEntityManager* EntityMgr = GetEntityManager();
		const FMassEntityHandle Entity = GetEntityHandle();
		if (EntityMgr && Entity.IsValid() && EntityMgr->IsEntityValid(Entity))
		{
			FArcCraftQueueFragment* QueueFrag = EntityMgr->GetFragmentDataPtr<FArcCraftQueueFragment>(Entity);
			if (QueueFrag)
			{
				const int32 Idx = QueueFrag->Entries.Add(NewEntry);
				QueueFrag->ActiveEntryIndex = INDEX_NONE; // Re-evaluate
				OnQueueChanged.Broadcast(this, QueueFrag->Entries[Idx]);
				return true;
			}
		}
	}

	const int32 Idx = CraftQueue.Entries.Add(NewEntry);
	
	// Enable actor tick for AutoTick mode, but only for non-entity-backed stations.
	// Entity-backed AutoTick stations are processed by UArcCraftTickProcessor.
	if (TimeMode == EArcCraftStationTimeMode::AutoTick && !bIsEntityBacked)
	{
		PrimaryComponentTick.SetTickFunctionEnable(true);
	}

	// Reset active entry tracking so it gets re-evaluated
	ActiveEntryIndex = INDEX_NONE;

	OnQueueChanged.Broadcast(this, CraftQueue.Entries[Idx]);

	UE_LOG(LogArcCraftStation, Log,
		TEXT("Queued recipe %s (x%d) on station %s"),
		*Recipe->GetName(), Amount, *GetOwner()->GetName());

	return true;
}

// -------------------------------------------------------------------
// Public API: Auto-discover recipe
// -------------------------------------------------------------------

UArcRecipeDefinition* UArcCraftStationComponent::DiscoverAndQueueRecipe(UObject* Instigator)
{
	const FArcCraftItemSource* Source = ItemSource.GetPtr<FArcCraftItemSource>();
	if (!Source)
	{
		return nullptr;
	}

	// Get available items
	TArray<FArcItemSpec> AvailableItems = Source->GetAvailableItems(this, Instigator);
	if (AvailableItems.Num() == 0)
	{
		return nullptr;
	}

	// Build union of all available item tags (for coarse filtering)
	FGameplayTagContainer AvailableItemTags;
	for (const FArcItemSpec& Item : AvailableItems)
	{
		if (Item.GetItemDefinitionId().IsValid())
		{
			const UArcItemDefinition* Def = Item.GetItemDefinition();
			if (Def)
			{
				const FArcItemFragment_Tags* TagsFragment = Def->FindFragment<FArcItemFragment_Tags>();
				if (TagsFragment)
				{
					AvailableItemTags.AppendTags(TagsFragment->ItemTags);
				}
			}
		}
	}

	// Get cached station recipes
	EnsureRecipesCached();

	// Coarse filter by ingredient tags
	TArray<FAssetData> Candidates = FArcRecipeLookup::FilterByAvailableIngredients(
		CachedStationRecipes, AvailableItemTags);

	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	// Find best match (loads candidate assets)
	float MatchScore = 0.0f;
	UArcRecipeDefinition* BestRecipe = FArcRecipeLookup::FindBestRecipeForIngredients(
		Candidates, AvailableItems, StationTags, MatchScore);

	if (!BestRecipe)
	{
		return nullptr;
	}

	// Queue the discovered recipe
	if (QueueRecipe(BestRecipe, Instigator))
	{
		return BestRecipe;
	}

	return nullptr;
}

// -------------------------------------------------------------------
// Public API: Cancel
// -------------------------------------------------------------------

bool UArcCraftStationComponent::CancelQueueEntry(const FGuid& EntryId)
{
	for (int32 Idx = 0; Idx < CraftQueue.Entries.Num(); ++Idx)
	{
		if (CraftQueue.Entries[Idx].EntryId == EntryId)
		{
			FArcCraftQueueEntry RemovedEntry = CraftQueue.Entries[Idx];
			CraftQueue.Edit().Remove(Idx);

			if (Idx == ActiveEntryIndex)
			{
				ActiveEntryIndex = INDEX_NONE;
			}

			// Also remove from entity queue fragment
			if (CachedVisComponent.IsValid())
			{
				FMassEntityManager* EntityMgr = GetEntityManager();
				const FMassEntityHandle Entity = GetEntityHandle();
				if (EntityMgr && Entity.IsValid() && EntityMgr->IsEntityValid(Entity))
				{
					FArcCraftQueueFragment* QueueFrag = EntityMgr->GetFragmentDataPtr<FArcCraftQueueFragment>(Entity);
					if (QueueFrag)
					{
						for (int32 EntityIdx = 0; EntityIdx < QueueFrag->Entries.Num(); ++EntityIdx)
						{
							if (QueueFrag->Entries[EntityIdx].EntryId == EntryId)
							{
								QueueFrag->Entries.RemoveAt(EntityIdx);
								QueueFrag->ActiveEntryIndex = INDEX_NONE;
								break;
							}
						}
					}
				}
			}

			OnQueueChanged.Broadcast(this, RemovedEntry);

			if (CraftQueue.Entries.IsEmpty())
			{
				PrimaryComponentTick.SetTickFunctionEnable(false);
			}

			return true;
		}
	}
	return false;
}

// -------------------------------------------------------------------
// Public API: Deposit
// -------------------------------------------------------------------

bool UArcCraftStationComponent::DepositItem(const FArcItemSpec& Item, UObject* Instigator)
{
	FArcCraftItemSource* Source = ItemSource.GetMutablePtr<FArcCraftItemSource>();
	if (!Source)
	{
		return false;
	}
	return Source->DepositItem(this, Item, Instigator);
}

// -------------------------------------------------------------------
// Public API: Set queue from entity
// -------------------------------------------------------------------

void UArcCraftStationComponent::SetQueueFromEntity(const TArray<FArcCraftQueueEntry>& InEntries)
{
	CraftQueue.Entries = InEntries;
	ActiveEntryIndex = INDEX_NONE;
}

// -------------------------------------------------------------------
// Public API: Interaction-based completion
// -------------------------------------------------------------------

bool UArcCraftStationComponent::TryCompleteOnInteraction(UObject* Instigator)
{
	if (CraftQueue.Entries.IsEmpty())
	{
		return false;
	}

	const int32 Idx = FindActiveEntryIndex();
	if (Idx == INDEX_NONE)
	{
		return false;
	}

	FArcCraftQueueEntry& Entry = CraftQueue.Entries[Idx];
	if (!Entry.Recipe)
	{
		return false;
	}

	CurrentInstigator = Instigator;

	const float CraftTime = Entry.Recipe->CraftTime;
	if (CraftTime <= 0.0f)
	{
		return false;
	}

	bool bCompletedAny = false;

	if (Entry.TimeMode == EArcCraftStationTimeMode::InteractionCheck)
	{
		// Check elapsed time since start
		const int64 Now = FDateTime::UtcNow().ToUnixTimestamp();
		const int64 Elapsed = Now - Entry.StartTimestamp;

		const int32 PossibleCompletions = FMath::FloorToInt(
			static_cast<float>(Elapsed) / CraftTime);
		const int32 Remaining = Entry.Amount - Entry.CompletedAmount;
		const int32 ToComplete = FMath::Min(PossibleCompletions, Remaining);

		for (int32 i = 0; i < ToComplete; ++i)
		{
			FinishCurrentItem(Entry);
			Entry.CompletedAmount++;
			bCompletedAny = true;
		}

		if (Entry.CompletedAmount < Entry.Amount)
		{
			// Update timestamp for remaining items
			Entry.StartTimestamp = Now - static_cast<int64>(
				(Elapsed - ToComplete * CraftTime));
		}
	}
	else if (Entry.TimeMode == EArcCraftStationTimeMode::AutoTick)
	{
		// AutoTick entries can also be manually completed via interaction
		if (Entry.ElapsedTickTime >= CraftTime)
		{
			FinishCurrentItem(Entry);
			Entry.CompletedAmount++;
			Entry.ElapsedTickTime = 0.0f;
			bCompletedAny = true;
		}
	}

	// Check if the entry is fully complete
	if (Entry.CompletedAmount >= Entry.Amount)
	{
		CraftQueue.Edit().Remove(Idx);
		ActiveEntryIndex = INDEX_NONE;

		if (CraftQueue.Entries.IsEmpty())
		{
			PrimaryComponentTick.SetTickFunctionEnable(false);
		}
	}

	return bCompletedAny;
}

// -------------------------------------------------------------------
// Public API: Queries
// -------------------------------------------------------------------

TArray<UArcRecipeDefinition*> UArcCraftStationComponent::GetAvailableRecipes()
{
	EnsureRecipesCached();

	TArray<UArcRecipeDefinition*> Recipes;
	for (const FAssetData& Data : CachedStationRecipes)
	{
		UArcRecipeDefinition* Recipe = Cast<UArcRecipeDefinition>(Data.GetAsset());
		if (Recipe)
		{
			Recipes.Add(Recipe);
		}
	}
	return Recipes;
}

bool UArcCraftStationComponent::CanCraftRecipe(
	const UArcRecipeDefinition* Recipe,
	UObject* Instigator) const
{
	if (!Recipe)
	{
		return false;
	}

	if (Recipe->RequiredStationTags.Num() > 0 && !StationTags.HasAll(Recipe->RequiredStationTags))
	{
		return false;
	}

	const FArcCraftItemSource* Source = ItemSource.GetPtr<FArcCraftItemSource>();
	if (!Source)
	{
		return false;
	}

	return Source->CanSatisfyRecipe(this, Recipe, Instigator);
}

const TArray<FArcCraftQueueEntry>& UArcCraftStationComponent::GetQueue() const
{
	return CraftQueue.Entries;
}

TArray<FArcCraftQueueEntry> UArcCraftStationComponent::GetLiveQueue() const
{
	if (CachedVisComponent.IsValid())
	{
		FMassEntityManager* EntityMgr = GetEntityManager();
		const FMassEntityHandle Entity = GetEntityHandle();
		if (EntityMgr && Entity.IsValid() && EntityMgr->IsEntityValid(Entity))
		{
			const FArcCraftQueueFragment* QueueFrag = EntityMgr->GetFragmentDataPtr<FArcCraftQueueFragment>(Entity);
			if (QueueFrag)
			{
				return QueueFrag->Entries;
			}
		}
	}
	return CraftQueue.Entries;
}

TArray<UArcRecipeDefinition*> UArcCraftStationComponent::GetCraftableRecipes(UObject* Instigator)
{
	TArray<UArcRecipeDefinition*> AllRecipes = GetAvailableRecipes();
	TArray<UArcRecipeDefinition*> Craftable;

	for (UArcRecipeDefinition* Recipe : AllRecipes)
	{
		if (CanCraftRecipe(Recipe, Instigator))
		{
			Craftable.Add(Recipe);
		}
	}

	return Craftable;
}

// -------------------------------------------------------------------
// Tick
// -------------------------------------------------------------------

void UArcCraftStationComponent::TickComponent(
	float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CraftQueue.Entries.IsEmpty())
	{
		PrimaryComponentTick.SetTickFunctionEnable(false);
		return;
	}

	ProcessActiveEntry(DeltaTime);
}

// -------------------------------------------------------------------
// Internal: Recipe cache
// -------------------------------------------------------------------

void UArcCraftStationComponent::EnsureRecipesCached()
{
	if (bRecipesCached)
	{
		return;
	}

	CachedStationRecipes = FArcRecipeLookup::FindRecipesForStation(StationTags);
	bRecipesCached = true;

	UE_LOG(LogArcCraftStation, Log,
		TEXT("Cached %d recipes for station %s"),
		CachedStationRecipes.Num(),
		*GetOwner()->GetName());
}

// -------------------------------------------------------------------
// Internal: Find active entry
// -------------------------------------------------------------------

int32 UArcCraftStationComponent::FindActiveEntryIndex() const
{
	int32 BestIdx = INDEX_NONE;
	int32 BestPriority = INT_MAX;

	for (int32 Idx = 0; Idx < CraftQueue.Entries.Num(); ++Idx)
	{
		const FArcCraftQueueEntry& Entry = CraftQueue.Entries[Idx];
		if (Entry.Priority < BestPriority)
		{
			BestPriority = Entry.Priority;
			BestIdx = Idx;
		}
	}

	return BestIdx;
}

// -------------------------------------------------------------------
// Internal: Process active entry (AutoTick)
// -------------------------------------------------------------------

void UArcCraftStationComponent::ProcessActiveEntry(float DeltaTime)
{
	// Find or confirm active entry
	if (ActiveEntryIndex == INDEX_NONE || !CraftQueue.Entries.IsValidIndex(ActiveEntryIndex))
	{
		ActiveEntryIndex = FindActiveEntryIndex();
	}

	if (ActiveEntryIndex == INDEX_NONE)
	{
		return;
	}

	FArcCraftQueueEntry& Entry = CraftQueue.Entries[ActiveEntryIndex];

	// Only auto-process AutoTick entries
	if (Entry.TimeMode != EArcCraftStationTimeMode::AutoTick)
	{
		return;
	}

	if (!Entry.Recipe)
	{
		return;
	}

	const float CraftTime = Entry.Recipe->CraftTime;
	if (CraftTime <= 0.0f)
	{
		return;
	}

	Entry.ElapsedTickTime += DeltaTime;

	if (Entry.ElapsedTickTime >= CraftTime)
	{
		FinishCurrentItem(Entry);
		Entry.CompletedAmount++;
		Entry.ElapsedTickTime = 0.0f;

		// Re-stamp for next item
		Entry.StartTimestamp = FDateTime::UtcNow().ToUnixTimestamp();

		if (Entry.CompletedAmount >= Entry.Amount)
		{
			FArcCraftQueueEntry CompletedEntry = Entry;
			CraftQueue.Edit().Remove(ActiveEntryIndex);
			ActiveEntryIndex = INDEX_NONE;

			OnQueueChanged.Broadcast(this, CompletedEntry);

			if (CraftQueue.Entries.IsEmpty())
			{
				PrimaryComponentTick.SetTickFunctionEnable(false);
			}
		}
	}
}

// -------------------------------------------------------------------
// Internal: Finish one item
// -------------------------------------------------------------------

void UArcCraftStationComponent::FinishCurrentItem(FArcCraftQueueEntry& Entry)
{
	if (!Entry.Recipe)
	{
		return;
	}

	// Build the output item spec
	FArcItemSpec OutputSpec = BuildOutputSpec(Entry.Recipe, CurrentInstigator.Get());

	// Deliver via output delivery
	const FArcCraftOutputDelivery* Delivery = OutputDelivery.GetPtr<FArcCraftOutputDelivery>();
	if (Delivery)
	{
		Delivery->DeliverOutput(this, OutputSpec, CurrentInstigator.Get());
	}
	else
	{
		UE_LOG(LogArcCraftStation, Warning,
			TEXT("No output delivery configured — crafted item lost!"));
	}

	OnItemCompleted.Broadcast(this, Entry.Recipe, OutputSpec);

	UE_LOG(LogArcCraftStation, Log,
		TEXT("Crafted item from recipe %s (%d/%d)"),
		*Entry.Recipe->GetName(),
		Entry.CompletedAmount + 1,
		Entry.Amount);
}

// -------------------------------------------------------------------
// Internal: Build output spec — delegates to FArcCraftOutputBuilder
// -------------------------------------------------------------------

FArcItemSpec UArcCraftStationComponent::BuildOutputSpec(
	const UArcRecipeDefinition* Recipe,
	const UObject* Instigator) const
{
	if (!Recipe || !Recipe->OutputItemDefinition.IsValid())
	{
		return FArcItemSpec();
	}

	// NOTE: Ingredients were consumed at queue time. We cannot re-match them now.
	// Quality data should ideally be cached on the queue entry at consume time.
	// This is a known simplification — for now, use default quality (1.0) for all slots.
	const int32 NumIngredients = Recipe->Ingredients.Num();

	TArray<FArcItemSpec> MatchedIngredients;
	MatchedIngredients.SetNum(NumIngredients);

	TArray<float> QualityMultipliers;
	QualityMultipliers.SetNum(NumIngredients);

	for (int32 i = 0; i < NumIngredients; ++i)
	{
		QualityMultipliers[i] = 1.0f;
	}

	// Compute average quality
	float TotalQuality = 0.0f;
	int32 TotalWeight = 0;
	for (int32 Idx = 0; Idx < NumIngredients; ++Idx)
	{
		const FArcRecipeIngredient* Ingredient = Recipe->GetIngredientBase(Idx);
		const int32 Weight = Ingredient ? Ingredient->Amount : 1;
		TotalQuality += QualityMultipliers[Idx] * Weight;
		TotalWeight += Weight;
	}
	const float AverageQuality = TotalWeight > 0 ? TotalQuality / TotalWeight : 1.0f;

	return FArcCraftOutputBuilder::Build(Recipe, MatchedIngredients, QualityMultipliers, AverageQuality);
}

// -------------------------------------------------------------------
// Internal: Entity access helpers
// -------------------------------------------------------------------

FMassEntityHandle UArcCraftStationComponent::GetEntityHandle() const
{
	if (CachedVisComponent.IsValid())
	{
		return CachedVisComponent->GetEntityHandle();
	}
	return FMassEntityHandle();
}

FMassEntityManager* UArcCraftStationComponent::GetEntityManager() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return nullptr;
	}

	return &EntitySubsystem->GetMutableEntityManager();
}
