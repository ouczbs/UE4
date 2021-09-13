// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quartz/QuartzMetronome.h"
#include "Sound/QuartzSubscription.h"

namespace Audio
{
	FQuartzMetronome::FQuartzMetronome()
	{
		SetTickRate(CurrentTickRate);
	}

	FQuartzMetronome::FQuartzMetronome(const FQuartzTimeSignature& InTimeSignature)
		: CurrentTimeSignature(InTimeSignature)
	{
		SetTickRate(CurrentTickRate);
	}

	FQuartzMetronome::~FQuartzMetronome()
	{
	}

	void FQuartzMetronome::Tick(int32 InNumSamples, int32 FramesOfLatency)
	{
		// TODO: update latency
		static bool bHasWarned = false;
		if (!bHasWarned && (MusicalDurationsInFrames[EQuartzCommandQuantization::ThirtySecondNote] < InNumSamples))
		{
			// TODO: fire duplicate events if this occurs to facilitate game play-side counting logic
			UE_LOG(LogAudioQuartz, Warning
				, TEXT("Small note durations are shorter than the audio callback size. Some musical events may not fire delegates"));

			bHasWarned = true;
		}

		int32 ToUpdateBitField = 0;

		for (int i = 0; i < static_cast<int32>(EQuartzCommandQuantization::Count); ++i)
		{
			EQuartzCommandQuantization DurationType = static_cast<EQuartzCommandQuantization>(i);
			FramesLeftInMusicalDuration[DurationType] -= InNumSamples;
			
			if (FramesLeftInMusicalDuration[DurationType] < 0)
			{
				// flag this duration for an update
				ToUpdateBitField |= (1 << i);

				// the beat value is constant
				if (!(DurationType == EQuartzCommandQuantization::Beat && PulseDurations.Num()))
				{
					do
					{
						FramesLeftInMusicalDuration[DurationType] += MusicalDurationsInFrames[DurationType];
					}
					while (FramesLeftInMusicalDuration[DurationType] <= 0);
				}
				else
				{
					// the beat value can change
					do
					{
						if (++PulseDurationIndex == PulseDurations.Num())
						{
							PulseDurationIndex = 0;
						}

						FramesLeftInMusicalDuration[DurationType] += PulseDurations[PulseDurationIndex];
					}
					while (FramesLeftInMusicalDuration[DurationType] <= 0);
				}
			}
		}

		// update transport
		if (ToUpdateBitField & (1 << static_cast<int>(EQuartzCommandQuantization::Bar)))
		{
			++CurrentTimeStamp.Bars;
			CurrentTimeStamp.Beat = 0;
		}
		else if (ToUpdateBitField & (1 << static_cast<int>(EQuartzCommandQuantization::Beat)))
		{
			++CurrentTimeStamp.Beat;
		}

		FireEvents(ToUpdateBitField);
	}

	void FQuartzMetronome::SetTickRate(FQuartzClockTickRate InNewTickRate, int32 NumFramesLeft)
	{
		// early exit?
		const bool bSameAsOldTickRate = (InNewTickRate.GetFramesPerTick() == CurrentTickRate.GetFramesPerTick());
		const bool bIsInitialized = (MusicalDurationsInFrames[0] > 0);

		if (bSameAsOldTickRate && bIsInitialized)
		{
			return;
		}

		// ratio between new and old rates
		const float Ratio = static_cast<float>(InNewTickRate.GetFramesPerTick()) / static_cast<float>(CurrentTickRate.GetFramesPerTick());

		if (NumFramesLeft)
		{
			for (int32& Value : FramesLeftInMusicalDuration.FramesInTimeValueInternal)
			{
				Value = NumFramesLeft + Ratio * (Value - NumFramesLeft);
			}
		}

		CurrentTickRate = InNewTickRate;
		RecalculateDurations();
	}

	void FQuartzMetronome::SetSampleRate(float InNewSampleRate)
	{
		CurrentTickRate.SetSampleRate(InNewSampleRate);
		RecalculateDurations();
	}

	void FQuartzMetronome::SetTimeSignature(const FQuartzTimeSignature& InNewTimeSignature)
	{
		CurrentTimeSignature = InNewTimeSignature;
		RecalculateDurations();
	}

	int32 FQuartzMetronome::GetFramesUntilBoundary(FQuartzQuantizationBoundary InQuantizationBoundary) const
	{
		if(!ensure(InQuantizationBoundary.Quantization != EQuartzCommandQuantization::None))
		{
			return 0; // Metronome's should not have to deal w/ Quartization == None
		}

		if (InQuantizationBoundary.Multiplier < 1.0f)
		{
			UE_LOG(LogAudioQuartz, Warning, TEXT("Quantizatoin Boundary being clamped to 1.0 (from %f)"), InQuantizationBoundary.Multiplier);
			InQuantizationBoundary.Multiplier = 1.f;
		}

		// number of frames until the next occurrence of this boundary
		int32 FramesUntilBoundary = FramesLeftInMusicalDuration[InQuantizationBoundary.Quantization];

		// in the simple case that's all we need to know
		bool bIsSimpleCase = FMath::IsNearlyEqual(InQuantizationBoundary.Multiplier, 1.f);

		// it is NOT the simple case if we are in Bar-Relative. // i.e. 1.f Beat here means "Beat 1 of the bar"
		bIsSimpleCase &= (InQuantizationBoundary.CountingReferencePoint != EQuarztQuantizationReference::BarRelative);

		if (bIsSimpleCase || CurrentTimeStamp.IsZero())
		{
			return FramesUntilBoundary;
		}

		// how many multiples actually exist until the boundary we care about?
		int32 NumDurationsLeft = static_cast<int32>(InQuantizationBoundary.Multiplier) - 1;

		// counting from the current point in time
		if (InQuantizationBoundary.CountingReferencePoint == EQuarztQuantizationReference::CurrentTimeRelative)
		{
			// continue
		}
		// counting from the beginning of the of the current transport
		else if (InQuantizationBoundary.CountingReferencePoint == EQuarztQuantizationReference::TransportRelative)
		{
			// how many of these subdivisions have happened since in the transport lifespan
			int32 CurrentCount = CountNumSubdivisionsSinceStart(InQuantizationBoundary.Quantization);
			 
			// find the remainder
			if (CurrentCount >= InQuantizationBoundary.Multiplier)
			{
				CurrentCount %= static_cast<int32>(InQuantizationBoundary.Multiplier);
			}

			NumDurationsLeft -= CurrentCount;
		}
		// counting from the current bar
		else if (InQuantizationBoundary.CountingReferencePoint == EQuarztQuantizationReference::BarRelative)
		{
			const int32 NumSubdivisionsPerBar = CountNumSubdivisionsPerBar(InQuantizationBoundary.Quantization);
			const int32 NumSubdivisionsAlreadyOccuredInCurrentBar = CountNumSubdivisionsSinceBarStart(InQuantizationBoundary.Quantization);
			NumDurationsLeft = (NumDurationsLeft % NumSubdivisionsPerBar) - NumSubdivisionsAlreadyOccuredInCurrentBar;

			// if NumDurationsLeft is negative, it means the target has already passed this bar.
			// instead we will schedule the sound for the same target in the next bar
			if (NumDurationsLeft < 0)
			{
				NumDurationsLeft += NumSubdivisionsPerBar;
			}
		}

		const float FrationalPortion = FMath::Fractional(InQuantizationBoundary.Multiplier);

		// for Beats, the lengths are not uniform for complex meters
		if ((InQuantizationBoundary.Quantization == EQuartzCommandQuantization::Beat) && PulseDurations.Num())
		{
			// if the metronome hasn't ticked yet, then PulseDurationIndex is -1 (treat as index zero)
			int32 TempPulseDurationIndex = (PulseDurationIndex < 0) ? 0 : PulseDurationIndex;

			for (int i = 0; i < NumDurationsLeft; ++i)
			{
				// need to increment now because FramesUntilBoundary already
				// represents the current (fractional) pulse duration
				if (++TempPulseDurationIndex == PulseDurations.Num())
				{
					TempPulseDurationIndex = 0;
				}

				FramesUntilBoundary += PulseDurations[TempPulseDurationIndex];
			}

			if (++TempPulseDurationIndex == PulseDurations.Num())
			{
				TempPulseDurationIndex = 0;
			}

			FramesUntilBoundary += FrationalPortion * PulseDurations[TempPulseDurationIndex];
		}
		else
		{
			const float Multiplier = NumDurationsLeft + FrationalPortion;
			const float Duration = static_cast<float>(MusicalDurationsInFrames[InQuantizationBoundary.Quantization]);
			FramesUntilBoundary += FMath::RoundToInt(Multiplier * Duration);
		}

		return FramesUntilBoundary;
	}

	float FQuartzMetronome::CountNumSubdivisionsPerBar(EQuartzCommandQuantization InSubdivision) const
	{
		if (InSubdivision == EQuartzCommandQuantization::Beat && PulseDurations.Num())
		{
			return static_cast<float>(PulseDurations.Num());
		}

		int32 LengthOfBar = MusicalDurationsInFrames[EQuartzCommandQuantization::Bar];
		int32 LengthOfOne = MusicalDurationsInFrames[InSubdivision];

		return MusicalDurationsInFrames[EQuartzCommandQuantization::Bar] / static_cast<float>(MusicalDurationsInFrames[InSubdivision]);
	}

	float FQuartzMetronome::CountNumSubdivisionsSinceBarStart(EQuartzCommandQuantization InSubdivision) const
	{
		// for our own counting, we don't say that "one bar has occurred since the start of the bar"
		if (InSubdivision == EQuartzCommandQuantization::Bar)
		{
			return 0.0f;
		}

		// Count starts at 1.0f since all musical subdivisions occur once at beat 0 in a bar
		float Count = 1.f;
		if ((InSubdivision == EQuartzCommandQuantization::Beat) && PulseDurations.Num())
		{
			Count += static_cast<float>(PulseDurationIndex);
		}
		else
		{
			float BarProgress = 1.0f - (FramesLeftInMusicalDuration[EQuartzCommandQuantization::Bar] / static_cast<float>(MusicalDurationsInFrames[EQuartzCommandQuantization::Bar]));
			Count += (BarProgress * CountNumSubdivisionsPerBar(InSubdivision));
		}

		return Count;
	}

	float FQuartzMetronome::CountNumSubdivisionsSinceStart(EQuartzCommandQuantization InSubdivision) const
	{
		int32 NumPerBar = CountNumSubdivisionsPerBar(InSubdivision);
		int32 NumInThisBar = CountNumSubdivisionsSinceBarStart(InSubdivision);

		return (CurrentTimeStamp.Bars - 1) * NumPerBar + NumInThisBar;
	}

	void FQuartzMetronome::SubscribeToTimeDivision(MetronomeCommandQueuePtr InListenerQueue, EQuartzCommandQuantization InQuantizationBoundary)
	{
		MetronomeSubscriptionMatrix[(int32)InQuantizationBoundary].AddUnique(InListenerQueue);
		ListenerFlags |= 1 << (int32)InQuantizationBoundary;
	}

	void FQuartzMetronome::SubscribeToAllTimeDivisions(MetronomeCommandQueuePtr InListenerQueue)
	{
		int32 i = 0;
		for (TArray<MetronomeCommandQueuePtr>& QuantizationBoundarySubscribers : MetronomeSubscriptionMatrix)
		{
			QuantizationBoundarySubscribers.AddUnique(InListenerQueue);
			ListenerFlags |= (1 << i++);
		}
	}

	void FQuartzMetronome::UnsubscribeFromTimeDivision(MetronomeCommandQueuePtr InListenerQueue, EQuartzCommandQuantization InQuantizationBoundary)
	{
		MetronomeSubscriptionMatrix[(int32)InQuantizationBoundary].RemoveSingleSwap(InListenerQueue);

		if (MetronomeSubscriptionMatrix[(int32)InQuantizationBoundary].Num() == 0)
		{
			ListenerFlags &= ~(1 << (int32)InQuantizationBoundary);
		}
	}

	void FQuartzMetronome::UnsubscribeFromAllTimeDivisions(MetronomeCommandQueuePtr InListenerQueue)
	{
		int32 i = 0;
		for (TArray<MetronomeCommandQueuePtr>& QuantizationBoundarySubscribers : MetronomeSubscriptionMatrix)
		{
			QuantizationBoundarySubscribers.RemoveSingleSwap(InListenerQueue);
			
			if (QuantizationBoundarySubscribers.Num() == 0)
			{
				ListenerFlags &= ~(1 << i);
			}

			++i;
		}
	}

	void FQuartzMetronome::ResetTransport()
	{
		CurrentTimeStamp.Reset();

		for (int32& FrameCount : FramesLeftInMusicalDuration.FramesInTimeValueInternal)
		{
			FrameCount = 0;
		}

		PulseDurationIndex = -1;
	}

	void FQuartzMetronome::RecalculateDurations()
	{
		PulseDurations.Reset();

		// get default values for each boundary:
		for (int32 i = 0; i < static_cast<int32>(EQuartzCommandQuantization::Count); ++i)
		{
			MusicalDurationsInFrames[i] = CurrentTickRate.GetFramesPerDuration(static_cast<EQuartzCommandQuantization>(i));
		}

		// determine actual length of a bar
		const int32 BarLength = CurrentTimeSignature.NumBeats * CurrentTickRate.GetFramesPerDuration(CurrentTimeSignature.BeatType);
		MusicalDurationsInFrames[EQuartzCommandQuantization::Bar] = BarLength;

		// default beat value to the denominator of our time signature
		MusicalDurationsInFrames[EQuartzCommandQuantization::Beat] = CurrentTickRate.GetFramesPerDuration(CurrentTimeSignature.BeatType);

		// potentially update the durations of BEAT and BAR
		if (CurrentTimeSignature.OptionalPulseOverride.Num() != 0)
		{
			// determine the length of each beat
			int32 LengthCounter = 0;
			int32 StepLength = 0;

			for (const FQuartzPulseOverrideStep& PulseStep : CurrentTimeSignature.OptionalPulseOverride)
			{
				for (int i = 0; i < PulseStep.NumberOfPulses; ++i)
				{
					StepLength = CurrentTickRate.GetFramesPerDuration(PulseStep.PulseDuration);
					LengthCounter += StepLength;

					PulseDurations.Add(StepLength);
				}
			}

			if (LengthCounter > BarLength)
			{
				UE_LOG(LogAudioQuartz, Warning
					, TEXT("Pulse override array on Time Signature reperesents more than a bar. The provided list will be trunctaed to 1 Bar in length"));

				return;
			}

			// extend the last duration to the length of the bar if needed
			while ((LengthCounter + StepLength) <= BarLength)
			{
				PulseDurations.Add(StepLength);
				LengthCounter += StepLength;
			}

			// check to see if all our pulses are the same length
			const int32 FirstValue = PulseDurations[0];
			bool bBeatDurationsAreConstant = true;

			for (const int32& Values : PulseDurations)
			{
				if (Values != FirstValue)
				{
					bBeatDurationsAreConstant = false;
					break;
				}
			}

			// we can just overwrite the duration array with the single value
			if (bBeatDurationsAreConstant)
			{
				MusicalDurationsInFrames[EQuartzCommandQuantization::Beat] = FirstValue;
				PulseDurations.Reset();
			}
		}
	}

	void FQuartzMetronome::FireEvents(int32 EventFlags)
	{
		FQuartzMetronomeDelegateData Data;
		Data.Bar = (CurrentTimeStamp.Bars);
		Data.Beat = (CurrentTimeStamp.Beat + 1);

		if (PulseDurations.Num())
		{
			Data.BeatFraction = 1.f - (FramesLeftInMusicalDuration[EQuartzCommandQuantization::Beat] / static_cast<float>(PulseDurations[PulseDurationIndex]));
		}
		else
		{
			Data.BeatFraction = 1.f - (FramesLeftInMusicalDuration[EQuartzCommandQuantization::Beat] / static_cast<float>(MusicalDurationsInFrames[EQuartzCommandQuantization::Beat]));
		}

		if (!(EventFlags &= ListenerFlags))
		{
			// no events occurred that we have listeners for
			return;
		}

		// loop through quantization boundaries
		int32 i = -1;
		for (TArray<MetronomeCommandQueuePtr>& QuantizationBoundarySubscribers : MetronomeSubscriptionMatrix)
		{
			if (EventFlags & (1 << ++i))
			{
				Data.Quantization = static_cast<EQuartzCommandQuantization>(i);

				// loop through subscribers to that boundary
				for (MetronomeCommandQueuePtr& Subscriber : QuantizationBoundarySubscribers)
				{
					Subscriber->PushEvent(Data);
				}
			}
		}
	}

} // namespace Audio