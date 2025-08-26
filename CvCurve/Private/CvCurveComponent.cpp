#include "CvCurveComponent.h"


// Sets default values for this component's properties
UCvCurveComponent::UCvCurveComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UCvCurveComponent::BeginPlay()
{
	Super::BeginPlay();

	
}


// Called every frame
void UCvCurveComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}


void UCvCurveComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

}

void UCvCurveComponent::OnRegister()
{
	Super::OnRegister();



	const int32 NumPoints = GetNumberOfSplinePoints();
	for (int32 i = 0; i < NumPoints; ++i)
	{
		SetSplinePointType(i, ESplinePointType::Linear, true);
	}

    UpdateCurveDataFromSpline();
    BuildArcLengthTable(1000);
    
#if WITH_EDITORONLY_DATA
    EditorUnselectedSplineSegmentColor = FLinearColor::Yellow;
    UpdateNurbsVisualization_DebugDraw();
#endif

}


void UCvCurveComponent::UpdateCurveDataFromSpline()
{
    const int32 NumPoints = GetNumberOfSplinePoints();

    if (NumPoints < 4)
    {
        CVPoints.Empty();
        Weights.Empty();
        KnotVector.Empty();
        return;
    }

    CVPoints.SetNum(NumPoints);
    for (int32 i = 0; i < NumPoints; ++i)
    {
        CVPoints[i] = GetLocationAtSplinePoint(i, ESplineCoordinateSpace::World);
    }

    Weights.Init(1.0f, CVPoints.Num());
    GenerateDefaultKnotVector();

}

void UCvCurveComponent::GenerateDefaultKnotVector()
{
    const int32 NumCV = CVPoints.Num();
    const int32 n = NumCV - 1;
    const int32 m = n + Degree + 1;

    KnotVector.Empty();

    for (int32 i = 0; i <= m; ++i)
    {
        if (i < Degree)
        {
            KnotVector.Add(0.0f);
        }
        else if (i > n)
        {
            KnotVector.Add(1.0f);
        }
        else
        {
            float Value = static_cast<float>(i - Degree) / (n - Degree + 1);
            KnotVector.Add(Value);
        }
    }
}

float UCvCurveComponent::BasisFunction(int i, int p, float u) const
{
    const TArray<float>& U = KnotVector;

    if (p == 0)
    {
        bool bIsLastSpan = (i + 1 == U.Num() - 1);
        bool bInSpan = (u >= U[i] && u < U[i + 1]);
        if (bInSpan || (bIsLastSpan && FMath::IsNearlyEqual(u, U.Last())))
        {
            return 1.0f;
        }
        return 0.0f;
    }

    float Denom1 = U[i + p] - U[i];
    float Denom2 = U[i + p + 1] - U[i + 1];

    float Term1 = 0.0f;
    float Term2 = 0.0f;

    if (Denom1 > KINDA_SMALL_NUMBER)
    {
        Term1 = ((u - U[i]) / Denom1) * BasisFunction(i, p - 1, u);
    }

    if (Denom2 > KINDA_SMALL_NUMBER)
    {
        Term2 = ((U[i + p + 1] - u) / Denom2) * BasisFunction(i + 1, p - 1, u);
    }

    return Term1 + Term2;
}

FVector UCvCurveComponent::EvaluateAt(float u) const
{
    const int32 NumCV = CVPoints.Num();
    const int32 n = NumCV - 1;
    const int32 m = n + Degree + 1;

    if (NumCV < 4 || KnotVector.Num() < m + 1)
    {
        UE_LOG(LogTemp, Error, TEXT("EvaluateAt: Invalid NURBS configuration"));
        return FVector::ZeroVector;
    }

    FVector Numerator = FVector::ZeroVector;
    float Denominator = 0.0f;

    for (int32 i = 0; i <= n; ++i)
    {
        float N = BasisFunction(i, Degree, u);
        float W = Weights[i];

        Numerator += N * W * CVPoints[i];
        Denominator += N * W;
    }

    if (Denominator < KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogTemp, Warning, TEXT("EvaluateAt: Denominator too small at u=%f"), u);
        return FVector::ZeroVector;
    }

    return Numerator / Denominator;
}


void UCvCurveComponent::BuildArcLengthTable(int32 NumSamples)
{
    ArcLengthTable.Empty();

    if (CVPoints.Num() < 4 || KnotVector.Num() == 0) return;

    float u_min = KnotVector[Degree];
    float u_max = KnotVector[KnotVector.Num() - Degree - 1];
    float AdjustedUMax = u_max - KINDA_SMALL_NUMBER;

    // Step 1: Raw sample（高精度）
    struct FSamplePoint
    {
        float U;
        float Distance;
    };

    TArray<FSamplePoint> RawSamples;
    RawSamples.Reserve(NumSamples + 1);

    FVector Prev = EvaluateAt(u_min);
    float Total = 0.0f;

    RawSamples.Add({ u_min, 0.0f });

    for (int32 i = 1; i <= NumSamples; ++i)
    {
        float Alpha = i / static_cast<float>(NumSamples);
        float u = FMath::Lerp(u_min, AdjustedUMax, Alpha);
        FVector Curr = EvaluateAt(u);
        Total += FVector::Dist(Prev, Curr);

        RawSamples.Add({ u, Total });
        Prev = Curr;
    }

    CurveTotalLength = Total;

    // Step 2: 等距離サンプリング（N=100なら0%,1%,2%...）
    int32 EquiDistSamples = 100;
    ArcLengthTable.Reserve(EquiDistSamples + 1);

    for (int32 i = 0; i <= EquiDistSamples; ++i)
    {
        float TargetDist = Total * (i / static_cast<float>(EquiDistSamples));

        // RawSamplesから距離に近い2点を探す
        for (int32 j = 1; j < RawSamples.Num(); ++j)
        {
            if (RawSamples[j].Distance >= TargetDist)
            {
                const FSamplePoint& A = RawSamples[j - 1];
                const FSamplePoint& B = RawSamples[j];

                float Alpha = (TargetDist - A.Distance) / (B.Distance - A.Distance);
                float InterpolatedU = FMath::Lerp(A.U, B.U, Alpha);

                ArcLengthTable.Add({ InterpolatedU, TargetDist });
                break;
            }
        }
    }
}

float UCvCurveComponent::FindUByDistance(float Distance) const
{
    if (ArcLengthTable.Num() < 2) return 0.0f;

    for (int32 i = 1; i < ArcLengthTable.Num(); ++i)
    {
        float D0 = ArcLengthTable[i - 1].Distance;
        float D1 = ArcLengthTable[i].Distance;

        if (Distance >= D0 && Distance <= D1)
        {
            float Alpha = (Distance - D0) / (D1 - D0);
            return FMath::Lerp(ArcLengthTable[i - 1].U, ArcLengthTable[i].U, Alpha);
        }
    }

    return ArcLengthTable.Last().U;
}


FTransform UCvCurveComponent::GetTransformAtDistance(float Distance) const
{
    if (CurveTotalLength <= 0.0f || ArcLengthTable.Num() < 2)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetTransformAtDistance: Arc length table is not ready"));
        return FTransform::Identity;
    }

    Distance = FMath::Clamp(Distance, 0.0f, CurveTotalLength);
    float u = FindUByDistance(Distance);

    u = FMath::Clamp(u, KnotVector[Degree], KnotVector.Last() - 0.0001f);

    const float Epsilon = 0.0005f;
    float uBack = FMath::Clamp(u - Epsilon, KnotVector[Degree], KnotVector.Last());

    FVector PosA = EvaluateAt(uBack);
    FVector PosB = EvaluateAt(u);
    FVector Tangent = (PosB - PosA).GetSafeNormal();

    if (Tangent.IsNearlyZero())
    {
        Tangent = FVector::ForwardVector;
    }

    FRotator Rotation = Tangent.Rotation();
    return FTransform(Rotation, PosB);
}

float UCvCurveComponent::GetCurveLength() const
{
    return CurveTotalLength;
}

void UCvCurveComponent::UpdateNurbsVisualization_DebugDraw()
{

    UWorld* World = GetWorld();

    if (!World || CVPoints.Num() < 4 || KnotVector.Num() == 0) return;

    FlushPersistentDebugLines(World);

    const int32 NumSegments = 1000;
    const float SegmentLength = CurveTotalLength / NumSegments;

    FVector PrevPos = EvaluateAt(FindUByDistance(0.0f));

    for (int32 i = 1; i <= NumSegments; ++i)
    {
        float Distance = i * SegmentLength;
        float u = FindUByDistance(Distance);
        FVector CurrPos = EvaluateAt(u);

        if (!World->IsGameWorld())
        {
            DrawDebugLine(
                World,
                PrevPos,
                CurrPos,
                FColor::White,
                true,      // persistent (until new construction)
                0.0f,
                0,
                2.0f
            );
        }
        PrevPos = CurrPos;
    }
}