#include "ArcProxyAttributeSetDebugger.h"

#include "ArcDebuggerHacks.h"
#include "imgui.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

void FArcProxyAttributeSetDebugger::Initialize()
{
}

void FArcProxyAttributeSetDebugger::Uninitialize()
{
}

void FArcProxyAttributeSetDebugger::Draw()
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

	APlayerState* PS = PC->PlayerState;
	ImGui::Begin("Proxy Attribute Sets");
	ImGui::Text("Target Pawn");
	APawn* P = PS->GetPawn();
	if (!P)
	{
		ImGui::Text("No Target");
		ImGui::End();
		return;
	}

	
	FVector EyeLoc;
	FRotator EyeRot;
	P->GetActorEyesViewPoint(EyeLoc, EyeRot);

	const FVector TraceStart = EyeLoc;
	const FVector TraceEnd = EyeLoc + EyeRot.Vector() * 10000.f;
	FCollisionQueryParams TraceParams(FName(TEXT("Proxy Attribute Set Trace")), true, P);
	TraceParams.AddIgnoredActor(P);
	
	FHitResult HitResult;
	World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, TraceParams);
	if (!HitResult.GetActor())
	{
		ImGui::Text("No Hit");
		ImGui::End();
		return;
	}

	UArcCoreAbilitySystemComponent* AbilitySystem = HitResult.GetActor()->FindComponentByClass<UArcCoreAbilitySystemComponent>();
	if (!AbilitySystem)
	{
		ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Target %s Does not have Ability System"), *GetNameSafe(HitResult.GetActor()))));
		ImGui::End();
		return;
	}

	ImGui::Text(TCHAR_TO_ANSI(*FString::Printf(TEXT("Target %s"), *GetNameSafe(HitResult.GetActor()))));
	
	const double WorldTime = AbilitySystem->GetWorld()->GetTimeSeconds();
	auto BoolToText = [](bool InBool)
	{
		return InBool ? TEXT("True") : TEXT("False");
	};

	const TArray<UAttributeSet*>& AttributeSets = AbilitySystem->GetSpawnedAttributes();
	

	const FActiveGameplayEffectsContainer& ActiveEffects = AbilitySystem->GetActiveGameplayEffects();
	const TMap<FGameplayAttribute, FAggregatorRef>& AggregatorMap = DebugHack::GetPrivateAggregaotMap(&ActiveEffects);
	
	for (UAttributeSet* AttributeSet : AttributeSets)
	{
		if (ImGui::TreeNode(TCHAR_TO_ANSI(*GetNameSafe(AttributeSet))))
		{
			FString TableName = FString::Printf(TEXT("AttributeSet_%s"), *GetNameSafe(AttributeSet));
			ImGui::BeginTable(TCHAR_TO_ANSI(*TableName), 2);
			
			for (TFieldIterator<FProperty> It(AttributeSet->GetClass()); It; ++It)
			{
				if (FStructProperty* StructProp = CastField<FStructProperty>(*It))
				{
					if (!StructProp->Struct->IsChildOf(FGameplayAttributeData::StaticStruct()))
					{
						continue;
					}
					
					FGameplayAttributeData* Attribute = StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttributeSet);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
						ImGui::Text(TCHAR_TO_ANSI(*StructProp->GetName()));
					ImGui::TableSetColumnIndex(1);
						ImGui::Text("Base %f", Attribute->GetBaseValue()); ImGui::SameLine();
						ImGui::Text("Current %f", Attribute->GetCurrentValue());

					FString AttributeData = FString::Printf(TEXT("AttributeData_%s_%s"), *StructProp->GetName(), *GetNameSafe(AttributeSet));
					ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
					ImGui::PushID(TCHAR_TO_ANSI(*AttributeData));
					if (ImGui::TreeNode("Attributes Details"))
					{
						FGameplayAttribute GameplayAttribute(StructProp);
						if (const FAggregatorRef* AggRef = AggregatorMap.Find(GameplayAttribute))
						{
							FAggregator* Agg = AggRef->Get();
							const FAggregatorModChannelContainer& ModChannels = DebugHack::GetPrivateAggregatorModChannels(Agg);

							TMap<EGameplayModEvaluationChannel, const TArray<FAggregatorMod>*> OutMods;
							ModChannels.GetAllAggregatorMods(OutMods);

							
							
							for (const auto& ChannelModsPair : OutMods)
							{
								const EGameplayModEvaluationChannel Channel = ChannelModsPair.Key;
								ImGui::Text("ModInfo:");
								ModChannels.ForEachMod([Channel](const FAggregatorModInfo& ModInfo)
								{
									if (ModInfo.Channel == Channel)
									{
										ImGui::Text("  Op: %s", TCHAR_TO_ANSI(*UEnum::GetValueAsString(ModInfo.Op)));
									}
								});
								
								const TArray<FAggregatorMod>* Mods = ChannelModsPair.Value;

								ImGui::Text("Channel: %s", TCHAR_TO_ANSI(*UEnum::GetValueAsString(Channel)));
								for (const FAggregatorMod& Mod : *Mods)
								{
									const FActiveGameplayEffect* Activeeffect = AbilitySystem->GetActiveGameplayEffect(Mod.ActiveHandle);
									if (Activeeffect)
									{
										ImGui::Text("  Effect: %s", TCHAR_TO_ANSI(*GetNameSafe(Activeeffect->Spec.Def)));	
									}
									if (Mod.SourceTagReqs)
									{
										ImGui::Text("     Source Tag: %s", TCHAR_TO_ANSI(*Mod.SourceTagReqs->ToString()));
									}
									if (Mod.TargetTagReqs)
									{
										ImGui::Text("     Target Tag: %s", TCHAR_TO_ANSI(*Mod.TargetTagReqs->ToString()));
									}
									ImGui::Text("     IsPredicted", BoolToText(Mod.IsPredicted));
									ImGui::Text("     IsQualified", BoolToText(Mod.Qualifies()));
									ImGui::Text("     Stack: %f, Magnitude: %f", Mod.StackCount, Mod.EvaluatedMagnitude);
								}
							}
						}
												
						ImGui::TreePop();
					}
					ImGui::PopID();
				}
			}
			
			ImGui::EndTable();
			ImGui::TreePop();
		}
	}
	ImGui::End();
}
