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


public class FDatasmithFacadeMaterialExpressionTextureCoordinate : FDatasmithFacadeMaterialExpression {
  private global::System.Runtime.InteropServices.HandleRef swigCPtr;

  internal FDatasmithFacadeMaterialExpressionTextureCoordinate(global::System.IntPtr cPtr, bool cMemoryOwn) : base(DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeMaterialExpressionTextureCoordinate_SWIGUpcast(cPtr), cMemoryOwn) {
    swigCPtr = new global::System.Runtime.InteropServices.HandleRef(this, cPtr);
  }

  internal static global::System.Runtime.InteropServices.HandleRef getCPtr(FDatasmithFacadeMaterialExpressionTextureCoordinate obj) {
    return (obj == null) ? new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero) : obj.swigCPtr;
  }

  protected override void Dispose(bool disposing) {
    lock(this) {
      if (swigCPtr.Handle != global::System.IntPtr.Zero) {
        if (swigCMemOwn) {
          swigCMemOwn = false;
          DatasmithFacadeCSharpPINVOKE.delete_FDatasmithFacadeMaterialExpressionTextureCoordinate(swigCPtr);
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
      base.Dispose(disposing);
    }
  }

  public int GetCoordinateIndex() {
    int ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeMaterialExpressionTextureCoordinate_GetCoordinateIndex(swigCPtr);
    return ret;
  }

  public void SetCoordinateIndex(int InCoordinateIndex) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeMaterialExpressionTextureCoordinate_SetCoordinateIndex(swigCPtr, InCoordinateIndex);
  }

  public float GetUTiling() {
    float ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeMaterialExpressionTextureCoordinate_GetUTiling(swigCPtr);
    return ret;
  }

  public void SetUTiling(float InUTiling) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeMaterialExpressionTextureCoordinate_SetUTiling(swigCPtr, InUTiling);
  }

  public float GetVTiling() {
    float ret = DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeMaterialExpressionTextureCoordinate_GetVTiling(swigCPtr);
    return ret;
  }

  public void SetVTiling(float InVTiling) {
    DatasmithFacadeCSharpPINVOKE.FDatasmithFacadeMaterialExpressionTextureCoordinate_SetVTiling(swigCPtr, InVTiling);
  }

}
