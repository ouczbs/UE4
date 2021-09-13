// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "DerivedDataPluginInterface.h"
#endif

#include "Animation/AnimCompressionTypes.h"

struct FAnimCompressContext;

#if WITH_EDITOR

//////////////////////////////////////////////////////////////////////////
// FDerivedDataAnimationCompression
class FDerivedDataAnimationCompression : public FDerivedDataPluginInterface
{
private:
	// The anim data to compress
	FCompressibleAnimPtr DataToCompressPtr;

	// The Type of anim data to compress (makes up part of DDC key)
	const TCHAR* TypeName;

	// Bulk of asset DDC key
	const FString AssetDDCKey;

	// FAnimCompressContext to use during compression if we don't pull from the DDC
	TSharedPtr<FAnimCompressContext> CompressContext;

public:
	FDerivedDataAnimationCompression(const TCHAR* InTypeName, const FString& InAssetDDCKey, TSharedPtr<FAnimCompressContext> InCompressContext);
	virtual ~FDerivedDataAnimationCompression();

	void SetCompressibleData(FCompressibleAnimRef InCompressibleAnimData)
	{
		DataToCompressPtr = InCompressibleAnimData;
	}

	FCompressibleAnimPtr GetCompressibleData() const { return DataToCompressPtr; }

	uint64 GetMemoryUsage() const
	{
		return DataToCompressPtr.IsValid() ? DataToCompressPtr->GetApproxMemoryUsage() : 0;
	}

	virtual const TCHAR* GetPluginName() const override
	{
		return TypeName;
	}

	virtual const TCHAR* GetVersionString() const override;

	virtual FString GetPluginSpecificCacheKeySuffix() const override
	{
		return AssetDDCKey;
	}


	virtual bool IsBuildThreadsafe() const override
	{
		return true;
	}

	virtual bool Build( TArray<uint8>& OutDataArray) override;

	/** Return true if we can build **/
	bool CanBuild()
	{
		return DataToCompressPtr.IsValid();
	}
};

#endif	//WITH_EDITOR
