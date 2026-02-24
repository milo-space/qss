#include "Kismet/KismetMathLibrary.h"

float GetCameraRollDelta(
    const FVector& LocationB,
    const FVector& LocationC,
    const FTransform& TransformA)
{
    // 基準LookAt
    const FRotator LookAtRot =
        UKismetMathLibrary::FindLookAtRotation(LocationB, LocationC);

    // 基準空間に変換
    const FQuat LocalQuat =
        LookAtRot.Quaternion().Inverse() *
        TransformA.GetRotation();

    // Rollが差分
    const float RollDelta =
        LocalQuat.Rotator().Roll;

    return FMath::UnwindDegrees(RollDelta);
}
