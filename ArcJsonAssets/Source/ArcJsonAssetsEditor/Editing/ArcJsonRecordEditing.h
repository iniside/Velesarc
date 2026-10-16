// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/Object.h"
#include "ArcJsonRecordEditing.generated.h"

class UArcJsonRecordEditObject;
struct FArcJsonAssetTypeInfo;

enum class EArcJsonRecordEditChange : uint8
{
	Edited,
	UndoRedo,
	Reloaded,
	Saved,
	Discarded,
	Removed
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FArcJsonRecordEditChanged, UArcJsonRecordEditObject* /*Object*/, EArcJsonRecordEditChange /*Change*/);
DECLARE_DELEGATE_RetVal_OneParam(bool, FArcJsonRecordEditorOpen, const FString& /*Filename*/);

UCLASS(Transient)
class ARCJSONASSETSEDITOR_API UArcJsonRecordEditObject : public UObject
{
	GENERATED_BODY()

public:
	const FString& GetFilename() const;
	const FInstancedStruct& GetRecord() const;
	FInstancedStruct& GetMutableRecord();
	bool IsDirty() const;
	bool IsOpen() const;
	bool Save();
	void Discard();
	FArcJsonRecordEditChanged& OnChanged();

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;

private:
	friend class FArcJsonAssetsEditorModule;

	bool LoadFromDisk(const FString& InFilename);
	void HandleFileChangedOnDisk();
	void HandleFileRemoved();
	void Close(EArcJsonRecordEditChange Change);

	UPROPERTY()
	FInstancedStruct Record;

	FString Filename;
	TArray<uint8> DiskUtf8;
	FArcJsonRecordEditChanged Changed;
	int32 HolderCount = 0;
	bool bDirty = false;
	bool bOpen = false;
};

namespace ArcJsonRecordEditing
{
	// Each non-null AcquireRecord is paired with one ReleaseRecord; the object leaves the registry at the last release or on Discard.
	ARCJSONASSETSEDITOR_API UArcJsonRecordEditObject* AcquireRecord(const FString& Filename);
	ARCJSONASSETSEDITOR_API void ReleaseRecord(UArcJsonRecordEditObject* Object);
	ARCJSONASSETSEDITOR_API UArcJsonRecordEditObject* FindRecord(const FString& Filename);

	ARCJSONASSETSEDITOR_API void BrowseToRecordFile(const FString& Filename);

	ARCJSONASSETSEDITOR_API bool CreateRecordFile(const FArcJsonAssetTypeInfo& Type, const FString& Filename);

	ARCJSONASSETSEDITOR_API void RegisterRecordEditor(const FString& Extension, const FArcJsonRecordEditorOpen& Open);
	ARCJSONASSETSEDITOR_API void UnregisterRecordEditor(const FString& Extension);
	ARCJSONASSETSEDITOR_API bool OpenRecordEditor(const FString& Filename);
}
