// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcPlacedEntityProxyDetails.h"

#include "PlacedEntities/ArcPlacedEntityProxyEditingObject.h"
#include "ArcMass/PlacedEntities/ArcLinkableGuid.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityTypes.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTemplate.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Text/STextBlock.h"
#include "StructUtils/InstancedStruct.h"

#define LOCTEXT_NAMESPACE "ArcPlacedEntityProxyDetails"

namespace ArcPlacedEntityProxyDetailsPrivate
{
	static void OnFragmentPropertyChanged(TWeakObjectPtr<UArcPlacedEntityProxyEditingObject> WeakProxy)
	{
		if (WeakProxy.IsValid())
		{
			WeakProxy->WriteOverridesToPartitionActor();
		}
	}

	static const FInstancedStruct* GetTemplateValueForFragment(
		UArcPlacedEntityProxyEditingObject* Proxy,
		int32 FragmentIndex)
	{
		if (Proxy == nullptr || !Proxy->IsValid())
		{
			return nullptr;
		}

		const TArray<FInstancedStruct>& Fragments = Proxy->EditableFragments;
		if (!Fragments.IsValidIndex(FragmentIndex))
		{
			return nullptr;
		}

		const UScriptStruct* FragmentType = Fragments[FragmentIndex].GetScriptStruct();
		if (FragmentType == nullptr)
		{
			return nullptr;
		}

		AArcPlacedEntityPartitionActor* PartitionActor = Proxy->GetPartitionActor();
		if (PartitionActor == nullptr)
		{
			return nullptr;
		}

		UWorld* World = PartitionActor->GetWorld();
		if (World == nullptr)
		{
			return nullptr;
		}

		const FArcPlacedEntityEntry& Entry = PartitionActor->Entries[Proxy->GetEntryIndex()];
		if (Entry.EntityConfig == nullptr)
		{
			return nullptr;
		}

		const FMassEntityTemplate& Template = Entry.EntityConfig->GetOrCreateEntityTemplate(*World);
		TConstArrayView<FInstancedStruct> InitialValues = Template.GetInitialFragmentValues();

		for (const FInstancedStruct& InitialValue : InitialValues)
		{
			if (InitialValue.GetScriptStruct() == FragmentType)
			{
				return &InitialValue;
			}
		}
		return nullptr;
	}
}

TSharedRef<IDetailCustomization> FArcPlacedEntityProxyDetails::MakeInstance()
{
	return MakeShareable(new FArcPlacedEntityProxyDetails);
}

void FArcPlacedEntityProxyDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> EditingObjects;
	DetailBuilder.GetObjectsBeingCustomized(EditingObjects);

	if (EditingObjects.Num() != 1)
	{
		return;
	}

	ProxyObject = Cast<UArcPlacedEntityProxyEditingObject>(EditingObjects[0].Get());
	if (!ProxyObject.IsValid())
	{
		return;
	}

	if (!ProxyObject->IsValid())
	{
		return;
	}

	TArray<FInstancedStruct>& Fragments = ProxyObject->EditableFragments;
	const TArray<FString>& DisplayNames = ProxyObject->FragmentDisplayNames;

	for (int32 Index = 0; Index < Fragments.Num(); ++Index)
	{
		FInstancedStruct& Fragment = Fragments[Index];
		if (!Fragment.IsValid())
		{
			continue;
		}

		const UScriptStruct* ScriptStruct = Fragment.GetScriptStruct();
		if (!ScriptStruct)
		{
			continue;
		}

		const FString& DisplayName = DisplayNames.IsValidIndex(Index) ? DisplayNames[Index] : ScriptStruct->GetName();

		// Build a safe category FName from the display name
		const FName CategoryName(*FString::Printf(TEXT("Fragment_%d_%s"), Index, *DisplayName));
		const FText CategoryDisplayText = FText::FromString(DisplayName);

		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
			CategoryName,
			CategoryDisplayText,
			ECategoryPriority::Default);

		// Show LinkGuid as read-only
		if (ScriptStruct == FArcLinkableGuidFragment::StaticStruct())
		{
			const FArcLinkableGuidFragment& GuidFrag = Fragment.Get<const FArcLinkableGuidFragment>();
			Category.AddCustomRow(FText::FromString(DisplayName))
			.NameContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Link GUID")))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			[
				SNew(STextBlock)
				.Text(FText::FromString(GuidFrag.LinkGuid.IsValid()
					? GuidFrag.LinkGuid.ToString(EGuidFormats::DigitsWithHyphens)
					: TEXT("None")))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
			continue;
		}

		// Wrap the fragment's existing memory — FStructOnScope does not own this memory
		// (OwnsMemory = false when InData is provided), so no double-free.
		TSharedRef<FStructOnScope> StructData = MakeShareable(
			new FStructOnScope(ScriptStruct, Fragment.GetMutableMemory()));

		TArray<IDetailPropertyRow*> PropertyRows;
		Category.AddAllExternalStructureProperties(StructData, EPropertyLocation::Default, &PropertyRows);

		TWeakObjectPtr<UArcPlacedEntityProxyEditingObject> WeakProxy = ProxyObject;
		for (IDetailPropertyRow* PropertyRow : PropertyRows)
		{
			if (PropertyRow == nullptr)
			{
				continue;
			}

			TSharedPtr<IPropertyHandle> PropHandle = PropertyRow->GetPropertyHandle();

			PropHandle->SetOnPropertyValueChanged(
				FSimpleDelegate::CreateLambda([WeakProxy]()
				{
					ArcPlacedEntityProxyDetailsPrivate::OnFragmentPropertyChanged(WeakProxy);
				}));
			PropHandle->SetOnChildPropertyValueChanged(
				FSimpleDelegate::CreateLambda([WeakProxy]()
				{
					ArcPlacedEntityProxyDetailsPrivate::OnFragmentPropertyChanged(WeakProxy);
				}));

			const int32 FragmentIdx = Index;
			FIsResetToDefaultVisible IsVisible = FIsResetToDefaultVisible::CreateLambda(
				[WeakProxy, FragmentIdx](TSharedPtr<IPropertyHandle> Handle) -> bool
				{
					if (!WeakProxy.IsValid() || !Handle.IsValid())
					{
						return false;
					}

					const FInstancedStruct* TemplateValue =
						ArcPlacedEntityProxyDetailsPrivate::GetTemplateValueForFragment(WeakProxy.Get(), FragmentIdx);
					if (TemplateValue == nullptr)
					{
						return false;
					}

					const FProperty* Prop = Handle->GetProperty();
					if (Prop == nullptr)
					{
						return false;
					}

					const FInstancedStruct& CurrentFragment = WeakProxy->EditableFragments[FragmentIdx];
					const int32 Offset = Prop->GetOffset_ForInternal();
					return !Prop->Identical(
						CurrentFragment.GetMemory() + Offset,
						TemplateValue->GetMemory() + Offset,
						PPF_None);
				});

			FResetToDefaultHandler ResetHandler = FResetToDefaultHandler::CreateLambda(
				[WeakProxy, FragmentIdx](TSharedPtr<IPropertyHandle> Handle)
				{
					if (!WeakProxy.IsValid() || !Handle.IsValid())
					{
						return;
					}

					const FInstancedStruct* TemplateValue =
						ArcPlacedEntityProxyDetailsPrivate::GetTemplateValueForFragment(WeakProxy.Get(), FragmentIdx);
					if (TemplateValue == nullptr)
					{
						return;
					}

					const FProperty* Prop = Handle->GetProperty();
					if (Prop == nullptr)
					{
						return;
					}

					FInstancedStruct& CurrentFragment = WeakProxy->EditableFragments[FragmentIdx];
					const int32 Offset = Prop->GetOffset_ForInternal();
					Prop->CopySingleValue(
						CurrentFragment.GetMutableMemory() + Offset,
						TemplateValue->GetMemory() + Offset);

					ArcPlacedEntityProxyDetailsPrivate::OnFragmentPropertyChanged(WeakProxy);
				});

			PropertyRow->OverrideResetToDefault(
				FResetToDefaultOverride::Create(IsVisible, ResetHandler));
		}
	}
}

#undef LOCTEXT_NAMESPACE
