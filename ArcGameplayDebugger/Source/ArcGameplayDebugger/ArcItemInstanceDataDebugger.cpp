#include "ArcItemInstanceDataDebugger.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "imgui.h"
#include "AbilitySystem/ArcCoreGameplayAbility.h"

void FArcItemInstanceDebugger::DrawGameplayEffectSpec(const FGameplayEffectSpecHandle* Spec)
{
	ImGui::Text("%s", TCHAR_TO_ANSI(*FString::Printf(TEXT("Level: %f"), Spec->Data->GetLevel())));
	ImGui::Text("%s", TCHAR_TO_ANSI(*FString::Printf(TEXT("Duration: %f"), Spec->Data->GetDuration())));
	ImGui::Text("%s", TCHAR_TO_ANSI(*FString::Printf(TEXT("Period: %f"), Spec->Data->GetPeriod())));
	ImGui::Text("%s", TCHAR_TO_ANSI(*FString::Printf(TEXT("CapturedSourceTags: %s"), *Spec->Data->CapturedSourceTags.GetAggregatedTags()->ToStringSimple())));
	ImGui::Text("%s", TCHAR_TO_ANSI(*FString::Printf(TEXT("CapturedTargetTags: %s"), *Spec->Data->CapturedTargetTags.GetAggregatedTags()->ToStringSimple())));
	ImGui::Text("%s", TCHAR_TO_ANSI(*FString::Printf(TEXT("DynamicGrantedTags: %s"), *Spec->Data->DynamicGrantedTags.ToStringSimple())));
	ImGui::Text("%s", TCHAR_TO_ANSI(*FString::Printf(TEXT("DynamicAssetTags: %s"), *Spec->Data->GetDynamicAssetTags().ToStringSimple())));

	ImGui::Text("Modified Attributes: ");

	for (const FGameplayEffectModifiedAttribute& Attribute : Spec->Data->ModifiedAttributes)
	{
		ImGui::Text("%s", TCHAR_TO_ANSI(*FString::Printf(TEXT("Attribute: %s Value: %f"), *Attribute.Attribute.GetName(), Attribute.TotalMagnitude)));
	}

	ImGui::Text("Modifiers: ");

	for (const FModifierSpec& Attribute : Spec->Data->Modifiers)
	{
		ImGui::Text("%s", TCHAR_TO_ANSI(*FString::Printf(TEXT("Value: %f"), Attribute.GetEvaluatedMagnitude())));
	}
}

void FArcItemInstanceDebugger::DrawGameplayAbilitySpec(UArcCoreAbilitySystemComponent* InASC, const FGameplayAbilitySpec* Spec)
{
	if (!InASC || !Spec)
	{
		return;
	}

	UArcCoreGameplayAbility* ArcAbility = Cast<UArcCoreGameplayAbility>(Spec->GetPrimaryInstance());

	FString AbilityName = GetNameSafe(Spec->GetPrimaryInstance());
	if (ImGui::TreeNode(TCHAR_TO_ANSI(*AbilityName)))
	{
		ImGui::Text("DynamicTags: %s", TCHAR_TO_ANSI(*Spec->GetDynamicSpecSourceTags().ToString()));
		if (ArcAbility)
		{
			double StartTime = InASC->GetAbilityActivationStartTime(Spec->Handle);
			const float ActivationTime = ArcAbility->GetActivationTime();
			double CurrentTime = StartTime > -1 ? FPlatformTime::Seconds() - StartTime : 0.0;
			FString ActivationTimeStr = FString::Printf(TEXT("StartTime: %.2f, ActivationTime: %.2f CurrentTime %.2f"), StartTime, ActivationTime, CurrentTime);
			ImGui::Text("ActivationTime: %s", TCHAR_TO_ANSI(*ActivationTimeStr));
		}

		ImGui::TreePop();
	}
}
