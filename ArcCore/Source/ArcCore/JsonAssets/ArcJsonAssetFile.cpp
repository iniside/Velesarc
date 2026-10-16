// Copyright Lukasz Baran. All Rights Reserved.

#include "JsonAssets/ArcJsonAssetFile.h"

#include "JsonAssets/ArcJsonAssetsLog.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "JsonAssets/ArcJsonRecordSerializer.h"
#include "Misc/FileHelper.h"
#include "StructUtils/InstancedStruct.h"

bool ArcJsonAssetFile::ParseRecord(const FArcJsonAssetTypeInfo& Type, TArray<uint8>& Utf8, const FString& Filename, FInstancedStruct& OutRecord)
{
	if (!Type.Struct)
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls: type %ls has no struct", *Filename, *Type.PrimaryAssetType.ToString());
		return false;
	}

	FInstancedStruct Record;
	FArcJsonRecordReport Report;
	const bool bParsed = ArcJsonRecord::ParseRecord(*Type.Struct, Utf8, Record, Report);
	for (const FString& Warning : Report.Warnings)
	{
		UE_LOGF(LogArcJsonAssets, Warning, "ArcJsonAssets: %ls: %ls", *Filename, *Warning);
	}
	if (!bParsed || Report.Errors.Num() > 0)
	{
		for (const FString& Error : Report.Errors)
		{
			UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls: %ls", *Filename, *Error);
		}
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls rejected with %d errors", *Filename, Report.Errors.Num());
		return false;
	}

	OutRecord = MoveTemp(Record);
	return true;
}

bool ArcJsonAssetFile::ReadRecordFile(const FArcJsonAssetTypeInfo& Type, const FString& Filename, FInstancedStruct& OutRecord)
{
	TArray<uint8> Utf8;
	if (!FFileHelper::LoadFileToArray(Utf8, *Filename))
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls could not be read", *Filename);
		return false;
	}
	return ParseRecord(Type, Utf8, Filename, OutRecord);
}

TArray<uint8> ArcJsonAssetFile::FormatRecord(const FInstancedStruct& Record)
{
	return ArcJsonRecord::FormatRecord(Record);
}

bool ArcJsonAssetFile::WriteRecordFile(const FString& Filename, const FInstancedStruct& Record)
{
	TArray<uint8> Utf8;
	return WriteRecordFile(Filename, Record, Utf8);
}

bool ArcJsonAssetFile::WriteRecordFile(const FString& Filename, const FInstancedStruct& Record, TArray<uint8>& OutUtf8)
{
	OutUtf8 = FormatRecord(Record);
	if (OutUtf8.IsEmpty())
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls was not written because the record could not be formatted", *Filename);
		return false;
	}
	if (!FFileHelper::SaveArrayToFile(OutUtf8, *Filename))
	{
		UE_LOGF(LogArcJsonAssets, Error, "ArcJsonAssets: %ls could not be written", *Filename);
		return false;
	}
	return true;
}
