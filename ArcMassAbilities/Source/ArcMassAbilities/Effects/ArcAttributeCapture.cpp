// Copyright Lukasz Baran. All Rights Reserved.

#include "Effects/ArcAttributeCapture.h"

bool FArcAttributeCaptureDef::operator==(const FArcAttributeCaptureDef& Other) const
{
	return Attribute == Other.Attribute
		&& CaptureSource == Other.CaptureSource
		&& bSnapshot == Other.bSnapshot;
}

uint32 GetTypeHash(const FArcAttributeCaptureDef& Def)
{
	uint32 Hash = GetTypeHash(Def.Attribute);
	Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Def.CaptureSource)));
	Hash = HashCombine(Hash, GetTypeHash(Def.bSnapshot));
	return Hash;
}
