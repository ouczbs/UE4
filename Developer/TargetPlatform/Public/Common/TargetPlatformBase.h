// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/ITargetPlatform.h"
#include "PlatformInfo.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"

/**
 * Base class for target platforms.
 */
class FTargetPlatformBase
	: public ITargetPlatform
{
public:

	// ITargetPlatform interface

	virtual bool AddDevice( const FString& DeviceName, bool bDefault ) override
	{
		return false;
	}

	virtual bool AddDevice(const FString& DeviceId, const FString& DeviceUserFriendlyName, const FString& Username, const FString& Password, bool bDefault) override
	{
		return AddDevice(DeviceId, bDefault);
	}

	virtual FText DisplayName() const override
	{
		return PlatformInfo->DisplayName;
	}

	virtual const PlatformInfo::FTargetPlatformInfo& GetTargetPlatformInfo() const override
	{
		return *PlatformInfo;
	}

	virtual const FDataDrivenPlatformInfo& GetPlatformInfo() const override
	{
		return *PlatformInfo->DataDrivenPlatformInfo;
	}

	virtual FConfigCacheIni* GetConfigSystem() const override
	{
		return FConfigCacheIni::ForPlatform(*IniPlatformName());
	}

	TARGETPLATFORM_API virtual bool UsesForwardShading() const override;

	TARGETPLATFORM_API virtual bool UsesDBuffer() const override;

	TARGETPLATFORM_API virtual bool UsesBasePassVelocity() const override;

    TARGETPLATFORM_API virtual bool VelocityEncodeDepth() const override;

    TARGETPLATFORM_API virtual bool UsesSelectiveBasePassOutputs() const override;
	
	TARGETPLATFORM_API virtual bool UsesDistanceFields() const override;

	TARGETPLATFORM_API virtual bool UsesRayTracing() const override;

	TARGETPLATFORM_API virtual bool ForcesSimpleSkyDiffuse() const override;

	TARGETPLATFORM_API virtual float GetDownSampleMeshDistanceFieldDivider() const override;

	TARGETPLATFORM_API virtual int32 GetHeightFogModeForOpaque() const override;

#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const override
	{
		OutFormats.Add(FName(TEXT("FullHDR")));
	}

	virtual FName FinalizeVirtualTextureLayerFormat(FName Format) const override
	{
		return Format;
	}

	virtual FName GetVirtualTextureLayerFormat(
		int32 SourceFormat,
		bool bAllowCompression, bool bNoAlpha,
		bool bSupportDX11TextureFormats, int32 Settings) const override
	{
		return FName();
	}
#endif //WITH_ENGINE

	virtual bool PackageBuild( const FString& InPackgeDirectory ) override
	{
		return true;
	}

	virtual bool CanSupportRemoteShaderCompile() const override
	{
		return true;
	}

	virtual void GetShaderCompilerDependencies(TArray<FString>& OutDependencies) const override
	{
	}

	/** Helper method to fill a dependencies array for the shader compiler with absolute paths, passing a relative path to the Engine as the parameter. */
	static void AddDependencySCArrayHelper(TArray<FString>& OutDependencies, const FString& DependencyRelativePath)
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FString DependencyAbsolutePath = PlatformFile.ConvertToAbsolutePathForExternalAppForRead(*(FPaths::EngineDir() / DependencyRelativePath));
		FPaths::NormalizeDirectoryName(DependencyAbsolutePath);
		OutDependencies.AddUnique(DependencyAbsolutePath);
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override
	{
		return true;
	}

	virtual int32 CheckRequirements(bool bProjectHasCode, EBuildConfiguration Configuration, bool bRequiresAssetNativization, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override
	{
		int32 bReadyToBuild = ETargetPlatformReadyStatus::Ready; // @todo How do we check that the iOS SDK is installed when building from Windows? Is that even possible?
		if (!IsSdkInstalled(bProjectHasCode, OutTutorialPath))
		{
			bReadyToBuild |= ETargetPlatformReadyStatus::SDKNotFound;
		}
		return bReadyToBuild;
	}

	TARGETPLATFORM_API virtual bool RequiresTempTarget(bool bProjectHasCode, EBuildConfiguration Configuration, bool bRequiresAssetNativization, FText& OutReason) const override;

	virtual bool SupportsValueForType(FName SupportedType, FName RequiredSupportedValue) const override
	{
#if WITH_ENGINE
		// check if the given shader format is returned by this TargetPlatform
		if (SupportedType == TEXT("ShaderFormat"))
		{
			TArray<FName> AllPossibleShaderFormats;
			GetAllPossibleShaderFormats(AllPossibleShaderFormats);
			return AllPossibleShaderFormats.Contains(RequiredSupportedValue);
		}
#endif
		return false;
	}

	virtual bool SupportsVariants() const override
	{
		return false;
	}

	virtual float GetVariantPriority() const override
	{
		return IsClientOnly() ? 0.0f : 0.2f;
	}

	virtual bool SendLowerCaseFilePaths() const override
	{
		return false;
	}

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		// do nothing in the base class
	}

	virtual void RefreshSettings() override
	{
	}

	virtual int32 GetPlatformOrdinal() const override
	{
		return PlatformOrdinal;
	}

	TARGETPLATFORM_API virtual TSharedPtr<IDeviceManagerCustomPlatformWidgetCreator> GetCustomWidgetCreator() const override;

	virtual bool ShouldExpandTo32Bit(const uint16* Indices, const int32 NumIndices) const override
	{
		return false;
	}

#if WITH_ENGINE
	virtual FName GetMeshBuilderModuleName() const override
	{
		// MeshBuilder is the default module. Platforms may override this to provide platform specific mesh data.
		static const FName NAME_MeshBuilder(TEXT("MeshBuilder"));
		return NAME_MeshBuilder;
	}
#endif

	virtual bool CopyFileToTarget(const FString& TargetAddress, const FString& HostFilename, const FString& TargetFilename, const TMap<FString,FString>& CustomPlatformData) override
	{
		return false; 
	}

	virtual bool InitializeHostPlatform()
	{
		// if the platform doesn't need anything, it's valid to do nothing
		return true;
	}


protected:

	FTargetPlatformBase(const PlatformInfo::FTargetPlatformInfo *const InPlatformInfo)
		: PlatformInfo(InPlatformInfo)
	{
		checkf(PlatformInfo, TEXT("Null PlatformInfo was passed to FTargetPlatformBase. Check the static IsUsable function before creating this object. See FWindowsTargetPlatformModule::GetTargetPlatform()"));

		PlatformOrdinal = AssignPlatformOrdinal(*this);
	}

	/** Information about this platform */
	const PlatformInfo::FTargetPlatformInfo *PlatformInfo;
	int32 PlatformOrdinal;

private:
	bool HasDefaultBuildSettings() const;
	static bool DoProjectSettingsMatchDefault(const FString& InPlatformName, const FString& InSection, const TArray<FString>* InBoolKeys, const TArray<FString>* InIntKeys, const TArray<FString>* InStringKeys);
};


/**
 * Template for target platforms.
 *
 * @param TPlatformProperties Type of platform properties.
 */
template<typename TPlatformProperties>
class TTargetPlatformBase
	: public FTargetPlatformBase
{
public:

	/**
	 * Returns true if the target platform will be able to be  initialized with an FPlatformInfo. Because FPlatformInfo now comes from a .ini file,
	 * it's possible that the .dll exists, but the .ini does not (should be uncommon, but is necessary to be handled)
	 */
	static bool IsUsable()
	{
		return true;
	}



	/**
	 * Constructor that already has a TPI (notably coming from TNonDesktopTargetPlatform)
	 */
	TTargetPlatformBase(PlatformInfo::FTargetPlatformInfo* PremadePlatformInfo)
		: FTargetPlatformBase(PremadePlatformInfo)
	{
		// HasEditorOnlyData and RequiresCookedData are mutually exclusive.
		check(TPlatformProperties::HasEditorOnlyData() != TPlatformProperties::RequiresCookedData());
	}

	/**
	 * Constructor that makes a TPI based solely on TPlatformProperties
	 */
	TTargetPlatformBase()
		: TTargetPlatformBase( new PlatformInfo::FTargetPlatformInfo(
			TPlatformProperties::IniPlatformName(),
			TPlatformProperties::HasEditorOnlyData() ? EBuildTargetType::Editor :
				TPlatformProperties::IsServerOnly() ? EBuildTargetType::Server : 
				TPlatformProperties::IsClientOnly() ? EBuildTargetType::Client : 
				EBuildTargetType::Game,
			TEXT(""))
		)
	{
	}

public:

	// ITargetPlatform interface

	virtual bool HasEditorOnlyData() const override
	{
		return TPlatformProperties::HasEditorOnlyData();
	}

	virtual bool IsLittleEndian() const override
	{
		return TPlatformProperties::IsLittleEndian();
	}

	virtual bool IsServerOnly() const override
	{
		return TPlatformProperties::IsServerOnly();
	}

	virtual bool IsClientOnly() const override
	{
		return TPlatformProperties::IsClientOnly();
	}

	virtual FString PlatformName() const override
	{
		// we assume these match for DesktopPlatforms (NonDesktop doesn't return "FooClient", but Desktop does, for legacy reasons)
		checkSlow(this->PlatformInfo->Name == TPlatformProperties::PlatformName());
		
		return FString(TPlatformProperties::PlatformName());
	}

	virtual FString IniPlatformName() const override
	{
		return FString(TPlatformProperties::IniPlatformName());
	}

	virtual bool RequiresCookedData() const override
	{
		return TPlatformProperties::RequiresCookedData();
	}

	virtual bool HasSecurePackageFormat() const override
	{
		return TPlatformProperties::HasSecurePackageFormat();
	}

	virtual EPlatformAuthentication RequiresUserCredentials() const override
	{
		if (TPlatformProperties::RequiresUserCredentials())
		{
			return EPlatformAuthentication::Always;
		}
		else
		{
			return EPlatformAuthentication::Never;
		}
	}

	virtual bool SupportsBuildTarget( EBuildTargetType TargetType ) const override
	{
		return TPlatformProperties::SupportsBuildTarget(TargetType);
	}

	virtual bool SupportsAutoSDK() const override
	{
		return TPlatformProperties::SupportsAutoSDK();
	}

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override
	{
		switch (Feature)
		{
		case ETargetPlatformFeatures::AudioStreaming:
			return TPlatformProperties::SupportsAudioStreaming();

		case ETargetPlatformFeatures::DistanceFieldShadows:
			return TPlatformProperties::SupportsDistanceFieldShadows();

		case ETargetPlatformFeatures::DistanceFieldAO:
			return TPlatformProperties::SupportsDistanceFieldAO();

		case ETargetPlatformFeatures::GrayscaleSRGB:
			return TPlatformProperties::SupportsGrayscaleSRGB();

		case ETargetPlatformFeatures::HighQualityLightmaps:
			return TPlatformProperties::SupportsHighQualityLightmaps();

		case ETargetPlatformFeatures::LowQualityLightmaps:
			return TPlatformProperties::SupportsLowQualityLightmaps();

		case ETargetPlatformFeatures::MultipleGameInstances:
			return TPlatformProperties::SupportsMultipleGameInstances();

		case ETargetPlatformFeatures::Packaging:
			return false;

		case ETargetPlatformFeatures::Tessellation:
			return TPlatformProperties::SupportsTessellation();

		case ETargetPlatformFeatures::TextureStreaming:
			return TPlatformProperties::SupportsTextureStreaming();
		case ETargetPlatformFeatures::MeshLODStreaming:
			return TPlatformProperties::SupportsMeshLODStreaming();
		case ETargetPlatformFeatures::LandscapeMeshLODStreaming:
			return false;

		case ETargetPlatformFeatures::MemoryMappedFiles:
			return TPlatformProperties::SupportsMemoryMappedFiles();

		case ETargetPlatformFeatures::MemoryMappedAudio:
			return TPlatformProperties::SupportsMemoryMappedAudio();
		case ETargetPlatformFeatures::MemoryMappedAnimation:
			return TPlatformProperties::SupportsMemoryMappedAnimation();

		case ETargetPlatformFeatures::VirtualTextureStreaming:
			return TPlatformProperties::SupportsVirtualTextureStreaming();

		case ETargetPlatformFeatures::SdkConnectDisconnect:
		case ETargetPlatformFeatures::UserCredentials:
			break;

		case ETargetPlatformFeatures::MobileRendering:
			return false;
		case ETargetPlatformFeatures::DeferredRendering:
			return true;

		case ETargetPlatformFeatures::ShouldSplitPaksIntoSmallerSizes :
			return false;

		case ETargetPlatformFeatures::HalfFloatVertexFormat:
			return true;

		case ETargetPlatformFeatures::LumenGI:
			return TPlatformProperties::SupportsLumenGI();

		case ETargetPlatformFeatures::HardwareLZDecompression:
			return TPlatformProperties::SupportsHardwareLZDecompression();
		}

		return false;
	}
	virtual FName GetZlibReplacementFormat() const override
	{
		return TPlatformProperties::GetZlibReplacementFormat() != nullptr ? FName(TPlatformProperties::GetZlibReplacementFormat()) : NAME_Zlib;
	}

	virtual int32 GetMemoryMappingAlignment() const override
	{
		return TPlatformProperties::GetMemoryMappingAlignment();
	}


#if WITH_ENGINE
	virtual FName GetPhysicsFormat( class UBodySetup* Body ) const override
	{
		return FName(TPlatformProperties::GetPhysicsFormat());
	}
#endif // WITH_ENGINE
};


template<typename TPlatformProperties>
class TNonDesktopTargetPlatformBase 
	: public TTargetPlatformBase<TPlatformProperties>
{
public:
	/**
	 * A simplified version for TPs that never will have Editor or ServerOnly versions, potentially multiple CookFlavors, as well as IN VERY RARE CASES, 
	 * a different runtime IniPlatformName than what is passed in here (an example being TVOS and IOS, where passing in TVOS properties is very complicated)
	 * Note that if we delayed the Info creation, we could just use this->IniPlatformName() and override that in, say TVOS, but we can't call a virtual here,
	 * so we pass it up into the ctor
	 */
	TNonDesktopTargetPlatformBase(bool bInIsClientOnly, const TCHAR* CookFlavor=nullptr, const TCHAR* OverrideIniPlatformName=nullptr)
		: TTargetPlatformBase<TPlatformProperties>(new PlatformInfo::FTargetPlatformInfo(
			OverrideIniPlatformName ? FString(OverrideIniPlatformName) : FString(TPlatformProperties::IniPlatformName()),
			bInIsClientOnly ? EBuildTargetType::Client : EBuildTargetType::Game,
			CookFlavor))
		, bIsClientOnly(bInIsClientOnly)
	{

	}

	virtual FString PlatformName() const override
	{
		// instead of TPlatformProperties (which won't have Client for non-desktop platforms), use the Info's name, which is programmatically made
		return this->PlatformInfo->Name.ToString();
	}

	virtual FString IniPlatformName() const override
	{
		// we use the Info's IniPlatformName as it may have been overridden in the constructor IN RARE CASES
		return this->PlatformInfo->IniPlatformName.ToString();
	}

	virtual bool HasEditorOnlyData() const override
	{
		return false;
	}

	virtual bool IsServerOnly() const override
	{
		return false;
	}

	virtual bool IsClientOnly() const override
	{
		return bIsClientOnly;
	}

	virtual bool IsRunningPlatform() const override
	{
		// IsRunningPlatform is only for editor platforms

		return false;
	}

protected:
	// true if this target platform is client-only, ie strips out server stuff
	bool bIsClientOnly;
};


