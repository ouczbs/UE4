// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderCore.h"
#include "ShaderCompilerCore.h"
#include "CrossCompilerDefinitions.h"

// Cross compiler support/common functionality
namespace CrossCompiler
{

	/** Wrapper structure to pass options descriptor to ShaderConductor. This is mapped to <struct ShaderConductor::Compiler::Options>. */
	struct SHADERCOMPILERCOMMON_API FShaderConductorOptions
	{
		/** Removes unused global variables and resources. This can only be used in the HLSL rewrite pass, i.e. 'RewriteHlslSource'. */
		bool bRemoveUnusedGlobals = false;

		/** Experimental: Decide how a matrix get packed. Default in HLSL is row-major. This will be inverted in the SPIR-V backend to match SPIR-V's column-major default. */
		bool bPackMatricesInRowMajor = true;

		/** Enable 16-bit types, such as half, uint16_t. Requires shader model 6.2+. */
		bool bEnable16bitTypes = false;

		/** Embed debug info into the binary. */
		bool bEnableDebugInfo = false;

		/** Force to turn off optimizations. Ignore optimizationLevel below. */
		bool bDisableOptimizations = false;

		/** Enable a pass that converts floating point MUL+ADD pairs into FMAs to avoid re-association. */
		bool bEnableFMAPass = false;

		/** Target shader profile. By default HCT_FeatureLevelSM5. */
		EHlslCompileTarget TargetProfile = HCT_FeatureLevelSM5;
	};

	/** Target high level languages for ShaderConductor output. */
	enum class EShaderConductorLanguage
	{
		Glsl,
		Essl,
		Metal_macOS,
		Metal_iOS,
	};

	/** Shader conductor output target descriptor. */
	struct SHADERCOMPILERCOMMON_API FShaderConductorTarget
	{
		/** Target shader semantics, e.g. "macOS" or "iOS" for Metal GPU semantics. */
		EShaderConductorLanguage Language = EShaderConductorLanguage::Glsl;

		/**
		Target shader version.
		Valid values for Metal family: 20300, 20200, 20100, 20000, 10200, 10100, 10000.
		Valid values for GLSL family: 310, 320, 330, 430.
		*/
		int32 Version = 0;

		/** Cross compilation flags. This is used for high-level cross compilation (such as Metal output) that is send over to SPIRV-Cross, e.g. { "invariant_float_math", "1" }. */
		FShaderCompilerDefinitions CompileFlags;

		/** Optional callback to rename certain variable types. */
		TFunction<bool(const FAnsiStringView& VariableName, const FAnsiStringView& TypeName, FString& OutRenamedTypeName)> VariableTypeRenameCallback;
	};

	/** Wrapper class to handle interface between UE and ShaderConductor. Use to compile HLSL shaders to SPIR-V or high-level languages such as Metal. */
	class SHADERCOMPILERCOMMON_API FShaderConductorContext
	{
	public:
		/** Initializes the context with internal buffers used for the conversion of input and option descriptors between UE and ShaderConductor. */
		FShaderConductorContext();

		/** Release the internal buffers. */
		~FShaderConductorContext();

		/** Move constructor to take ownership of internal buffers from 'Rhs'. */
		FShaderConductorContext(FShaderConductorContext&& Rhs);

		/** Move operator to take ownership of internal buffers from 'Rhs'. */
		FShaderConductorContext& operator = (FShaderConductorContext&& Rhs);

		FShaderConductorContext(const FShaderConductorContext&) = delete;
		FShaderConductorContext& operator = (const FShaderConductorContext&) = delete;

		/** Loads the shader source and converts the input descriptor to a format suitable for ShaderConductor. If 'Definitions' is null, the previously loaded definitions are not modified. */
		bool LoadSource(const FString& ShaderSource, const FString& Filename, const FString& EntryPoint, EHlslShaderFrequency ShaderStage, const FShaderCompilerDefinitions* Definitions = nullptr);
		bool LoadSource(const ANSICHAR* ShaderSource, const ANSICHAR* Filename, const ANSICHAR* EntryPoint, EHlslShaderFrequency ShaderStage, const FShaderCompilerDefinitions* Definitions = nullptr);

		/** Rewrites the specified HLSL shader source code. This allows to reduce the HLSL code by removing unused global resources for instance.
		This will update the internally loaded source (see 'LoadSource'), so the output parameter 'OutSource' is optional. */
		bool RewriteHlsl(const FShaderConductorOptions& Options, FString* OutSource = nullptr);

		/** Compiles the specified HLSL shader source code to SPIR-V. */
		bool CompileHlslToSpirv(const FShaderConductorOptions& Options, TArray<uint32>& OutSpirv);

		/** Compiles the specified SPIR-V shader binary code to high level source code (Metal or GLSL). */
		bool CompileSpirvToSource(const FShaderConductorOptions& Options, const FShaderConductorTarget& Target, const void* InSpirv, uint32 InSpirvByteSize, FString& OutSource);

		/** Compiles the specified SPIR-V shader binary code to high level source code (Metal or GLSL) stored as null terminated ANSI string. */
		bool CompileSpirvToSourceAnsi(const FShaderConductorOptions& Options, const FShaderConductorTarget& Target, const void* InSpirv, uint32 InSpirvByteSize, TArray<ANSICHAR>& OutSource);

		/** Compiles the specified SPIR-V shader binary code to high level source code (Metal or GLSL) stored as byte buffer (without null terminator as it comes from ShaderConductor). */
		bool CompileSpirvToSourceBuffer(const FShaderConductorOptions& Options, const FShaderConductorTarget& Target, const void* InSpirv, uint32 InSpirvByteSize, const TFunction<void(const void* Data, uint32 Size)>& OutputCallback);

		/** Flushes the list of current compile errors and moves the ownership to the caller. */
		void FlushErrors(TArray<FShaderCompilerError>& OutErrors);

		/** Returns a pointer to a null terminated ANSI string of the internal loaded sources, or null if no source has been loaded yet. This is automatically updated when RewriteHlsl() is called. */
		const ANSICHAR* GetSourceString() const;

		/** Returns a length of the internal loaded sources (excluding the null terminator). This is automatically updated when RewriteHlsl() is called. */
		int32 GetSourceLength() const;

		/** Returns the list of current compile errors. */
		inline const TArray<FShaderCompilerError>& GetErrors() const
		{
			return Errors;
		}

	public:
		/** Convert array of error string lines into array of <FShaderCompilerError>. */
		static void ConvertCompileErrors(TArray<FString>&& ErrorStringLines, TArray<FShaderCompilerError>& OutErrors);

		/** Returns whether the specified variable name denotes an intermediate output variable.
		This is only true for a special identifiers generated by DXC to communicate patch constant data in the Hull Shader. */
		static bool IsIntermediateSpirvOutputVariable(const ANSICHAR* SpirvVariableName);

	public:
		struct FShaderConductorIntermediates; // Pimpl idiom

	private:
		TArray<FShaderCompilerError> Errors;
		FShaderConductorIntermediates* Intermediates; // Pimpl idiom
	};

}

// Error code for SCW to help track down crashes
extern SHADERCOMPILERCOMMON_API ESCWErrorCode GSCWErrorCode;
