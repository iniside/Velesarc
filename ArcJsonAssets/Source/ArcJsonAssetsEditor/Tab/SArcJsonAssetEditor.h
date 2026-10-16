// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Editing/ArcJsonRecordEditing.h"
#include "Misc/NotifyHook.h"
#include "Widgets/SCompoundWidget.h"

class IStructureDetailsView;

class SArcJsonAssetEditor : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SArcJsonAssetEditor) {}
		SLATE_ARGUMENT(FString, Filename)
		SLATE_EVENT(FSimpleDelegate, OnRecordClosed)
	SLATE_END_ARGS()

	virtual ~SArcJsonAssetEditor() override;

	void Construct(const FArguments& InArgs);

	bool IsLoaded() const;
	FText GetTabLabel() const;
	bool CanClose();
	void HandleTabClosed();
	bool Save();

	virtual bool SupportsKeyboardFocus() const override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

private:
	void BindRecord(UArcJsonRecordEditObject* Object);
	void UnbindRecord();
	void RefreshDetails();
	void HandleRecordChanged(UArcJsonRecordEditObject* Object, EArcJsonRecordEditChange Change);
	FReply HandleSaveClicked();

	FString Filename;
	FSimpleDelegate OnRecordClosed;
	TWeakObjectPtr<UArcJsonRecordEditObject> Record;
	FDelegateHandle RecordChangedHandle;
	TSharedPtr<IStructureDetailsView> DetailsView;
	bool bApplyingEdit = false;
	bool bClosing = false;
	uint64 DiscardOnCloseFrame = MAX_uint64;
};
