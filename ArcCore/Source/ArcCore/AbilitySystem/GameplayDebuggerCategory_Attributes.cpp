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

#include "GameplayDebuggerCategory_Attributes.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"

FGameplayDebuggerCategory_Attributes::FGameplayDebuggerCategory_Attributes()
{
	SetDataPackReplication<FRepData>(&DataPack);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Attributes::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_Attributes());
}

void FGameplayDebuggerCategory_Attributes::CollectData(APlayerController* OwnerPC
													   , AActor* DebugActor)
{
	UAbilitySystemComponent* AbilityComp = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DebugActor);
	if (AbilityComp)
	{
		TArray<FGameplayAttribute> AllAttributes;
		AbilityComp->GetAllAttributes(AllAttributes);

		for (const FGameplayAttribute& Attribute : AllAttributes)
		{
			FRepData::FGameplayAttributeDebug AttributeData;

			AttributeData.AttributeName = Attribute.AttributeName;
			AttributeData.AttributeSetName = Attribute.GetAttributeSetClass()->GetName();
			AttributeData.AttributeValue = AbilityComp->GetNumericAttribute(Attribute);
			AttributeData.AttributeBaseValue = AbilityComp->GetNumericAttributeBase(Attribute);

			DataPack.Attributes.Add(AttributeData);
		}
	}
}

void FGameplayDebuggerCategory_Attributes::DrawData(APlayerController* OwnerPC
													, FGameplayDebuggerCanvasContext& CanvasContext)
{
	for (const FRepData::FGameplayAttributeDebug& AttributeData : DataPack.Attributes)
	{
		CanvasContext.Printf(
			TEXT("{white}Attribute: {yellow}%s{white}, Value: {yellow}%.2f{white}, Base: {yellow}%.2f{white}, Set: "
				"{yellow}%s")
			, *AttributeData.AttributeName
			, AttributeData.AttributeValue
			, AttributeData.AttributeBaseValue
			, *AttributeData.AttributeSetName);
	}
}

void FGameplayDebuggerCategory_Attributes::FRepData::Serialize(FArchive& Ar)
{
	int32 NumAttributes = Attributes.Num();
	Ar << NumAttributes;
	if (Ar.IsLoading())
	{
		Attributes.SetNum(NumAttributes);
	}

	for (int32 Idx = 0; Idx < NumAttributes; Idx++)
	{
		Ar << Attributes[Idx].AttributeName;
		Ar << Attributes[Idx].AttributeSetName;
		Ar << Attributes[Idx].AttributeValue;
		Ar << Attributes[Idx].AttributeBaseValue;
	}
}

#endif
