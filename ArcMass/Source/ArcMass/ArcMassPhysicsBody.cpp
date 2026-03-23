// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsBody.h"
#include "PhysicsEngine/BodyInstance.h"

void FArcMassPhysicsBodyFragment::TerminateBodies()
{
    for (FBodyInstance*& Body : Bodies)
    {
        if (Body)
        {
            Body->TermBody();
            delete Body;
            Body = nullptr;
        }
    }
    Bodies.Empty();
}
