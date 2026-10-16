// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSEditorUtils.h"

#include "ArcTQSQueryDefinition.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/PropertyBag.h"

void ArcTQSEditor::CollectEntityParameters(
	const UArcTQSQueryDefinition* Definition,
	TArray<const FPropertyBagPropertyDesc*>& OutParameters)
{
	const UPropertyBag* Bag = Definition ? Definition->Parameters.GetPropertyBagStruct() : nullptr;
	if (!Bag)
	{
		return;
	}
	for (const FPropertyBagPropertyDesc& Desc : Bag->GetPropertyDescs())
	{
		if (Desc.ValueType == EPropertyBagPropertyType::Struct
			&& Desc.ValueTypeObject == FMassEntityHandle::StaticStruct()
			&& Desc.ContainerTypes.IsEmpty())
		{
			OutParameters.Add(&Desc);
		}
	}
}

void ArcTQSEditor::NotifyPreviewSetupChanged(UArcTQSQueryDefinition& Definition)
{
	FProperty* PreviewSetupProperty = FindFProperty<FProperty>(
		UArcTQSQueryDefinition::StaticClass(),
		GET_MEMBER_NAME_CHECKED(UArcTQSQueryDefinition, PreviewSetup));
	FPropertyChangedEvent Event(PreviewSetupProperty, EPropertyChangeType::ValueSet);
	Definition.PostEditChangeProperty(Event);
}
