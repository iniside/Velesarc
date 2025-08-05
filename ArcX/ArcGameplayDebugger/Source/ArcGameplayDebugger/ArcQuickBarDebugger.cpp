#include "ArcQuickBarDebugger.h"

#include "ArcItemInstanceDataDebugger.h"
#include "EnhancedInputSubsystems.h"
#include "imgui.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Commands/ArcAddItemToQuickBarCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerState.h"
#include "Input/ArcInputActionConfig.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Kismet/GameplayStatics.h"
#include "Pawn/ArcPawnData.h"
#include "Pawn/ArcPawnExtensionComponent.h"
#include "QuickBar/ArcQuickBarComponent.h"
#include "QuickBar/ArcQuickBarInputBindHandling.h"

void FArcQuickBarDebugger::Initialize()
{
}

void FArcQuickBarDebugger::Uninitialize()
{
}

void FArcQuickBarDebugger::Draw()
{
	if (!GEngine)
	{
		return;
	}
	
	if (!GEngine->GameViewport)
	{
		return;
	}

	UWorld* World = GEngine->GameViewport->GetWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}

	if (!PC->PlayerState)
	{
		return;
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return;
	}
	ULocalPlayer* LP = PC->GetLocalPlayer();
	if (!LP)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* EnhancedInputSubsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	UEnhancedPlayerInput* EnhancedPlayerInput = EnhancedInputSubsystem ? EnhancedInputSubsystem->GetPlayerInput() : nullptr;
	if (!EnhancedPlayerInput)
	{
		return;
	}
	
	APlayerState* PS = PC->PlayerState;

	UArcCoreAbilitySystemComponent* AbilitySystem = PS->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	TArray<UArcItemsStoreComponent*> ItemsStores;
	PS->GetComponents<UArcItemsStoreComponent>(ItemsStores);

	UArcQuickBarComponent* QuickBarComponent = PS->FindComponentByClass<UArcQuickBarComponent>();
	if (!QuickBarComponent)
	{
		return;
	}

	UArcPawnExtensionComponent* PawnExtensionComponent = UArcPawnExtensionComponent::FindPawnExtensionComponent(PC->GetPawn());
	if (!PawnExtensionComponent)
	{
		return;
	}

	const UArcPawnData* PawnData = PawnExtensionComponent->GetPawnData<UArcPawnData>();
	const TArray<UArcInputActionConfig*> InputConfigs = PawnData->GetInputConfig();
	
	ImGui::Begin("Quick Bar Debugger");

	const TArray<FArcQuickBar>& QuickBars = QuickBarComponent->GetQuickBars();
	for (int32 BarIdx = 0; BarIdx < QuickBars.Num(); BarIdx++)
	{
		if (ImGui::TreeNode(TCHAR_TO_ANSI(*QuickBars[BarIdx].BarId.ToString())))
		{
			auto SlotValidator = [](const FArcItemData* InItemData)
			{
				return true;
			};
			
			FGameplayTag ActiveSlot = QuickBarComponent->GetFirstActiveSlot(QuickBars[BarIdx].BarId);
			if (ImGui::Button("Cycle Backwards"))
			{
				QuickBarComponent->CycleSlotForward(QuickBars[BarIdx].BarId, ActiveSlot, SlotValidator);;
			} ImGui::SameLine();
			if (ImGui::Button("Cycle Forward"))
			{
				QuickBarComponent->CycleSlotBackward(QuickBars[BarIdx].BarId, ActiveSlot, SlotValidator);
			}
			
			UArcItemsStoreComponent* QuickBarStore = QuickBarComponent->GetItemStoreComponent(QuickBars[BarIdx]);
			if (!QuickBarStore)
			{
				ImGui::TreePop();
				continue;
			}
			for (int32 QuickSlotIdx = 0; QuickSlotIdx < QuickBars[BarIdx].Slots.Num(); QuickSlotIdx++)	
			{
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*QuickBars[BarIdx].Slots[QuickSlotIdx].QuickBarSlotId.ToString())))
				{
					const FArcQuickBarSlot& QuickSlot = QuickBars[BarIdx].Slots[QuickSlotIdx];

					FArcItemId ItemId = QuickBarComponent->GetItemId(QuickBars[BarIdx].BarId, QuickBars[BarIdx].Slots[QuickSlotIdx].QuickBarSlotId);
					const FArcItemData* ItemData = QuickBarStore->GetItemPtr(ItemId);

					const bool bIsActive = QuickBarComponent->IsQuickSlotActive(QuickBars[BarIdx].BarId, QuickBars[BarIdx].Slots[QuickSlotIdx].QuickBarSlotId);
					ImGui::Text("Active: %s", bIsActive ? "Yes" : "No");
					if (ItemData)
					{
						const FArcQuickBarInputBindHandling* InputHandling = QuickSlot.InputBind.GetPtr<FArcQuickBarInputBindHandling>();
						const UInputAction* InputAction = nullptr;
						for (const UArcInputActionConfig* InputConfig : InputConfigs)
						{
							InputAction = InputConfig->FindAbilityInputActionForTag(InputHandling->TagInput, false);
							if (InputAction)
							{
								break;
							}
						}
						
						FInputActionValue ActionValue = EnhancedPlayerInput->GetActionValue(InputAction);

						FString InputString = FString::Printf(TEXT("Input: %s (%s)"), *InputHandling->TagInput.ToString(), *ActionValue.ToString());
						FString ItemName = FString::Printf(TEXT("%s"), *GetNameSafe(ItemData->GetItemDefinition()));

						ImGui::Text(TCHAR_TO_ANSI(*InputString));
						ImGui::Text(TCHAR_TO_ANSI(*ItemName)); ImGui::SameLine();
						
						if (ImGui::Button("Remove"))
						{
							Arcx::SendServerCommand<FArcRemoveItemFromQuickSlotCommand>(PC, QuickBarComponent,
								QuickBars[BarIdx].BarId, QuickBars[BarIdx].Slots[QuickSlotIdx].QuickBarSlotId, false);
						}

						const FArcItemInstance_GrantedAbilities* GrantedAbilities = ArcItems::FindInstance<FArcItemInstance_GrantedAbilities>(ItemData);
						if (GrantedAbilities)
						{
							if (ImGui::TreeNode("Granted Abilities"))
							{
								for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : GrantedAbilities->GetGrantedAbilities())
								{
									const FGameplayAbilitySpec* AbilitySpec = AbilitySystem->FindAbilitySpecFromHandle(AbilitySpecHandle);
									if (!AbilitySpec)
									{
										continue;
									}
									
									FArcItemInstanceDebugger::DrawGameplayAbilitySpec(AbilitySystem, AbilitySpec);
								}
								ImGui::TreePop();
							}
						}
					}
					else
					{
						ImGui::Text("No Item Assigned");
					}

					
					TArray<const FArcItemData*> Items = QuickBarStore->GetItems();
					TArray<const FArcItemData*> FilteredItems;
					if (QuickBars[BarIdx].ItemRequiredTags.Num() > 0)
					{
						for (const FArcItemData* Item : Items)
						{
							if (const FArcItemFragment_Tags* Tags = ArcItems::FindFragment<FArcItemFragment_Tags>(Item))
							{
								if (Tags->ItemTags.HasAll(QuickBars[BarIdx].ItemRequiredTags))
								{
									FilteredItems.Add(Item);
								}
							}
						}
					}
					else
					{
						FilteredItems.Append(Items);
					}
					
					Items = FilteredItems;
					FString PreviewValue = "Select Item";
					if (ImGui::BeginCombo("Selected Item", TCHAR_TO_ANSI(*PreviewValue)))
					{
						for (int32 ItemIdx = 0; ItemIdx < FilteredItems.Num(); ++ItemIdx)
						{
							const FArcItemData* Item = Items[ItemIdx];
							if (!Item || !Item->GetItemDefinition())
							{
								continue;
							}
							
						
							if (ItemId == Item->GetItemId())
							{
								PreviewValue = FString::Printf(TEXT("%s"), *GetNameSafe(Item->GetItemDefinition()));
							}
							
							FString ItemName = GetNameSafe(Item->GetItemDefinition());
							FString ItemDisplayName = FString::Printf(TEXT("Add (%s)"), *ItemName);
							ImGui::PushID(TCHAR_TO_ANSI(*Item->GetItemId().ToString()));
							if (ImGui::Selectable(TCHAR_TO_ANSI(*ItemDisplayName)))
							{
								Arcx::SendServerCommand<FArcAddItemToQuickBarCommand>(PC, QuickBarComponent, QuickBarStore
									, Item->GetItemId(), QuickBars[BarIdx].BarId, QuickBars[BarIdx].Slots[QuickSlotIdx].QuickBarSlotId);
							}
							
							ImGui::PopID();
								
						}
						ImGui::EndCombo();
					}
					
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
	}
	
	ImGui::End();
}
