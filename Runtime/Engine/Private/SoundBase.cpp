// Copyright Epic Games, Inc. All Rights Reserved.
#include "Sound/SoundBase.h"

#include "AudioDevice.h"
#include "EngineDefines.h"
#include "IAudioExtensionPlugin.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundSubmix.h"
#include "Engine/AssetUserData.h"


USoundClass* USoundBase::DefaultSoundClassObject = nullptr;
USoundConcurrency* USoundBase::DefaultSoundConcurrencyObject = nullptr;

USoundBase::USoundBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, VirtualizationMode(EVirtualizationMode::Restart)
	, Duration(-1.0f)
	, Priority(1.0f)
{
#if WITH_EDITORONLY_DATA
	MaxConcurrentPlayCount_DEPRECATED = 16;
#endif // WITH_EDITORONLY_DATA

	//Migrate bOutputToBusOnly settings to Enablement based UI
	bEnableBusSends = true;
	bEnableBaseSubmix = true;
	bEnableSubmixSends = true;
}

void USoundBase::PostInitProperties()
{
	Super::PostInitProperties();

	if (USoundBase::DefaultSoundClassObject == nullptr)
	{
		const FSoftObjectPath DefaultSoundClassName = GetDefault<UAudioSettings>()->DefaultSoundClassName;
		if (DefaultSoundClassName.IsValid())
		{
			SCOPED_BOOT_TIMING("USoundBase::LoadSoundClass");
			USoundBase::DefaultSoundClassObject = LoadObject<USoundClass>(nullptr, *DefaultSoundClassName.ToString());
		}
	}
	SoundClassObject = USoundBase::DefaultSoundClassObject;

	if (USoundBase::DefaultSoundConcurrencyObject == nullptr)
	{
		const FSoftObjectPath DefaultSoundConcurrencyName = GetDefault<UAudioSettings>()->DefaultSoundConcurrencyName;
		if (DefaultSoundConcurrencyName.IsValid())
		{
			SCOPED_BOOT_TIMING("USoundBase::LoadSoundConcurrency");
			USoundBase::DefaultSoundConcurrencyObject = LoadObject<USoundConcurrency>(nullptr, *DefaultSoundConcurrencyName.ToString());
		}
	}

	if (USoundBase::DefaultSoundConcurrencyObject != nullptr)
	{
		ConcurrencySet.Add(USoundBase::DefaultSoundConcurrencyObject);
	}
}

bool USoundBase::IsPlayable() const
{
	return false;
}

bool USoundBase::SupportsSubtitles() const
{
	return false;
}

bool USoundBase::HasAttenuationNode() const
{
	return false;
}

const FSoundAttenuationSettings* USoundBase::GetAttenuationSettingsToApply() const
{
	if (AttenuationSettings)
	{
		return &AttenuationSettings->Attenuation;
	}
	return nullptr;
}

float USoundBase::GetMaxDistance() const
{
	if (AttenuationSettings)
	{
		FSoundAttenuationSettings& Settings = AttenuationSettings->Attenuation;
		if (Settings.bAttenuate)
		{
			return Settings.GetMaxDimension();
		}
	}

	return WORLD_MAX;
}

float USoundBase::GetDuration()
{
	return Duration;
}

bool USoundBase::HasDelayNode() const
{
	return bHasDelayNode;
}

bool USoundBase::HasConcatenatorNode() const
{
	return bHasConcatenatorNode;
}

bool USoundBase::IsPlayWhenSilent() const
{
	return VirtualizationMode == EVirtualizationMode::PlayWhenSilent;
}

float USoundBase::GetVolumeMultiplier()
{
	return 1.f;
}

float USoundBase::GetPitchMultiplier()
{
	return 1.f;
}

bool USoundBase::IsLooping()
{
	return (GetDuration() >= INDEFINITELY_LOOPING_DURATION);
}

bool USoundBase::ShouldApplyInteriorVolumes()
{
	return (SoundClassObject && SoundClassObject->Properties.bApplyAmbientVolumes);
}

USoundClass* USoundBase::GetSoundClass() const
{
	return SoundClassObject;
}

USoundSubmixBase* USoundBase::GetSoundSubmix() const
{
	return SoundSubmixObject;
}

void USoundBase::GetSoundSubmixSends(TArray<FSoundSubmixSendInfo>& OutSends) const
{
	OutSends = SoundSubmixSends;
}

void USoundBase::GetSoundSourceBusSends(EBusSendType BusSendType, TArray<FSoundSourceBusSendInfo>& OutSends) const
{
	if (BusSendType == EBusSendType::PreEffect)
	{
		OutSends = PreEffectBusSends;
	}
	else
	{
		OutSends = BusSends;
	}
}

void USoundBase::GetConcurrencyHandles(TArray<FConcurrencyHandle>& OutConcurrencyHandles) const
{
	OutConcurrencyHandles.Reset();
	if (bOverrideConcurrency)
	{
		OutConcurrencyHandles.Add(ConcurrencyOverrides);
	}
	else
	{
		for (const USoundConcurrency* Concurrency : ConcurrencySet)
		{
			if (Concurrency)
			{
				OutConcurrencyHandles.Emplace(*Concurrency);
			}
		}
	}
}

float USoundBase::GetPriority() const
{
	return FMath::Clamp(Priority, MIN_SOUND_PRIORITY, MAX_SOUND_PRIORITY);
}

bool USoundBase::GetSoundWavesWithCookedAnalysisData(TArray<USoundWave*>& OutSoundWaves)
{
	return false;
}

#if WITH_EDITORONLY_DATA
void USoundBase::PostLoad()
{
	Super::PostLoad();

	if (bOutputToBusOnly_DEPRECATED)
	{
		bEnableBusSends = true;
		bEnableBaseSubmix = !bOutputToBusOnly_DEPRECATED;
		bEnableSubmixSends = !bOutputToBusOnly_DEPRECATED;
		bOutputToBusOnly_DEPRECATED = false;
	}

	const int32 LinkerUEVersion = GetLinkerUEVersion();

	if (LinkerUEVersion < VER_UE4_SOUND_CONCURRENCY_PACKAGE)
	{
		bOverrideConcurrency = true;
		ConcurrencyOverrides.bLimitToOwner = false;
		ConcurrencyOverrides.MaxCount = FMath::Max(MaxConcurrentPlayCount_DEPRECATED, 1);
		ConcurrencyOverrides.ResolutionRule = MaxConcurrentResolutionRule_DEPRECATED;
	}
}
#endif // WITH_EDITORONLY_DATA

bool USoundBase::CanBeClusterRoot() const
{
	return false;
}

bool USoundBase::CanBeInCluster() const
{
	return false;
}

void USoundBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading() || Ar.IsSaving())
	{
		if (SoundConcurrencySettings_DEPRECATED != nullptr)
		{
			ConcurrencySet.Add(SoundConcurrencySettings_DEPRECATED);
			SoundConcurrencySettings_DEPRECATED = nullptr;
		}
	}
#endif // WITH_EDITORONLY_DATA
}

void USoundBase::AddAssetUserData(UAssetUserData* InUserData)
{
	if (InUserData != nullptr)
	{
		UAssetUserData* ExistingData = GetAssetUserDataOfClass(InUserData->GetClass());
		if (ExistingData != nullptr)
		{
			AssetUserData.Remove(ExistingData);
		}
		AssetUserData.Add(InUserData);
	}
}

UAssetUserData* USoundBase::GetAssetUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != nullptr && Datum->IsA(InUserDataClass))
		{
			return Datum;
		}
	}
	return nullptr;
}

void USoundBase::RemoveUserDataOfClass(TSubclassOf<UAssetUserData> InUserDataClass)
{
	for (int32 DataIdx = 0; DataIdx < AssetUserData.Num(); DataIdx++)
	{
		UAssetUserData* Datum = AssetUserData[DataIdx];
		if (Datum != nullptr && Datum->IsA(InUserDataClass))
		{
			AssetUserData.RemoveAt(DataIdx);
			return;
		}
	}
}

const TArray<UAssetUserData*>* USoundBase::GetAssetUserDataArray() const
{
	return &ToRawPtrTArrayUnsafe(AssetUserData);
}