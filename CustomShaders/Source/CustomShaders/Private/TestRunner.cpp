#include "TestRunner.h"
#include "Engine/World.h"
#include "SceneInterface.h"
#include "RayGenTest.h"
#include "../Private/ScenePrivate.h"

ATestRunner::ATestRunner()
{
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void ATestRunner::BeginPlay()
{
	Super::BeginPlay();
	mTest = FRayGenTest();
	Initialized = false;

	if (mRenderTarget != nullptr)
		UpdateTestParameters();
}

void ATestRunner::UpdateTestParameters()
{
	FRayGenTestParameters parameters;
	parameters.Scene = &GetWorld()->Scene->GetRenderScene()->RayTracingScene;
	parameters.CachedRenderTargetSize = FIntPoint(mRenderTarget->SizeX, mRenderTarget->SizeY);
	parameters.RenderTarget = mRenderTarget;
	mTest.UpdateParameters(parameters);
}

// Called every frame
void ATestRunner::Tick(float DeltaTime)
{
	TranscurredTime+=DeltaTime;
	Super::Tick(DeltaTime);

	// we want a slight delay before we start, otherwise some resources such as the accelerated 
	// structure will not be ready
	if(mRenderTarget != nullptr && TranscurredTime>3.0f)
	{
		UpdateTestParameters();

		if(!Initialized)
		{
			mTest.BeginRendering();
			Initialized = true;
		}
	}
}
void ATestRunner::BeginDestroy()
{
	Super::BeginDestroy();
	mTest.EndRendering();
}