// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomNodeBuilder.h"
#include "IDetailPropertyRow.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Widgets/Input/SComboBox.h"

class IDetailChildrenBuilder;
class IDetailGroup;
class IPropertyHandle;
class UCollisionProfile;

struct FArcCollisionChannelInfo
{
	FString DisplayName;
	ECollisionChannel CollisionChannel;
	bool bTraceType;
};

class FArcBodyInstanceExpander : public TSharedFromThis<FArcBodyInstanceExpander>
{
public:
	void BuildBodyInstanceUI(TSharedRef<IPropertyHandle> BodyInstanceHandle, IDetailChildrenBuilder& ChildrenBuilder);

private:
	void CreateCollisionSetup(IDetailGroup& CollisionGroup, TSharedRef<IPropertyHandle> BodyInstanceHandle);
	void AddCollisionProperties(IDetailGroup& CollisionGroup, TSharedRef<IPropertyHandle> BodyInstanceHandle);
	void AddPhysicsProperties(IDetailGroup& PhysicsGroup, TSharedRef<IPropertyHandle> BodyInstanceHandle);

	static IDetailPropertyRow* AddPropertyToGroup(IDetailGroup& Group, TSharedRef<IPropertyHandle> StructHandle, FName PropertyName);

	TSharedRef<SWidget> MakeCollisionProfileComboWidget(TSharedPtr<FString> InItem);
	void OnCollisionProfileChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	FText GetCollisionProfileComboBoxContent() const;
	FText GetCollisionProfileComboBoxToolTip() const;
	void OnCollisionProfileComboOpening();
	void SetToDefaultProfile();
	bool ShouldShowResetToDefaultProfile() const;

	TSharedRef<SWidget> MakeObjectTypeComboWidget(TSharedPtr<FString> InItem);
	void OnObjectTypeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	FText GetObjectTypeComboBoxContent() const;
	int32 InitializeObjectTypeComboList();

	void OnCollisionChannelChanged(ECheckBoxState InNewValue, int32 ValidIndex, ECollisionResponse InCollisionResponse);
	ECheckBoxState IsCollisionChannelChecked(int32 ValidIndex, ECollisionResponse InCollisionResponse) const;
	void OnAllCollisionChannelChanged(ECheckBoxState InNewValue, ECollisionResponse InCollisionResponse);
	ECheckBoxState IsAllCollisionChannelChecked(ECollisionResponse InCollisionResponse) const;

	void SetToDefaultResponse(int32 ValidIndex);
	bool ShouldShowResetToDefaultResponse(int32 ValidIndex) const;

	bool ShouldEnableCustomCollisionSetup() const;
	EVisibility ShouldShowCustomCollisionSetup() const;
	bool IsCollisionEnabled() const;

	void SetResponse(int32 ValidIndex, ECollisionResponse InCollisionResponse);
	void SetCollisionResponseContainer(const FCollisionResponseContainer& ResponseContainer);
	void UpdateCollisionProfile();
	void UpdateValidCollisionChannels();
	void RefreshCollisionProfiles();
	TSharedPtr<FString> GetProfileString(FName ProfileName) const;

	TSharedPtr<IPropertyHandle> BodyInstanceHandle;
	TSharedPtr<IPropertyHandle> CollisionProfileNameHandle;
	TSharedPtr<IPropertyHandle> CollisionEnabledHandle;
	TSharedPtr<IPropertyHandle> ObjectTypeHandle;
	TSharedPtr<IPropertyHandle> CollisionResponsesHandle;

	TSharedPtr<SComboBox<TSharedPtr<FString>>> CollisionProfileComboBox;
	TArray<TSharedPtr<FString>> CollisionProfileComboList;

	TSharedPtr<SComboBox<TSharedPtr<FString>>> ObjectTypeComboBox;
	TArray<TSharedPtr<FString>> ObjectTypeComboList;
	TArray<ECollisionChannel> ObjectTypeValues;

	UCollisionProfile* CollisionProfile = nullptr;

	TArray<FBodyInstance*> BodyInstances;
	TArray<FArcCollisionChannelInfo> ValidCollisionChannels;
};

// Thin adapter so IDetailCustomization (which has no IDetailChildrenBuilder) can use the expander.
class FArcBodyInstanceNodeBuilder : public IDetailCustomNodeBuilder, public TSharedFromThis<FArcBodyInstanceNodeBuilder>
{
public:
	FArcBodyInstanceNodeBuilder(TSharedRef<IPropertyHandle> InBodyInstanceHandle)
		: BodyInstanceHandle(InBodyInstanceHandle)
		, Expander(MakeShared<FArcBodyInstanceExpander>())
	{
	}

	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override { OnRegenerateChildren = InOnRegenerateChildren; }
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual bool RequiresTick() const override { return false; }
	virtual bool InitiallyCollapsed() const override { return false; }
	virtual FName GetName() const override { return TEXT("BodyInstanceExpander"); }

private:
	TSharedRef<IPropertyHandle> BodyInstanceHandle;
	TSharedRef<FArcBodyInstanceExpander> Expander;
	FSimpleDelegate OnRegenerateChildren;
};
