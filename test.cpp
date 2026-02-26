#include "Engine/World.h"
#include "CollisionQueryParams.h"

static bool SweepFocusSlabAndGetForwardDistance(
    const UWorld* World,
    const FVector& CamPos,
    const FQuat& CamRot,                 // カメラ回転
    const FVector& TargetCenterWorld,    // 被写体中心
    float SlabSizeCm,                    // 例: 200.0f (2m四方)
    float SlabThicknessCm,               // 例: 10.0f  (厚み10cm)
    float& OutFocusDistanceCm,
    FHitResult& OutHit)
{
    if (!World)
    {
        return false;
    }

    const FVector Forward = CamRot.GetForwardVector().GetSafeNormal();

    // 「板を飛ばす」方向：カメラ→被写体中心（あなたの要件通り）
    const FVector Dir = (TargetCenterWorld - CamPos).GetSafeNormal();
    if (Dir.IsNearlyZero())
    {
        return false;
    }

    const FVector Start = CamPos;
    const FVector End   = TargetCenterWorld;

    // 2m四方・厚み10cmの板（HalfExtentなので半分）
    // ここでは板の法線をカメラForwardに合わせ、板の厚みをForward方向に持たせます。
    const float HalfXY = SlabSizeCm * 0.5f;       // 100cm
    const float HalfZ  = SlabThicknessCm * 0.5f;  // 5cm
    const FVector HalfExtent(HalfXY, HalfXY, HalfZ);

    const FCollisionShape Shape = FCollisionShape::MakeBox(HalfExtent);

    // 板の向き：カメラと同じ（Z=Forward, X/Yが画面面内、というニュアンス）
    // ※ Boxの軸がどう割り当てられるかは「回転付きBox」として扱われるので、ここではCamRotでOKです。
    const FQuat Orientation = CamRot;

    FCollisionObjectQueryParams ObjParams;
    ObjParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FocusSlabSweep), /*bTraceComplex=*/false);

    const bool bHit = World->SweepSingleByObjectType(
        OutHit,
        Start,
        End,
        Orientation,
        ObjParams,
        Shape,
        QueryParams
    );

    if (!bHit)
    {
        return false;
    }

    // 欲しいのは「ヒット点Pの、カメラForward方向の深度」
    const FVector P = OutHit.ImpactPoint; // 基本これ。必要なら OutHit.Location との使い分け
    const float FocusDist = FVector::DotProduct((P - CamPos), Forward);

    // 背面や数値誤差を除外したいなら
    if (FocusDist <= 0.0f)
    {
        return false;
    }

    OutFocusDistanceCm = FocusDist;
    return true;
}