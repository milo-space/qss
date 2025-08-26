#pragma once

#include "CoreMinimal.h"
#include "Components/SplineComponent.h"
#include "CvCurveComponent.generated.h"

USTRUCT()
struct FArcLengthSample
{
    GENERATED_BODY()

    float U;
    float Distance;
};

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), DisplayName = "CV Curve")
class CVCURVE_API UCvCurveComponent : public USplineComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCvCurveComponent();


    UFUNCTION(BlueprintCallable, Category = "CV Curve")
    FTransform GetTransformAtDistance(float Distance) const;

    UFUNCTION(BlueprintCallable, Category = "CV Curve")
    float GetCurveLength() const;

public:
    // Called every frame
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void OnComponentCreated() override;
	virtual void OnRegister() override;

    TArray<FVector> CVPoints;

    TArray<float> Weights;

    TArray<float> KnotVector;

    int32 Degree = 3;

    TArray<FArcLengthSample> ArcLengthTable;

    float CurveTotalLength = 0.0f;

    void UpdateCurveDataFromSpline();

    FVector EvaluateAt(float u) const;

    void GenerateDefaultKnotVector();

    float BasisFunction(int i, int p, float u) const;

    void UpdateNurbsVisualization_DebugDraw();

    void BuildArcLengthTable(int32 NumSamples);

    float FindUByDistance(float Distance) const;
	
};
