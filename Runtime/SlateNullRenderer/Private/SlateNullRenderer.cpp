// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateNullRenderer.h"
#include "Rendering/SlateDrawBuffer.h"

FSlateNullRenderer::FSlateNullRenderer(const TSharedRef<FSlateFontServices>& InSlateFontServices, const TSharedRef<FSlateShaderResourceManager>& InResourceManager)
	: FSlateRenderer(InSlateFontServices)
	, ResourceManager(InResourceManager)
{
}

bool FSlateNullRenderer::Initialize()
{
	return true;
}

void FSlateNullRenderer::Destroy()
{
}

FSlateDrawBuffer& FSlateNullRenderer::GetDrawBuffer()
{
	static TUniquePtr<FSlateDrawBuffer> StaticDrawBuffer;
	if (!StaticDrawBuffer.IsValid())
	{
		StaticDrawBuffer = MakeUnique<FSlateDrawBuffer>();
	}

	StaticDrawBuffer->ClearBuffer();
	return *StaticDrawBuffer;
}

void FSlateNullRenderer::CreateViewport( const TSharedRef<SWindow> Window )
{
}

void FSlateNullRenderer::UpdateFullscreenState( const TSharedRef<SWindow> Window, uint32 OverrideResX, uint32 OverrideResY )
{
}

void FSlateNullRenderer::RestoreSystemResolution(const TSharedRef<SWindow> InWindow)
{
}

void FSlateNullRenderer::OnWindowDestroyed( const TSharedRef<SWindow>& InWindow )
{
}

void FSlateNullRenderer::DrawWindows( FSlateDrawBuffer& WindowDrawBuffer )
{
}

FIntPoint FSlateNullRenderer::GenerateDynamicImageResource(const FName InTextureName)
{
	return FIntPoint( 0, 0 );
}

bool FSlateNullRenderer::GenerateDynamicImageResource( FName ResourceName, uint32 Width, uint32 Height, const TArray< uint8 >& Bytes )
{
	return false;
}

FSlateResourceHandle FSlateNullRenderer::GetResourceHandle(const FSlateBrush& Brush, FVector2D LocalSize, float DrawScale)
{
	return ResourceManager.IsValid() ? ResourceManager->GetResourceHandle(Brush) : FSlateResourceHandle();
}

void FSlateNullRenderer::RemoveDynamicBrushResource( TSharedPtr<FSlateDynamicImageBrush> BrushToRemove )
{
}

void FSlateNullRenderer::ReleaseDynamicResource( const FSlateBrush& InBrush )
{
}

void FSlateNullRenderer::PrepareToTakeScreenshot(const FIntRect& Rect, TArray<FColor>* OutColorData, SWindow* InScreenshotWindow)
{
	if (OutColorData)
	{
		int32 TotalSize = Rect.Width() * Rect.Height();
		OutColorData->Empty(TotalSize);
		OutColorData->AddZeroed(TotalSize);
	}
}

FSlateUpdatableTexture* FSlateNullRenderer::CreateUpdatableTexture(uint32 Width, uint32 Height)
{
	return nullptr;
}

FSlateUpdatableTexture* FSlateNullRenderer::CreateSharedHandleTexture(void* SharedHandle)
{
	return nullptr;
}

void FSlateNullRenderer::ReleaseUpdatableTexture(FSlateUpdatableTexture* Texture)
{
}

void FSlateNullRenderer::RequestResize( const TSharedPtr<SWindow>& Window, uint32 NewWidth, uint32 NewHeight )
{
}

FCriticalSection* FSlateNullRenderer::GetResourceCriticalSection()
{
	return &ResourceCriticalSection;
}

int32 FSlateNullRenderer::RegisterCurrentScene(FSceneInterface* Scene) 
{
	// This is a no-op
	return -1;
}

int32 FSlateNullRenderer::GetCurrentSceneIndex() const
{
	// This is a no-op
	return -1;
}

void FSlateNullRenderer::ClearScenes() 
{
	// This is a no-op
}