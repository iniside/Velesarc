#include "ArcModifiableScalarCustomization.h"

#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"

TSharedRef<IPropertyTypeCustomization> FArcModifiableScalarCustomization::MakeInstance()
{
	return MakeShared<FArcModifiableScalarCustomization>();
}

void FArcModifiableScalarCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	(void)StructCustomizationUtils;
	const TSharedPtr<IPropertyHandle> BaseValueHandle = StructPropertyHandle->GetChildHandle(TEXT("BaseValue"));
	check(BaseValueHandle.IsValid());
	if (const FProperty* MetaDataProperty = StructPropertyHandle->GetMetaDataProperty())
	{
		if (const TMap<FName, FString>* MetaDataMap = MetaDataProperty->GetMetaDataMap())
		{
			for (const TPair<FName, FString>& MetaData : *MetaDataMap)
			{
				BaseValueHandle->SetInstanceMetaData(MetaData.Key, MetaData.Value);
			}
		}
	}

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f)
	[
		BaseValueHandle->CreatePropertyValueWidget()
	];
}

void FArcModifiableScalarCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	(void)StructPropertyHandle;
	(void)ChildBuilder;
	(void)StructCustomizationUtils;
}
