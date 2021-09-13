// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "SourceControlHelpers.h"

class UNREALED_API FPackageSourceControlHelper
{
public:
	bool UseSourceControl() const;
	bool Delete(const FString& PackageName) const;
	bool Delete(UPackage* Package) const;
	bool Delete(const TArray<UPackage*>& Packages) const;
	bool AddToSourceControl(UPackage* Package) const;
	bool Checkout(UPackage* Package) const;

private:
	ISourceControlProvider& GetSourceControlProvider() const;
	FScopedSourceControl SourceControl;	
};
