#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RayGenTest.h"
#include "TestRunner.generated.h"

UCLASS()
class CUSTOMSHADERS_API ATestRunner : public AActor
{
	GENERATED_BODY()
	
public:	
	ATestRunner();
	FRayGenTest mTest;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ShaderDemo)
	class UTextureRenderTarget2D* mRenderTarget = nullptr;
	
protected:
	virtual void BeginPlay() override;
	void UpdateTestParameters();

	float TranscurredTime; ///< allows us to add a delay on BeginPlay() 
	bool Initialized;

public:	
	virtual void Tick(float DeltaTime) override;
	virtual void BeginDestroy() override;
};
