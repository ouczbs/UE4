// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AssertionMacros.h"
#include "Misc/VarArgs.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/Atomic.h"
#include "Misc/CString.h"
#include "Misc/Crc.h"
#include "Containers/UnrealString.h"
#include "Containers/StringConv.h"
#include "GenericPlatform/GenericPlatformStackWalk.h"
#include "HAL/PlatformStackWalk.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "Misc/Parse.h"
#include "Misc/ScopeLock.h"
#include "Misc/CoreMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/OutputDeviceError.h"
#include "Stats/StatsMisc.h"
#include "Misc/CoreDelegates.h"
#include "HAL/ExceptionHandling.h"
#include "HAL/ThreadHeartBeat.h"

namespace 
{
	// used to track state of assets/ensures
	bool bHasAsserted = false;
	TAtomic<SIZE_T> NumEnsureFailures {0};
	int32 ActiveEnsureCount = 0;

	/** Lock used to synchronize the fail debug calls. */
	static FCriticalSection& GetFailDebugCriticalSection()
	{
		static FCriticalSection FailDebugCriticalSection;
		return FailDebugCriticalSection;
	}	

	struct FTempCommandLineScope
	{
		// The code which is run when an assert or ensure fails (without a
		// debugger attached) calls FCommandLine::Get() *a lot*. If the failed
		// assert is before a command line has been set then the many Get()
		// calls will in turn throw asserts.  It is impractical to chase these
		// and guard against calling Get() and inappropriate in many instances.
		FTempCommandLineScope()
		{
			if (!FCommandLine::IsInitialized())
			{
				FCommandLine::Set(TEXT(""));
				bShouldReset = true;
			}
		}

		~FTempCommandLineScope()
		{
			if (bShouldReset)
			{
				FCommandLine::Reset();
			}
		}

		bool bShouldReset = false;
	};
}

#define FILE_LINE_DESC TEXT(" [File:%s] [Line: %i] ")

/*
	Ensure behavior

	* ensure() macro calls OptionallyLogFormattedEnsureMessageReturningFalse 
	* OptionallyLogFormattedEnsureMessageReturningFalse calls EnsureFailed()
	* EnsureFailed() -
		* Formats the ensure failure and calls StaticFailDebug to populate the global error info (without callstack)
		* Prints the script callstack (if any)
		* Halts if a debugger is attached 
		* If not, logs the callstack and attempts to submit an error report
	* execution continues as normal, (on some platforms this can take ~30 secs to perform)

	Check behavior

	* check() macro calls LogAssertFailedMessage
	* LogAssertFailedMessage formats the assertion message and calls StaticFailDebug
	* StaticFailDebug populates global error info with the failure message and if supported (AllowsCallStackDumpDuringAssert) the callstack
	* If a debugger is attached execution halts
	* If not FDebug::AssertFailed is called
	* FDebug::AssertFailed logs the assert message and description to GError
	* At this point behavior depends on the platform-specific error output device implementation
		* Desktop platforms (Windows, Mac, Linux) will generally throw an exception and in the handler attempt to submit a crash report and exit
		* Console platforms will generally dump the info to the log and abort()

	Fatal-error behavior

	* The UE_LOG macro calls FMsg::Logf which checks for "Fatal" verbosity
	* FMsg::Logf formats the failure message and calls StaticFailDebug
	* StaticFailDebug populates global error info with the failure message and if supported (AllowsCallStackDumpDuringAssert) the callstack
	* FDebug::AssertFailed is then called, and from this point behavior is identical to an assert but with a different message

*/



CORE_API void (*GPrintScriptCallStackFn)() = nullptr;

void PrintScriptCallstack()
{
	if(GPrintScriptCallStackFn)
	{
		GPrintScriptCallStackFn();
	}
}

static void AssertFailedImplV(const FDebug::FFailureInfo& Info, const TCHAR* Format, va_list Args)
{
	FTempCommandLineScope TempCommandLine;

	// This is not perfect because another thread might crash and be handled before this assert
	// but this static variable will report the crash as an assert. Given complexity of a thread
	// aware solution, this should be good enough. If crash reports are obviously wrong we can
	// look into fixing this.
	bHasAsserted = true;

	if (GError)
	{
		GError->SetErrorProgramCounter(Info.ProgramCounter);
	}

	TCHAR DescriptionString[4096];
	FCString::GetVarArgs(DescriptionString, UE_ARRAY_COUNT(DescriptionString), Format, Args);

	TCHAR ErrorString[MAX_SPRINTF];
	FCString::Sprintf(ErrorString, TEXT("%s"), ANSI_TO_TCHAR(Info.Expr));
	if (GError)
	{
		GError->Logf(TEXT("Assertion failed: %s") FILE_LINE_DESC TEXT("\n%s\n"), ErrorString, ANSI_TO_TCHAR(Info.File), Info.Line, DescriptionString);
	}
}

/**
 *	Prints error to the debug output, 
 *	prompts for the remote debugging if there is not debugger, breaks into the debugger 
 *	and copies the error into the global error message.
 */
FORCENOINLINE void StaticFailDebug( const TCHAR* Error, const FDebug::FFailureInfo& Info, const TCHAR* Description, bool bIsEnsure )
{
	const ANSICHAR* File = Info.File;
	int32 Line = Info.Line;

	// Print out the blueprint callstack
	PrintScriptCallstack();

	TCHAR DescriptionAndTrace[4096];

	FCString::Strncpy(DescriptionAndTrace, Description, UE_ARRAY_COUNT(DescriptionAndTrace) - 1);

	// some platforms (Windows, Mac, Linux) generate this themselves by throwing an exception and capturing
	// the backtrace later on
	if (FPlatformProperties::AllowsCallStackDumpDuringAssert() && bIsEnsure == false)
	{
		ANSICHAR StackTrace[4096];
		StackTrace[0] = 0;
		FPlatformStackWalk::StackWalkAndDump(StackTrace, UE_ARRAY_COUNT(StackTrace), Info.ProgramCounter);

		FCString::Strncat(DescriptionAndTrace, TEXT("\n"), UE_ARRAY_COUNT(DescriptionAndTrace) - 1);
		FCString::Strncat(DescriptionAndTrace, ANSI_TO_TCHAR(StackTrace), UE_ARRAY_COUNT(DescriptionAndTrace) - 1);
	}

	FScopeLock Lock( &GetFailDebugCriticalSection());
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s") FILE_LINE_DESC TEXT("\n%s\n"), Error, ANSI_TO_TCHAR(File), Line, Description);

	// Copy the detailed error into the error message.
	TCHAR ErrorMessage[4096];
	if (FCString::Snprintf( ErrorMessage, UE_ARRAY_COUNT( ErrorMessage ), TEXT( "%s" ) FILE_LINE_DESC TEXT( "\n%s\n" ), Error, ANSI_TO_TCHAR( File ), Line, DescriptionAndTrace) < 0)
	{
		// Description and callstack was too long to fit in GErrorMessage. Use only description
		FCString::Snprintf( ErrorMessage, UE_ARRAY_COUNT( ErrorMessage ), TEXT( "%s" ) FILE_LINE_DESC TEXT( "\n%s\n<< callstack too long >>" ), Error, ANSI_TO_TCHAR( File ), Line, Description);
	}

	// Copy the error message to the error history.
	FCString::Strncpy( GErrorHist, ErrorMessage, UE_ARRAY_COUNT( GErrorHist ) );
	FCString::Strncat( GErrorHist, TEXT( "\r\n\r\n" ), UE_ARRAY_COUNT( GErrorHist ) );

	if (GError != nullptr)
	{
		GError->SetErrorProgramCounter(Info.ProgramCounter);
	}
}


/// track thread asserts
bool FDebug::HasAsserted()
{
	return bHasAsserted;
}

// track ensures
bool FDebug::IsEnsuring()
{
	return ActiveEnsureCount > 0;
}
SIZE_T FDebug::GetNumEnsureFailures()
{
	return NumEnsureFailures.Load();
}

void FDebug::LogFormattedMessageWithCallstack(const FName& InLogName, const ANSICHAR* File, int32 Line, const TCHAR* Heading, const TCHAR* Message, ELogVerbosity::Type Verbosity)
{
	FLogCategoryName LogName(InLogName);

	const bool bLowLevel = LogName == NAME_None;
	const bool bWriteUATMarkers = FParse::Param(FCommandLine::Get(), TEXT("CrashForUAT")) && FParse::Param(FCommandLine::Get(), TEXT("stdout")) && !bLowLevel;

	if (bWriteUATMarkers)
	{
		FMsg::Logf(File, Line, LogName, Verbosity, TEXT("begin: stack for UAT"));
	}

	if (bLowLevel)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s\n"), Heading);
	}
	else
	{
		FMsg::Logf(File, Line, LogName, Verbosity, TEXT("%s"), Heading);
		FMsg::Logf(File, Line, LogName, Verbosity, TEXT(""));
	}

	for (const TCHAR* LineStart = Message;; )
	{
		TCHAR SingleLine[1024];

		// Find the end of the current line
		const TCHAR* LineEnd = LineStart;
		TCHAR* SingleLineWritePos = SingleLine;
		int32 SpaceRemaining = UE_ARRAY_COUNT(SingleLine) - 1;

		while (SpaceRemaining > 0 && *LineEnd != 0 && *LineEnd != '\r' && *LineEnd != '\n')
		{
			*SingleLineWritePos++ = *LineEnd++;
			--SpaceRemaining;
		}

		// cap it
		*SingleLineWritePos = 0;

		// prefix function lines with [Callstack] for parsing tools
		const TCHAR* Prefix = (FCString::Strnicmp(LineStart, TEXT("0x"), 2) == 0) ? TEXT("[Callstack] ") : TEXT("");

		// if this is an address line, prefix it with [Callstack]
		if (bLowLevel)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%s%s\n"), Prefix, SingleLine);
		}
		else
		{
			FMsg::Logf(File, Line, LogName, Verbosity, TEXT("%s%s"), Prefix, SingleLine);
		}
		
		// Quit if this was the last line
		if (*LineEnd == 0)
		{
			break;
		}

		// Move to the next line
		LineStart = (LineEnd[0] == '\r' && LineEnd[1] == '\n') ? (LineEnd + 2) : (LineEnd + 1);
	}

	if (bWriteUATMarkers)
	{
		FMsg::Logf(File, Line, LogName, Verbosity, TEXT("end: stack for UAT"));
	}
}

#if DO_CHECK || DO_GUARD_SLOW || DO_ENSURE
//
// Failed assertion handler.
//warning: May be called at library startup time.
//

FORCENOINLINE void FDebug::LogAssertFailedMessageImpl(const FFailureInfo& Info, const TCHAR* Fmt, ...)
{
	va_list Args;
	va_start(Args, Fmt);
	LogAssertFailedMessageImplV(Info, Fmt, Args);
	va_end(Args);
}

void FDebug::LogAssertFailedMessageImplV(const FFailureInfo& Info, const TCHAR* Fmt, va_list Args)
{
	// Ignore this assert if we're already forcibly shutting down because of a critical error.
	if( !GIsCriticalError )
	{
		TCHAR DescriptionString[4096];
		FCString::GetVarArgs( DescriptionString, UE_ARRAY_COUNT(DescriptionString), Fmt, Args );

		TCHAR ErrorString[MAX_SPRINTF];
		FCString::Sprintf( ErrorString, TEXT( "Assertion failed: %s" ), ANSI_TO_TCHAR( Info.Expr ) );

		StaticFailDebug( ErrorString, Info, DescriptionString, false );
	}
}

/**
 * Called when an 'ensure' assertion fails; gathers stack data and generates and error report.
 *
 * @param	Expr	Code expression ANSI string (#code)
 * @param	File	File name ANSI string (__FILE__)
 * @param	Line	Line number (__LINE__)
 * @param	Msg		Informative error message text
 * @param	NumStackFramesToIgnore	Number of stack frames to ignore in the callstack
 */
FORCENOINLINE void FDebug::EnsureFailed(const FFailureInfo& Info, const TCHAR* Msg)
{
	const ANSICHAR* Expr = Info.Expr;
	const ANSICHAR* File = Info.File;
	int32 Line = Info.Line;

	FTempCommandLineScope TempCommandLine;

	// if time isn't ready yet, we better not continue
	if (FPlatformTime::GetSecondsPerCycle() == 0.0)
	{
		return;
	}
	
	++NumEnsureFailures;

#if STATS
	FString EnsureFailedPerfMessage = FString::Printf(TEXT("FDebug::EnsureFailed"));
	SCOPE_LOG_TIME_IN_SECONDS(*EnsureFailedPerfMessage, nullptr)
#endif

	// You can set bShouldCrash to true to cause a regular assertion to trigger (stopping program execution) when an ensure() error occurs
	const bool bShouldCrash = false;		// By default, don't crash on ensure()
	if( bShouldCrash )
	{
		// Just trigger a regular assertion which will crash via GError->Logf()
		FDebug::LogAssertFailedMessageImpl( Info, TEXT("%s"), Msg );
		return;
	}

	// Should we spin here?
	FPlatformAtomics::InterlockedIncrement(&ActiveEnsureCount);

	// Print initial debug message for this error
	TCHAR ErrorString[MAX_SPRINTF];
	FCString::Sprintf(ErrorString,TEXT("Ensure condition failed: %s"),ANSI_TO_TCHAR(Info.Expr));

	StaticFailDebug( ErrorString, Info, Msg, true );

	// Is there a debugger attached?  If not we'll submit an error report.
	if (FPlatformMisc::IsDebuggerPresent() && !GAlwaysReportCrash)
	{
#if !NO_LOGGING
		UE_LOG(LogOutputDevice, Error, TEXT("%s") FILE_LINE_DESC TEXT("\n%s\n"), ErrorString, ANSI_TO_TCHAR(File), Line, Msg);
#endif
	}
	else
	{
		// If we determine that we have not sent a report for this ensure yet, send the report below.
		bool bShouldSendNewReport = false;

		// Create a final string that we'll output to the log (and error history buffer)
		TCHAR ErrorMsg[16384];
		FCString::Snprintf(ErrorMsg, UE_ARRAY_COUNT(ErrorMsg), TEXT("Ensure condition failed: %s [File:%s] [Line: %i]") LINE_TERMINATOR TEXT("%s") LINE_TERMINATOR TEXT("Stack: ") LINE_TERMINATOR, ANSI_TO_TCHAR(Expr), ANSI_TO_TCHAR(File), Line, Msg);

		// No debugger attached, so generate a call stack and submit a crash report
		// Walk the stack and dump it to the allocated memory.
		const SIZE_T StackTraceSize = 65535;
		ANSICHAR* StackTrace = (ANSICHAR*)FMemory::SystemMalloc(StackTraceSize);
		if (StackTrace != NULL)
		{
			// Stop checking heartbeat for this thread (and stop the gamethread hitch detector if we're the game thread).
			// Ensure can take a lot of time (when stackwalking), so we don't want hitches/hangs firing.
			// These are no-ops on threads that didn't already have a heartbeat etc.
			FSlowHeartBeatScope SuspendHeartBeat;
			FDisableHitchDetectorScope SuspendGameThreadHitch;

			{
#if STATS
				FString StackWalkPerfMessage = FString::Printf(TEXT("FPlatformStackWalk::StackWalkAndDump"));
				SCOPE_LOG_TIME_IN_SECONDS(*StackWalkPerfMessage, nullptr)
#endif
				StackTrace[0] = 0;
				FPlatformStackWalk::StackWalkAndDumpEx(StackTrace, StackTraceSize, Info.ProgramCounter, FGenericPlatformStackWalk::EStackWalkFlags::FlagsUsedWhenHandlingEnsure);
			}

			// Also append the stack trace
			FCString::Strncat(ErrorMsg, ANSI_TO_TCHAR(StackTrace), UE_ARRAY_COUNT(ErrorMsg) - 1);
			FMemory::SystemFree(StackTrace);

			// Dump the error and flush the log.
#if !NO_LOGGING
			FDebug::LogFormattedMessageWithCallstack(LogOutputDevice.GetCategoryName(), __FILE__, __LINE__, TEXT("=== Handled ensure: ==="), ErrorMsg, ELogVerbosity::Error);
#endif
			GLog->Flush();

			// Submit the error report to the server! (and display a balloon in the system tray)
			{
				// How many unique previous errors we should keep track of
				const uint32 MaxPreviousErrorsToTrack = 4;
				static uint32 StaticPreviousErrorCount = 0;
				if (StaticPreviousErrorCount < MaxPreviousErrorsToTrack)
				{
					// Check to see if we've already reported this error.  No point in blasting the server with
					// the same error over and over again in a single application session.
					bool bHasErrorAlreadyBeenReported = false;

					// Static: Array of previous unique error message CRCs
					static uint32 StaticPreviousErrorCRCs[MaxPreviousErrorsToTrack];

					// Compute CRC of error string.  Note that along with the call stack, this includes the message
					// string passed to the macro, so only truly redundant errors will go unreported.  Though it also
					// means you shouldn't pass loop counters to ensureMsgf(), otherwise failures may spam the server!
					const uint32 ErrorStrCRC = FCrc::StrCrc_DEPRECATED(ErrorMsg);

					for (uint32 CurErrorIndex = 0; CurErrorIndex < StaticPreviousErrorCount; ++CurErrorIndex)
					{
						if (StaticPreviousErrorCRCs[CurErrorIndex] == ErrorStrCRC)
						{
							// Found it!  This is a redundant error message.
							bHasErrorAlreadyBeenReported = true;
							break;
						}
					}

					// Add the element to the list and bump the count
					StaticPreviousErrorCRCs[StaticPreviousErrorCount++] = ErrorStrCRC;

					if (!bHasErrorAlreadyBeenReported)
					{
#if STATS
						FString SubmitErrorReporterfMessage = FString::Printf(TEXT("SubmitErrorReport"));
						SCOPE_LOG_TIME_IN_SECONDS(*SubmitErrorReporterfMessage, nullptr)
#endif

						FCoreDelegates::OnHandleSystemEnsure.Broadcast();

						FPlatformMisc::SubmitErrorReport(ErrorMsg, EErrorReportMode::Balloon);

						bShouldSendNewReport = true;
					}
				}
			}
		}
		else
		{
			// If we fail to generate the string to identify the crash we don't know if we should skip sending the report,
			// so we will just send the report anyway.
			bShouldSendNewReport = true;

			// Add message to log even without stacktrace. It is useful for testing fail on ensure.
#if !NO_LOGGING
			UE_LOG(LogOutputDevice, Error, TEXT("%s [File:%s] [Line: %i]"), ErrorString, ANSI_TO_TCHAR(File), Line);
#endif
		}

		if (bShouldSendNewReport)
		{
#if STATS
			FString SendNewReportMessage = FString::Printf(TEXT("SendNewReport"));
			SCOPE_LOG_TIME_IN_SECONDS(*SendNewReportMessage, nullptr)
#endif

#if PLATFORM_DESKTOP
			FScopeLock Lock(&GetFailDebugCriticalSection());

			ReportEnsure(ErrorMsg, Info.ProgramCounter);

			GErrorHist[0] = 0;
			GErrorExceptionDescription[0] = 0;
#endif
		}
	}


	FPlatformAtomics::InterlockedDecrement(&ActiveEnsureCount);
}

void FORCENOINLINE FDebug::CheckVerifyFailedImpl(
	const FFailureInfo& Info,
	const TCHAR* Format,
	...)
{
	va_list Args;

	va_start(Args, Format);
	FDebug::LogAssertFailedMessageImplV(Info, Format, Args);
	va_end(Args);

	if (!FPlatformMisc::IsDebuggerPresent())
	{
		FPlatformMisc::PromptForRemoteDebugging(false);

		va_start(Args, Format);
		AssertFailedImplV(Info, Format, Args);
		va_end(Args);
	}
}

#endif // DO_CHECK || DO_GUARD_SLOW || DO_ENSURE

void VARARGS FDebug::AssertFailed(const ANSICHAR* Expr, const ANSICHAR* File, int32 Line, const TCHAR* Format/* = TEXT("")*/, ...)
{
	va_list Args;
	va_start(Args, Format);
	FDebug::FFailureInfo Info = { Expr, File, Line };
	AssertFailedImplV(Info, Format, Args);
	va_end(Args);
}

void FDebug::ProcessFatalError(const FFailureInfo& Info)
{
	// This is not perfect because another thread might crash and be handled before this assert
	// but this static variable will report the crash as an assert. Given complexity of a thread
	// aware solution, this should be good enough. If crash reports are obviously wrong we can
	// look into fixing this.
	bHasAsserted = true;

	GError->SetErrorProgramCounter(Info.ProgramCounter);
	GError->Logf(TEXT("%s"), GErrorHist);
}

#if DO_CHECK || DO_GUARD_SLOW || DO_ENSURE
FORCENOINLINE bool VARARGS FDebug::OptionallyLogFormattedEnsureMessageReturningFalseImpl( bool bLog, const FFailureInfo& Info, const TCHAR* FormattedMsg, ... )
{
	if (bLog)
	{
		const int32 TempStrSize = 4096;
		TCHAR TempStr[ TempStrSize ];
		GET_VARARGS( TempStr, TempStrSize, TempStrSize - 1, FormattedMsg, FormattedMsg );

		EnsureFailed( Info, TempStr );
	}
	
	return false;
}
#endif

FORCENOINLINE void VARARGS LowLevelFatalErrorHandler(const FDebug::FFailureInfo& Info, const TCHAR* Format, ...)
{
	TCHAR DescriptionString[4096];
	GET_VARARGS( DescriptionString, UE_ARRAY_COUNT(DescriptionString), UE_ARRAY_COUNT(DescriptionString)-1, Format, Format );

	StaticFailDebug(TEXT("LowLevelFatalError"), Info, DescriptionString, false);
}

void FDebug::DumpStackTraceToLog(const ELogVerbosity::Type LogVerbosity)
{
	DumpStackTraceToLog(TEXT("=== FDebug::DumpStackTrace(): ==="), LogVerbosity);
}

FORCENOINLINE void FDebug::DumpStackTraceToLog(const TCHAR* Heading, const ELogVerbosity::Type LogVerbosity)
{
#if !NO_LOGGING
	// Walk the stack and dump it to the allocated memory.
	const SIZE_T StackTraceSize = 65535;
	ANSICHAR* StackTrace = (ANSICHAR*)FMemory::SystemMalloc(StackTraceSize);

	{
#if STATS
		FString StackWalkPerfMessage = FString::Printf(TEXT("FPlatformStackWalk::StackWalkAndDump"));
		SCOPE_LOG_TIME_IN_SECONDS(*StackWalkPerfMessage, nullptr)
#endif
		StackTrace[0] = 0;

		const int32 NumStackFramesToIgnore = 1;
		FPlatformStackWalk::StackWalkAndDumpEx(StackTrace, StackTraceSize, NumStackFramesToIgnore, FGenericPlatformStackWalk::EStackWalkFlags::FlagsUsedWhenHandlingEnsure);
	}

	// Dump the error and flush the log.
	// ELogVerbosity::Error to make sure it gets printed in log for conveniency.
	FDebug::LogFormattedMessageWithCallstack(LogOutputDevice.GetCategoryName(), __FILE__, __LINE__, Heading, ANSI_TO_TCHAR(StackTrace), LogVerbosity);
	GLog->Flush();
	FMemory::SystemFree(StackTrace);
#endif
}

#undef FILE_LINE_DESC
