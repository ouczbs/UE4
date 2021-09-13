// Copyright Epic Games, Inc. All Rights Reserved.

//------------------------------------------------------------------------------
// <auto-generated />
//
// This file was automatically generated by SWIG (http://www.swig.org).
// Version 4.0.1
//
// Do not make changes to this file unless you know what you are doing--modify
// the SWIG interface file instead.
//------------------------------------------------------------------------------


public class FDatasmithFacadeActorLight : FDatasmithFacadeActor {
  private global::System.Runtime.InteropServices.HandleRef swigCPtr;

  internal FDatasmithFacadeActorLight(global::System.IntPtr cPtr, bool cMemoryOwn) : base(DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SWIGUpcast(cPtr), cMemoryOwn) {
    swigCPtr = new global::System.Runtime.InteropServices.HandleRef(this, cPtr);
  }

  internal static global::System.Runtime.InteropServices.HandleRef getCPtr(FDatasmithFacadeActorLight obj) {
    return (obj == null) ? new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero) : obj.swigCPtr;
  }

  protected override void Dispose(bool disposing) {
    lock(this) {
      if (swigCPtr.Handle != global::System.IntPtr.Zero) {
        if (swigCMemOwn) {
          swigCMemOwn = false;
          DatasmithFacadeCSharpPINVOKE.delete_FDatasmithFacadeActorLight(swigCPtr);
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
      base.Dispose(disposing);
    }
  }

  public bool IsEnabled() {
    bool ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_IsEnabled(swigCPtr);
    return ret;
  }

  public void SetEnabled(bool bIsEnabled) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetEnabled(swigCPtr, bIsEnabled);
  }

  public double GetIntensity() {
    double ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetIntensity(swigCPtr);
    return ret;
  }

  public void SetIntensity(double Intensity) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetIntensity(swigCPtr, Intensity);
  }

  public void GetColor(out byte OutR, out byte OutG, out byte OutB, out byte OutA) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetColor__SWIG_0(swigCPtr, out OutR, out OutG, out OutB, out OutA);
  }

  public void GetColor(out float OutR, out float OutG, out float OutB, out float OutA) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetColor__SWIG_1(swigCPtr, out OutR, out OutG, out OutB, out OutA);
  }

  public void SetColor(byte InR, byte InG, byte InB, byte InA) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetColor__SWIG_0(swigCPtr, InR, InG, InB, InA);
  }

  public void SetColor(float InR, float InG, float InB, float InA) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetColor__SWIG_1(swigCPtr, InR, InG, InB, InA);
  }

  public double GetTemperature() {
    double ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetTemperature(swigCPtr);
    return ret;
  }

  public void SetTemperature(double Temperature) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetTemperature(swigCPtr, Temperature);
  }

  public bool GetUseTemperature() {
    bool ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetUseTemperature(swigCPtr);
    return ret;
  }

  public void SetUseTemperature(bool bUseTemperature) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetUseTemperature(swigCPtr, bUseTemperature);
  }

  public string GetIesFile() {
    string ret = global::System.Runtime.InteropServices.Marshal.PtrToStringUni(DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetIesFile(swigCPtr));
    return ret;
  }

  public void WriteIESFile(string InIESFileFolder, string InIESFileName, string InIESData) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_WriteIESFile(swigCPtr, InIESFileFolder, InIESFileName, InIESData);
  }

  public void SetIesFile(string IesFile) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetIesFile(swigCPtr, IesFile);
  }

  public bool GetUseIes() {
    bool ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetUseIes(swigCPtr);
    return ret;
  }

  public void SetUseIes(bool bUseIes) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetUseIes(swigCPtr, bUseIes);
  }

  public double GetIesBrightnessScale() {
    double ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetIesBrightnessScale(swigCPtr);
    return ret;
  }

  public void SetIesBrightnessScale(double IesBrightnessScale) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetIesBrightnessScale(swigCPtr, IesBrightnessScale);
  }

  public bool GetUseIesBrightness() {
    bool ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetUseIesBrightness(swigCPtr);
    return ret;
  }

  public void SetUseIesBrightness(bool bUseIesBrightness) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetUseIesBrightness(swigCPtr, bUseIesBrightness);
  }

  public void GetIesRotation(out float OutX, out float OutY, out float OutZ, out float OutW) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetIesRotation__SWIG_0(swigCPtr, out OutX, out OutY, out OutZ, out OutW);
  }

  public void GetIesRotation(out float OutPitch, out float OutYaw, out float OutRoll) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetIesRotation__SWIG_1(swigCPtr, out OutPitch, out OutYaw, out OutRoll);
  }

  public void SetIesRotation(float X, float Y, float Z, float W) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetIesRotation__SWIG_0(swigCPtr, X, Y, Z, W);
  }

  public void SetIesRotation(float Pitch, float Yaw, float Roll) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetIesRotation__SWIG_1(swigCPtr, Pitch, Yaw, Roll);
  }

  public FDatasmithFacadeMaterialID GetLightFunctionMaterial() {
	global::System.IntPtr objectPtr = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_GetLightFunctionMaterial(swigCPtr);
	if(objectPtr == global::System.IntPtr.Zero)
	{
		return null;
	}
	else
	{
		return new FDatasmithFacadeMaterialID(objectPtr, true);
	}
}

  public void SetLightFunctionMaterial(FDatasmithFacadeMaterialID InMaterial) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetLightFunctionMaterial__SWIG_0(swigCPtr, FDatasmithFacadeMaterialID.getCPtr(InMaterial));
    if (DatasmithFacadeCSharpPINVOKE.SWIGPendingException.Pending) throw DatasmithFacadeCSharpPINVOKE.SWIGPendingException.Retrieve();
  }

  public void SetLightFunctionMaterial(string InMaterialName) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeActorLight_SetLightFunctionMaterial__SWIG_1(swigCPtr, InMaterialName);
  }

}
