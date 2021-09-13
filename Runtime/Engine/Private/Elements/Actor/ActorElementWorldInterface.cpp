// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Actor/ActorElementWorldInterface.h"
#include "Elements/Actor/ActorElementData.h"
#include "GameFramework/Actor.h"

#include "Elements/Component/ComponentElementData.h"
#include "Components/PrimitiveComponent.h"

bool UActorElementWorldInterface::IsTemplateElement(const FTypedElementHandle& InElementHandle)
{
	const AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle);
	return Actor && Actor->IsTemplate();
}

ULevel* UActorElementWorldInterface::GetOwnerLevel(const FTypedElementHandle& InElementHandle)
{
	const AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle);
	return Actor ? Actor->GetLevel() : nullptr;
}

UWorld* UActorElementWorldInterface::GetOwnerWorld(const FTypedElementHandle& InElementHandle)
{
	const AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle);
	return Actor ? Actor->GetWorld() : nullptr;
}

bool UActorElementWorldInterface::GetBounds(const FTypedElementHandle& InElementHandle, FBoxSphereBounds& OutBounds)
{
	if (const AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle))
	{
		// TODO: This was taken from FActorOrComponent, but AActor has a function to calculate bounds too...
		OutBounds = Actor->GetRootComponent()->Bounds;
		return true;
	}

	return false;
}

bool UActorElementWorldInterface::GetWorldTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform)
{
	if (const AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle))
	{
		OutTransform = Actor->GetActorTransform();
		return true;
	}

	return false;
}

bool UActorElementWorldInterface::SetWorldTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform)
{
	if (AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle))
	{
		Actor->Modify();
		return Actor->SetActorTransform(InTransform);
	}

	return false;
}

bool UActorElementWorldInterface::GetRelativeTransform(const FTypedElementHandle& InElementHandle, FTransform& OutTransform)
{
	if (const AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle))
	{
		if (const USceneComponent* RootComponent = Actor->GetRootComponent())
		{
			OutTransform = RootComponent->GetRelativeTransform();
		}
		else
		{
			OutTransform = FTransform::Identity;
		}
		return true;
	}

	return false;
}

bool UActorElementWorldInterface::SetRelativeTransform(const FTypedElementHandle& InElementHandle, const FTransform& InTransform)
{
	if (AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle))
	{
		if (USceneComponent* RootComponent = Actor->GetRootComponent())
		{
			Actor->Modify();
			RootComponent->SetRelativeTransform(InTransform);
			return true;
		}
	}

	return false;
}

bool UActorElementWorldInterface::FindSuitableTransformAtPoint(const FTypedElementHandle& InElementHandle, const FTransform& InPotentialTransform, FTransform& OutSuitableTransform)
{
	if (const AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle))
	{
		UWorld* World = Actor->GetWorld();
		const UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(Actor->GetRootComponent());

		if (World && PrimComponent && PrimComponent->IsQueryCollisionEnabled())
		{
			const FVector PivotOffset = PrimComponent->Bounds.Origin - Actor->GetActorLocation();

			FVector NewLocation = InPotentialTransform.GetTranslation();
			FRotator NewRotation = InPotentialTransform.Rotator();

			// Apply the pivot offset to the desired location
			NewLocation += PivotOffset;

			// Check if able to find an acceptable destination for this actor that doesn't embed it in world geometry
			if (World->FindTeleportSpot(Actor, NewLocation, NewRotation))
			{
				// Undo the pivot offset
				NewLocation -= PivotOffset;

				OutSuitableTransform = InPotentialTransform;
				OutSuitableTransform.SetTranslation(NewLocation);
				OutSuitableTransform.SetRotation(NewRotation.Quaternion());
				return true;
			}
		}
		else
		{
			OutSuitableTransform = InPotentialTransform;
			return true;
		}
	}

	return false;
}

bool UActorElementWorldInterface::FindSuitableTransformAlongPath(const FTypedElementHandle& InElementHandle, const FVector& InPathStart, const FVector& InPathEnd, const FCollisionShape& InTestShape, TArrayView<const FTypedElementHandle> InElementsToIgnore, FTransform& OutSuitableTransform)
{
	if (const AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle))
	{	
		if (const UWorld* World = Actor->GetWorld())
		{
			FCollisionQueryParams Params(SCENE_QUERY_STAT(FindSuitableTransformAlongPath), false);
			
			// Don't hit ourself
			{
				Params.AddIgnoredActor(Actor);

				TArray<AActor*> ChildActors;
				Actor->GetAllChildActors(ChildActors);
				Params.AddIgnoredActors(ChildActors);
			}

			return UActorElementWorldInterface::FindSuitableTransformAlongPath_WorldSweep(World, InPathStart, InPathEnd, InTestShape, InElementsToIgnore, Params, OutSuitableTransform);
		}
	}

	return false;
}

bool UActorElementWorldInterface::FindSuitableTransformAlongPath_WorldSweep(const UWorld* InWorld, const FVector& InPathStart, const FVector& InPathEnd, const FCollisionShape& InTestShape, TArrayView<const FTypedElementHandle> InElementsToIgnore, FCollisionQueryParams& InOutParams, FTransform& OutSuitableTransform)
{
	for (const FTypedElementHandle& ElementToIgnore : InElementsToIgnore)
	{
		AddIgnoredCollisionQueryElement(ElementToIgnore, InOutParams);
	}

	FHitResult Hit(1.0f);
	if (InWorld->SweepSingleByChannel(Hit, InPathStart, InPathEnd, FQuat::Identity, ECC_WorldStatic, InTestShape, InOutParams))
	{
		FVector NewLocation = Hit.Location;
		NewLocation.Z += KINDA_SMALL_NUMBER; // Move the new desired location up by an error tolerance

		//@todo: This doesn't take into account that rotating the actor changes LocationOffset.
		FRotator NewRotation = Hit.Normal.Rotation();
		NewRotation.Pitch -= 90.f;

		OutSuitableTransform.SetTranslation(NewLocation);
		OutSuitableTransform.SetRotation(NewRotation.Quaternion());
		OutSuitableTransform.SetScale3D(FVector::OneVector);
		return true;
	}

	return false;
}

void UActorElementWorldInterface::AddIgnoredCollisionQueryElement(const FTypedElementHandle& InElementHandle, FCollisionQueryParams& InOutParams)
{
	if (const AActor* Actor = ActorElementDataUtil::GetActorFromHandle(InElementHandle, /*bSilent*/true))
	{
		InOutParams.AddIgnoredActor(Actor);
		return;
	}

	if (const UActorComponent* Component = ComponentElementDataUtil::GetComponentFromHandle(InElementHandle, /*bSilent*/true))
	{
		if (const UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(Component))
		{
			InOutParams.AddIgnoredComponent(PrimComponent);
		}
		return;
	}
}
