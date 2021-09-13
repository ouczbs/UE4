// Copyright Epic Games, Inc. All Rights Reserved.

// This is the Swig interface file for the Datasmith C# facade.

%module DatasmithFacadeCSharp

%begin %{
// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithFacadeCSharp.h"
%}

%header %{
#include "DatasmithFacadeElement.h"
#include "DatasmithFacadeKeyValueProperty.h"
#include "DatasmithFacadeTexture.h"
#include "DatasmithFacadeMaterial.h"
#include "DatasmithFacadeMaterialID.h"
#include "DatasmithFacadeUEPbrMaterial.h"
#include "DatasmithFacadeMaterialsUtils.h"
#include "DatasmithFacadeMesh.h"
#include "DatasmithFacadeMetaData.h"
#include "DatasmithFacadeActor.h"
#include "DatasmithFacadeDirectLink.h"
#include "DatasmithFacadeActorCamera.h"
#include "DatasmithFacadeActorLight.h"
#include "DatasmithFacadeActorMesh.h"
#include "DatasmithFacadeScene.h"
#include "DatasmithFacadeLog.h"
#include "DatasmithFacadeUtils.h"

#include "IDatasmithExporterUIModule.h"
#include "IDirectLinkUI.h"
%}

%include <arrays_csharp.i>
%include <wchar.i>
%include <swiginterface.i>
%include <typemaps.i>

typedef wchar_t TCHAR;
typedef signed int int32;
typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned long long size_t;

%apply float INPUT[] { const float* InWorldMatrix }
%apply float INPUT[] { const float* InLocalMatrix }

%typemap(cscode) FDatasmithFacadeElement %{
  public static string GetStringHash(string Input)
  {
    System.Text.StringBuilder sb = new System.Text.StringBuilder(32);
    InternalGetStringHash(Input, sb, (ulong)sb.Capacity + 1);
    return sb.ToString();
  }
%}

%typemap(cscode) FDatasmithFacadeTexture %{
  public string GetFileHash()
  {
    System.Text.StringBuilder sb = new System.Text.StringBuilder(32);
    InternalGetFileHash(sb, (ulong)sb.Capacity + 1);
    return sb.ToString();
  }
%}

// Rename functions returning object owned by C# to their corresponding interface name.
%rename (GetChild) GetNewChild;
%rename (GetParentActor) GetNewParentActor;
%rename (GetTexture) GetNewTexture;
%rename (GetActor) GetNewActor;
%rename (GetMaterial) GetNewMaterial;
%rename (GetMetaData) GetNewMetaData;
%rename (GetLightFunctionMaterial) GetNewLightFunctionMaterial;
%rename (GetMaterialOverride) GetNewMaterialOverride;
%rename (GetExpression) GetNewFacadeExpression;
%rename (CreateWeightedMaterialExpression) CreateNewFacadeWeightedMaterialExpression;
%rename (CreateTextureExpression) CreateNewFacadeTextureExpression;
%rename (GetInput) GetNewFacadeInput;
%rename (GetLightFunctionMaterial) GetNewLightFunctionMaterial;
%rename (GetProperty) GetNewProperty;
%rename (GetPropertyByName) GetNewPropertyByName;
%rename (GetLOD) GetNewLOD;
%rename (GetMesh) GetNewMesh;

// Make sure the returned pointer is managed by the C# interface and that the C# type is "downcastable"
%define OWNED_RETURNED_MATERIAL_EXPRESSION(FUNCTIONNAME)
%typemap(csout, excode=SWIGEXCODE) FDatasmithFacadeMaterialExpression* FUNCTIONNAME {
	global::System.IntPtr objectPtr = $imcall;$excode
	if(objectPtr == global::System.IntPtr.Zero)
	{
		return null;
	}
	else
	{
		//Query the type with a temporary wrapper with no memory ownership.
		EDatasmithFacadeMaterialExpressionType ExpressionType = (new FDatasmithFacadeMaterialExpression(objectPtr, false)).GetExpressionType();

		switch(ExpressionType)
		{
		case EDatasmithFacadeMaterialExpressionType.ConstantBool:
			return new FDatasmithFacadeMaterialExpressionBool(objectPtr, true);
		case EDatasmithFacadeMaterialExpressionType.ConstantColor:
			return new FDatasmithFacadeMaterialExpressionColor(objectPtr, true);
		case EDatasmithFacadeMaterialExpressionType.ConstantScalar:
			return new FDatasmithFacadeMaterialExpressionScalar(objectPtr, true);
		case EDatasmithFacadeMaterialExpressionType.FlattenNormal:
			return new FDatasmithFacadeMaterialExpressionFlattenNormal(objectPtr, true);
		case EDatasmithFacadeMaterialExpressionType.FunctionCall:
			return new FDatasmithFacadeMaterialExpressionFunctionCall(objectPtr, true);
		case EDatasmithFacadeMaterialExpressionType.Generic:
			return new FDatasmithFacadeMaterialExpressionGeneric(objectPtr, true);
		case EDatasmithFacadeMaterialExpressionType.Texture:
			return new FDatasmithFacadeMaterialExpressionTexture(objectPtr, true);
		case EDatasmithFacadeMaterialExpressionType.TextureCoordinate:
			return new FDatasmithFacadeMaterialExpressionTextureCoordinate(objectPtr, true);
		default:
			return null;
		}
	}
}
%enddef

OWNED_RETURNED_MATERIAL_EXPRESSION(GetNewFacadeExpression)
OWNED_RETURNED_MATERIAL_EXPRESSION(CreateNewFacadeWeightedMaterialExpression)
#undef OWNED_RETURNED_MATERIAL_EXPRESSION

// Make sure the returned pointer is managed by the C# interface and that the C# type is "downcastable"
%define OWNED_RETURNED_ACTOR(FUNCTIONNAME)
%typemap(csout, excode=SWIGEXCODE) FDatasmithFacadeActor* FUNCTIONNAME {
	global::System.IntPtr objectPtr = $imcall;$excode
	if(objectPtr == global::System.IntPtr.Zero)
	{
		return null;
	}
	else
	{
		FDatasmithFacadeActor.EActorType ActorType = (new FDatasmithFacadeActor(objectPtr, false)).GetActorType();

		switch(ActorType)
		{
		case FDatasmithFacadeActor.EActorType.DirectionalLight:
			return new FDatasmithFacadeDirectionalLight(objectPtr, true);
		case FDatasmithFacadeActor.EActorType.AreaLight:
			return new FDatasmithFacadeAreaLight(objectPtr, true);
		case FDatasmithFacadeActor.EActorType.LightmassPortal:
			return new FDatasmithFacadeLightmassPortal(objectPtr, true);
		case FDatasmithFacadeActor.EActorType.PointLight:
			return new FDatasmithFacadePointLight(objectPtr, true);
		case FDatasmithFacadeActor.EActorType.SpotLight:
			return new FDatasmithFacadeSpotLight(objectPtr, true);
		case FDatasmithFacadeActor.EActorType.StaticMeshActor:
			return new FDatasmithFacadeActorMesh(objectPtr, true);
		case FDatasmithFacadeActor.EActorType.Camera:
			return new FDatasmithFacadeActorCamera(objectPtr, true);
		case FDatasmithFacadeActor.EActorType.Actor:
			return new FDatasmithFacadeActor(objectPtr, true);
		case FDatasmithFacadeActor.EActorType.Unsupported:
		default:
			return null;
		}
	}
}
%enddef

OWNED_RETURNED_ACTOR(GetNewChild)
OWNED_RETURNED_ACTOR(GetNewParentActor)
OWNED_RETURNED_ACTOR(GetNewActor)
#undef OWNED_RETURNED_ACTOR

// Make sure the returned pointer is managed by the C# interface and that the C# type is "downcastable"
%define OWNED_RETURNED_MATERIAL(FUNCTIONNAME)
%typemap(csout, excode=SWIGEXCODE) FDatasmithFacadeBaseMaterial* FUNCTIONNAME {
	global::System.IntPtr objectPtr = $imcall;$excode
	if(objectPtr == global::System.IntPtr.Zero)
	{
		return null;
	}
	else
	{
		FDatasmithFacadeBaseMaterial.EDatasmithMaterialType MaterialType = (new FDatasmithFacadeBaseMaterial(objectPtr, false)).GetDatasmithMaterialType();

		switch(MaterialType)
		{
		case FDatasmithFacadeBaseMaterial.EDatasmithMaterialType.UEPbrMaterial:
			return new FDatasmithFacadeUEPbrMaterial(objectPtr, true);
		case FDatasmithFacadeBaseMaterial.EDatasmithMaterialType.MasterMaterial:
			return new FDatasmithFacadeMasterMaterial(objectPtr, true);
		case FDatasmithFacadeBaseMaterial.EDatasmithMaterialType.Unsupported:
		default:
			return null;
		}
	}
}
%enddef

OWNED_RETURNED_MATERIAL(GetNewMaterial)
#undef OWNED_RETURNED_MATERIAL

// Make sure we the pointer returned by the given function is managed by the C# interface.
%define OWNED_GENERIC_FACADE_OBJECT(OBJECTTYPE, FUNCTIONNAME)
%typemap(csout, excode=SWIGEXCODE) OBJECTTYPE* FUNCTIONNAME {
	global::System.IntPtr objectPtr = $imcall;$excode
	if(objectPtr == global::System.IntPtr.Zero)
	{
		return null;
	}
	else
	{
		return new OBJECTTYPE(objectPtr, true);
	}
}
%enddef

OWNED_GENERIC_FACADE_OBJECT(FDatasmithFacadeTexture, GetNewTexture)
OWNED_GENERIC_FACADE_OBJECT(FDatasmithFacadeMetaData, GetNewMetaData)
OWNED_GENERIC_FACADE_OBJECT(FDatasmithFacadeMaterialID, GetNewLightFunctionMaterial)
OWNED_GENERIC_FACADE_OBJECT(FDatasmithFacadeMaterialID, GetNewMaterialOverride)
OWNED_GENERIC_FACADE_OBJECT(FDatasmithFacadeMaterialExpressionTexture, CreateNewFacadeTextureExpression)
OWNED_GENERIC_FACADE_OBJECT(FDatasmithFacadeExpressionInput, GetNewFacadeInput)
OWNED_GENERIC_FACADE_OBJECT(FDatasmithFacadeKeyValueProperty, GetNewProperty)
OWNED_GENERIC_FACADE_OBJECT(FDatasmithFacadeKeyValueProperty, GetNewPropertyByName)
OWNED_GENERIC_FACADE_OBJECT(FDatasmithFacadeMesh, GetNewLOD)
OWNED_GENERIC_FACADE_OBJECT(FDatasmithFacadeMeshElement, GetNewMesh)
#undef OWNED_GENERIC_FACADE_OBJECT

// Serialize the uint8* returned by FDatasmithFacadeTexture::GetData() into byte[]
%typemap(csout, excode=SWIGEXCODE) uint8* GetData {
	global::System.IntPtr arrayPtr = $imcall;$excode
	if(arrayPtr == global::System.IntPtr.Zero || OutDataSize == 0)
	{
		return null;
	}
	else
	{
		byte[] Data = new byte[OutDataSize];
		global::System.Runtime.InteropServices.Marshal.Copy(arrayPtr, Data, 0, (int)OutDataSize);
		return Data;
	}
}

%typemap(ctype) float[16] "float *"
%typemap(imtype) float[16] "[global::System.Runtime.InteropServices.In, global::System.Runtime.InteropServices.MarshalAs(global::System.Runtime.InteropServices.UnmanagedType.LPArray)] float[]"
%typemap(cstype) float[16] "float[]"
%typemap(csin) float[16] "$csinput"

// Macro used to return custom enum types as C# "out arguments"
%define OUTPUT_CUSTOMENUM_TYPEMAP(ENUMTYPE, ENUMCSTYPE)
%typemap(ctype) ENUMTYPE *OUTPUT, ENUMTYPE &OUTPUT "int *"
%typemap(imtype) ENUMTYPE *OUTPUT, ENUMTYPE &OUTPUT "out ENUMCSTYPE"
%typemap(cstype) ENUMTYPE *OUTPUT, ENUMTYPE &OUTPUT "out ENUMCSTYPE"
%typemap(csin) ENUMTYPE *OUTPUT, ENUMTYPE &OUTPUT "out $csinput"
%enddef

OUTPUT_CUSTOMENUM_TYPEMAP(FDatasmithFacadeTexture::ETextureFormat, FDatasmithFacadeTexture.ETextureFormat)
#undef OUTPUT_CUSTOMENUM_TYPEMAP

%apply FDatasmithFacadeTexture::ETextureFormat& OUTPUT { FDatasmithFacadeTexture::ETextureFormat& OutFormat }

%apply float &OUTPUT { float& OutR }
%apply float &OUTPUT { float& OutG }
%apply float &OUTPUT { float& OutB }
%apply float &OUTPUT { float& OutA }
%apply uint8 &OUTPUT { uint8& OutR }
%apply uint8 &OUTPUT { uint8& OutG }
%apply uint8 &OUTPUT { uint8& OutB }
%apply uint8 &OUTPUT { uint8& OutA }
%apply float &OUTPUT { float& OutX }
%apply float &OUTPUT { float& OutY }
%apply float &OUTPUT { float& OutZ }
%apply float &OUTPUT { float& OutW }
%apply float &OUTPUT { float& OutU }
%apply float &OUTPUT { float& OutV }
%apply float &OUTPUT { float& OutPitch }
%apply float &OUTPUT { float& OutYaw }
%apply float &OUTPUT { float& OutRoll }
%apply uint32 &OUTPUT { uint32& OutDataSize }
%apply int32 &OUTPUT { int32& OutVertex1 }
%apply int32 &OUTPUT { int32& OutVertex2 }
%apply int32 &OUTPUT { int32& OutVertex3 }
%apply int32 &OUTPUT { int32& OutMaterialId }


// Implement the proper string serialisation for FDatasmtihFacadeElement::GetStringHash() and FDatasmithFacadeTexture::GetFileHash() with StringBuilder and hide the low-level implementation behind a wraper function.
%rename (InternalGetStringHash) GetStringHash;
%csmethodmodifiers GetStringHash "private";
%rename (InternalGetFileHash) GetFileHash;
%csmethodmodifiers GetFileHash "private";
%typemap(imtype) TCHAR OutBuffer[33] "[global::System.Runtime.InteropServices.MarshalAs(global::System.Runtime.InteropServices.UnmanagedType.LPWStr)] System.Text.StringBuilder"
%typemap(cstype) TCHAR OutBuffer[33]  "System.Text.StringBuilder"
%typemap(csin) TCHAR OutBuffer[33]  "$csinput"
%typemap(imtype) size_t BufferSize "ulong"
%typemap(cstype) size_t BufferSize "ulong"
%typemap(csin) size_t BufferSize  "$csinput"

// Use the correct array types for serializing FDatasmtihFacadeTexture::SetData() and FDatasmtihFacadeTexture::GetData()
%typemap(imtype) uint8* InData "byte[]"
%typemap(cstype) uint8* InData "byte[]"
%typemap(csin) uint8* InData "$csinput"
%typemap(cstype) uint8* GetData "byte[]"

// C# does not support multiple inheritance, declaring FDatasmithFacadeExpressionParameter as a C# interface.
%interface_impl(FDatasmithFacadeExpressionParameter);

%include "DatasmithFacadeElement.h"
%include "DatasmithFacadeKeyValueProperty.h"
%include "DatasmithFacadeTexture.h"
%include "DatasmithFacadeMaterial.h"
%include "DatasmithFacadeMaterialID.h"
%include "DatasmithFacadeUEPbrMaterial.h"
%include "DatasmithFacadeMaterialsUtils.h"
%include "DatasmithFacadeMesh.h"
%include "DatasmithFacadeMetaData.h"
%include "DatasmithFacadeActor.h"
%include "DatasmithFacadeActorCamera.h"
%include "DatasmithFacadeActorLight.h"
%include "DatasmithFacadeActorMesh.h"
%include "DatasmithFacadeScene.h"
%include "DatasmithFacadeLog.h"
%include "DatasmithFacadeDirectLink.h"
%include "DatasmithFacadeUtils.h"

// Datasmith UI
%include "IDatasmithExporterUIModule.h"
%include "IDirectLinkUI.h"

// Specialized templates instantiation 
%extend FDatasmithFacadeUEPbrMaterial {
	%template(AddMaterialExpressionBool) AddMaterialExpression<FDatasmithFacadeMaterialExpressionBool>;
	%template(AddMaterialExpressionColor) AddMaterialExpression<FDatasmithFacadeMaterialExpressionColor>;
	%template(AddMaterialExpressionFlattenNormal) AddMaterialExpression<FDatasmithFacadeMaterialExpressionFlattenNormal>;
	%template(AddMaterialExpressionFunctionCall) AddMaterialExpression<FDatasmithFacadeMaterialExpressionFunctionCall>;
	%template(AddMaterialExpressionGeneric) AddMaterialExpression<FDatasmithFacadeMaterialExpressionGeneric>;
	%template(AddMaterialExpressionScalar) AddMaterialExpression<FDatasmithFacadeMaterialExpressionScalar>;
	%template(AddMaterialExpressionTexture) AddMaterialExpression<FDatasmithFacadeMaterialExpressionTexture>;
	%template(AddMaterialExpressionTextureCoordinate) AddMaterialExpression<FDatasmithFacadeMaterialExpressionTextureCoordinate>;
}

%{
#if PLATFORM_MAC
//wchar_t size is platform dependent, we need to make sure the size stays 16-bit, this should cover all our use-cases.
#define wchar_t char16_t
#endif
%}