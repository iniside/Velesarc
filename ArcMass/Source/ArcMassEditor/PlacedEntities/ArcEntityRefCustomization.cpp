// Copyright Lukasz Baran. All Rights Reserved.

#include "PlacedEntities/ArcEntityRefCustomization.h"
#include "LevelEditorSubsystem.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "ArcMass/PlacedEntities/ArcEntityRef.h"
#include "ArcMass/PlacedEntities/ArcLinkableGuid.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityPartitionActor.h"
#include "ArcMass/PlacedEntities/ArcPlacedEntityTypes.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "MassEntityConfigAsset.h"
#include "MassEntityTraitBase.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Styling/AppStyle.h"
#include "EditorModeManager.h"
#include "EditorModeRegistry.h"
#include "Editor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Elements/SMInstance/SMInstanceElementId.h"
#include "Elements/SMInstance/SMInstanceElementData.h"
#include "StructUtils/InstancedStruct.h"

#define LOCTEXT_NAMESPACE "ArcEntityRefCustomization"

// -----------------------------------------------------------------------------
// FArcEntityPickerEdMode
// -----------------------------------------------------------------------------

const FEditorModeID FArcEntityPickerEdMode::ModeId = TEXT("ArcEntityPickerEdMode");

bool FArcEntityPickerEdMode::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if (Key == EKeys::Escape && Event == IE_Pressed)
	{
		OnPicked.ExecuteIfBound(FGuid(), nullptr);
		GetModeManager()->DeactivateMode(ModeId);
		return true;
	}

	if (Key == EKeys::RightMouseButton && Event == IE_Pressed)
	{
		OnPicked.ExecuteIfBound(FGuid(), nullptr);
		GetModeManager()->DeactivateMode(ModeId);
		return true;
	}

	if (Key == EKeys::LeftMouseButton && Event == IE_Pressed)
	{
		int32 HitX = Viewport->GetMouseX();
		int32 HitY = Viewport->GetMouseY();
		HHitProxy* HitProxy = Viewport->GetHitProxy(HitX, HitY);

		if (HitProxy != nullptr && HitProxy->IsA(HInstancedStaticMeshInstance::StaticGetType()))
		{
			HInstancedStaticMeshInstance* InstanceHit = static_cast<HInstancedStaticMeshInstance*>(HitProxy);
			UInstancedStaticMeshComponent* HitComponent = InstanceHit->Component.Get();
			if (HitComponent != nullptr)
			{
				AArcPlacedEntityPartitionActor* PartitionActor = Cast<AArcPlacedEntityPartitionActor>(HitComponent->GetOwner());
				if (PartitionActor != nullptr)
				{
					FSMInstanceId InstanceId;
					InstanceId.ISMComponent = HitComponent;
					InstanceId.InstanceIndex = InstanceHit->InstanceIndex;

					int32 EntryIndex = INDEX_NONE;
					int32 TransformIndex = INDEX_NONE;
					if (PartitionActor->ResolveInstanceId(InstanceId, EntryIndex, TransformIndex))
					{
						const TArray<FArcPlacedEntityEntry>& Entries = PartitionActor->GetEntries();
						const FArcPlacedEntityEntry& Entry = Entries[EntryIndex];

						if (Entry.EntityConfig != nullptr)
						{
							const UMassEntityTraitBase* LinkableTrait = Entry.EntityConfig->GetConfig().FindTrait(UArcLinkableGuidTrait::StaticClass());
							if (LinkableTrait != nullptr)
							{
								const FArcPlacedEntityFragmentOverrides* Overrides = Entry.PerInstanceOverrides.Find(TransformIndex);
								if (Overrides != nullptr)
								{
									for (const FInstancedStruct& FragmentValue : Overrides->FragmentValues)
									{
										if (FragmentValue.GetScriptStruct() == FArcLinkableGuidFragment::StaticStruct())
										{
											const FArcLinkableGuidFragment& GuidFrag = FragmentValue.Get<const FArcLinkableGuidFragment>();
											if (GuidFrag.LinkGuid.IsValid())
											{
												OnPicked.ExecuteIfBound(GuidFrag.LinkGuid, PartitionActor);
												GetModeManager()->DeactivateMode(ModeId);
												return true;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		// TODO: HSkinnedMeshInstance (used by UInstancedSkinnedMeshComponent hit proxies) is declared
		// only in Engine/Source/Runtime/Engine/Private/Components/InstancedSkinnedMeshComponent.cpp
		// with no public header, so it cannot be used from outside that translation unit.
		// bHasPerInstanceHitProxies is set on the skinned mesh component (so per-instance proxies are
		// emitted), but eyedropper picking of skinned mesh instances is not yet supported here.
		// If UE exports HSkinnedMeshInstance to a public header in a future engine version, add a
		// parallel check here mirroring the HInstancedStaticMeshInstance block above.

		// Click consumed but no valid linkable entity — stay in pick mode
		return true;
	}

	return false;
}

// -----------------------------------------------------------------------------
// FArcEntityRefCustomization
// -----------------------------------------------------------------------------

TSharedRef<IPropertyTypeCustomization> FArcEntityRefCustomization::MakeInstance()
{
	return MakeShareable(new FArcEntityRefCustomization);
}

FArcEntityRefCustomization::~FArcEntityRefCustomization()
{
	FEditorModeTools& ModeTools = GLevelEditorModeTools();
	if (ModeTools.IsModeActive(FArcEntityPickerEdMode::ModeId))
	{
		ModeTools.DeactivateMode(FArcEntityPickerEdMode::ModeId);
	}
}

void FArcEntityRefCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	StructPropertyHandle = PropertyHandle;
	TargetGuidHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcEntityRef, TargetGuid));
	SourcePartitionActorHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcEntityRef, SourcePartitionActor));

	HeaderRow.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(this, &FArcEntityRefCustomization::GetGuidDisplayText)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &FArcEntityRefCustomization::OnPickClicked)
			.ToolTipText(LOCTEXT("PickTooltip", "Pick a linkable entity from the viewport"))
			.ContentPadding(4.0f)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.EyeDropper"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &FArcEntityRefCustomization::OnSelectClicked)
			.IsEnabled(this, &FArcEntityRefCustomization::IsSelectEnabled)
			.ToolTipText(LOCTEXT("SelectTooltip", "Select the referenced entity in the viewport"))
			.ContentPadding(4.0f)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.SelectInViewport"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.OnClicked(this, &FArcEntityRefCustomization::OnClearClicked)
			.ToolTipText(LOCTEXT("ClearTooltip", "Clear the entity reference"))
			.ContentPadding(4.0f)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.X"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	];
}

void FArcEntityRefCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// DisplayLabel is displayed in the header row — hide it from children
}

FText FArcEntityRefCustomization::GetGuidDisplayText() const
{
	if (StructPropertyHandle.IsValid())
	{
		void* StructPtr = nullptr;
		if (StructPropertyHandle->GetValueData(StructPtr) == FPropertyAccess::Success && StructPtr != nullptr)
		{
			const FArcEntityRef& EntityRef = *static_cast<FArcEntityRef*>(StructPtr);
			if (!EntityRef.DisplayLabel.IsEmpty())
			{
				return FText::FromString(EntityRef.DisplayLabel);
			}
		}
	}

	if (!TargetGuidHandle.IsValid())
	{
		return LOCTEXT("None", "None");
	}

	void* GuidPtr = nullptr;
	if (TargetGuidHandle->GetValueData(GuidPtr) == FPropertyAccess::Success && GuidPtr != nullptr)
	{
		const FGuid& GuidValue = *static_cast<FGuid*>(GuidPtr);
		if (GuidValue.IsValid())
		{
			return FText::FromString(GuidValue.ToString(EGuidFormats::DigitsWithHyphens).Left(8));
		}
	}

	return LOCTEXT("None", "None");
}

FReply FArcEntityRefCustomization::OnClearClicked()
{
	if (TargetGuidHandle.IsValid())
	{
		void* GuidPtr = nullptr;
		if (TargetGuidHandle->GetValueData(GuidPtr) == FPropertyAccess::Success && GuidPtr != nullptr)
		{
			TargetGuidHandle->NotifyPreChange();
			*static_cast<FGuid*>(GuidPtr) = FGuid();
			TargetGuidHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}
	}

	if (StructPropertyHandle.IsValid())
	{
		void* StructPtr = nullptr;
		if (StructPropertyHandle->GetValueData(StructPtr) == FPropertyAccess::Success && StructPtr != nullptr)
		{
			StructPropertyHandle->NotifyPreChange();
			static_cast<FArcEntityRef*>(StructPtr)->DisplayLabel.Empty();
			StructPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}
	}

	return FReply::Handled();
}

FReply FArcEntityRefCustomization::OnPickClicked()
{
	FEditorModeTools& ModeTools = GLevelEditorModeTools();

	// If already picking, cancel
	if (ModeTools.IsModeActive(FArcEntityPickerEdMode::ModeId))
	{
		ModeTools.DeactivateMode(FArcEntityPickerEdMode::ModeId);
		return FReply::Handled();
	}

	ModeTools.ActivateMode(FArcEntityPickerEdMode::ModeId);
	FArcEntityPickerEdMode* PickerMode = ModeTools.GetActiveModeTyped<FArcEntityPickerEdMode>(FArcEntityPickerEdMode::ModeId);
	if (PickerMode != nullptr)
	{
		PickerMode->OnPicked = FOnEntityPicked::CreateRaw(this, &FArcEntityRefCustomization::OnEntityPicked);
	}

	return FReply::Handled();
}

void FArcEntityRefCustomization::OnEntityPicked(const FGuid& PickedGuid, AArcPlacedEntityPartitionActor* SourceActor)
{
	if (!PickedGuid.IsValid() || !TargetGuidHandle.IsValid())
	{
		return;
	}

	void* GuidPtr = nullptr;
	if (TargetGuidHandle->GetValueData(GuidPtr) == FPropertyAccess::Success && GuidPtr != nullptr)
	{
		TargetGuidHandle->NotifyPreChange();
		*static_cast<FGuid*>(GuidPtr) = PickedGuid;
		TargetGuidHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}

	if (SourcePartitionActorHandle.IsValid() && SourceActor != nullptr)
	{
		void* SoftPtrData = nullptr;
		if (SourcePartitionActorHandle->GetValueData(SoftPtrData) == FPropertyAccess::Success && SoftPtrData != nullptr)
		{
			SourcePartitionActorHandle->NotifyPreChange();
			*static_cast<TSoftObjectPtr<AArcPlacedEntityPartitionActor>*>(SoftPtrData) = SourceActor;
			SourcePartitionActorHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}
	}

	if (StructPropertyHandle.IsValid() && SourceActor != nullptr)
	{
		int32 EntryIndex = INDEX_NONE;
		int32 TransformIndex = INDEX_NONE;
		if (SourceActor->FindInstanceByGuid(PickedGuid, EntryIndex, TransformIndex))
		{
			const TArray<FArcPlacedEntityEntry>& Entries = SourceActor->GetEntries();
			const FArcPlacedEntityEntry& Entry = Entries[EntryIndex];
			FString ConfigName = Entry.EntityConfig != nullptr
				? Entry.EntityConfig->GetName()
				: TEXT("Unknown");

			void* StructPtr = nullptr;
			if (StructPropertyHandle->GetValueData(StructPtr) == FPropertyAccess::Success && StructPtr != nullptr)
			{
				StructPropertyHandle->NotifyPreChange();
				static_cast<FArcEntityRef*>(StructPtr)->DisplayLabel = FString::Printf(TEXT("%s [%d]"), *ConfigName, TransformIndex);
				StructPropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
			}
		}
	}
}

bool FArcEntityRefCustomization::IsSelectEnabled() const
{
	if (!TargetGuidHandle.IsValid())
	{
		return false;
	}

	void* GuidPtr = nullptr;
	if (TargetGuidHandle->GetValueData(GuidPtr) == FPropertyAccess::Success && GuidPtr != nullptr)
	{
		const FGuid& GuidValue = *static_cast<FGuid*>(GuidPtr);
		return GuidValue.IsValid();
	}

	return false;
}

FReply FArcEntityRefCustomization::OnSelectClicked()
{
	if (!TargetGuidHandle.IsValid())
	{
		return FReply::Handled();
	}

	void* GuidPtr = nullptr;
	if (TargetGuidHandle->GetValueData(GuidPtr) != FPropertyAccess::Success || GuidPtr == nullptr)
	{
		return FReply::Handled();
	}

	const FGuid& TargetGuid = *static_cast<FGuid*>(GuidPtr);
	if (!TargetGuid.IsValid())
	{
		return FReply::Handled();
	}

	// Resolve partition actor from the soft pointer
	AArcPlacedEntityPartitionActor* PartitionActor = nullptr;
	if (SourcePartitionActorHandle.IsValid())
	{
		void* SoftPtrData = nullptr;
		if (SourcePartitionActorHandle->GetValueData(SoftPtrData) == FPropertyAccess::Success && SoftPtrData != nullptr)
		{
			TSoftObjectPtr<AArcPlacedEntityPartitionActor>& SoftPtr = *static_cast<TSoftObjectPtr<AArcPlacedEntityPartitionActor>*>(SoftPtrData);
			PartitionActor = SoftPtr.LoadSynchronous();
		}
	}

	if (PartitionActor == nullptr)
	{
		return FReply::Handled();
	}

	int32 EntryIndex = INDEX_NONE;
	int32 TransformIndex = INDEX_NONE;
	if (!PartitionActor->FindInstanceByGuid(TargetGuid, EntryIndex, TransformIndex))
	{
		return FReply::Handled();
	}

	UInstancedStaticMeshComponent* ISMC = PartitionActor->GetEditorPreviewISMC(EntryIndex);
	if (ISMC == nullptr)
	{
		return FReply::Handled();
	}

	FSMInstanceId InstanceId;
	InstanceId.ISMComponent = ISMC;
	InstanceId.InstanceIndex = TransformIndex;

	TTypedElementOwner<FSMInstanceElementData> Element = UEngineElementsLibrary::CreateSMInstanceElement(InstanceId);
	if (!Element.IsSet())
	{
		return FReply::Handled();
	}

	ULevelEditorSubsystem* LevelEditorSubsystem = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
	if (LevelEditorSubsystem == nullptr)
	{
		return FReply::Handled();
	}

	UTypedElementSelectionSet* SelectionSet = LevelEditorSubsystem->GetSelectionSet();
	if (SelectionSet == nullptr)
	{
		return FReply::Handled();
	}

	FTypedElementSelectionOptions SelectionOptions;
	FTypedElementHandle ElementHandle = Element.AcquireHandle();
	SelectionSet->SetSelection(TArray<FTypedElementHandle>{ MoveTemp(ElementHandle) }, SelectionOptions);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
