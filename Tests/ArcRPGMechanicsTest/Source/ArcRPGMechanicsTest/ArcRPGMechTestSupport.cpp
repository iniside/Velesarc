// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRPGMechTestSupport.h"

ArcRPGMechTest::FVerboseLogScope::FVerboseLogScope()
	: MechanicsScope(TEXT("LogArcMassMechanics"))
	, RPGScope(TEXT("LogArcRPGMechanics"))
{
}
