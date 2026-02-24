#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "PhysicsEngine/BodySetup.h"
#include "CylinderConvexTraceComponent.generated.h"

UENUM(BlueprintType)
enum class EActorTagFilterMode : uint8
{
    Include UMETA(DisplayName="Include"),
    Exclude UMETA(DisplayName="Exclude"),
};

/**
 * 描画しない、凸(Convex)衝突のみを持つ「有限円柱（多角柱近似）」スイープ用コンポーネント。
 *
 * - 形状：半径1・高さ1 の N角柱（Z軸が高さ方向）を UBodySetup の FKConvexElem として生成（1回だけ）
 * - スイープ：MoveComponent(bSweep=true) を使用（内部で Chaos の Convex sweep）
 * - 円柱寸法：ScaleXY=Radius, ScaleZ=2*HalfLength で指定
 * - 方向：Transform の回転をそのまま使用（ローカルZが円柱軸）
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UCylinderConvexTraceComponent : public UPrimitiveComponent
{
    GENERATED_BODY()

public:
    UCylinderConvexTraceComponent();

    /** 近似角数（24〜32 推奨）。Register後に自動ビルドされます（bAutoBuildOnRegister=trueの場合）。 */
    UPROPERTY(EditAnywhere, Category="CylinderTrace")
    int32 NumSides = 32;

    /** OnRegisterでConvex生成するか（デフォルト true）。 */
    UPROPERTY(EditAnywhere, Category="CylinderTrace")
    bool bAutoBuildOnRegister = true;

    /** フィルタ不一致ヒットをスキップするための再スイープ上限 */
    UPROPERTY(EditAnywhere, Category="CylinderTrace", meta=(ClampMin="1", ClampMax="256"))
    int32 MaxFilterIterations = 32;

    /** 微小押し出し（cm）。同一ヒット繰り返し回避用（基本はIgnoreで回避できるが保険） */
    UPROPERTY(EditAnywhere, Category="CylinderTrace", meta=(ClampMin="0.0", ClampMax="10.0"))
    float AdvanceEpsilonCm = 0.1f;

public:
    /** 衝突形状（単位N角柱）を構築（必要なら自動でクック）。通常は明示呼び不要。 */
    void BuildUnitPrismConvex(int32 InNumSides);

    /** QueryOnly想定の衝突設定（必要に応じて呼び出し） */
    void ConfigureCollision(ECollisionChannel ObjectType, ECollisionResponse ResponseToAll);

    /**
     * 指定Transformの「ローカルZ-方向」に、有限円柱（多角柱近似）でスイープします。
     *
     * @param Transform    基準Transform（位置=円柱中心開始点、回転=円柱の向き）
     * @param TraceDistance Z-方向に進む距離（cm）
     * @param Radius       円柱半径（cm）
     * @param HalfLength   円柱半長（cm）
     * @param Tag          Actorタグ（Noneならフィルタ無効）
     * @param FilterMode   Include: Tagを持つActorのみ有効 / Exclude: Tagを持つActorは無効
     * @param OutHit       最初に条件を満たしたヒット
     */
    bool CylinderTraceFromTransform(
        const FTransform& Transform,
        float TraceDistance,
        float Radius,
        float HalfLength,
        FName Tag,
        EActorTagFilterMode FilterMode,
        FHitResult& OutHit
    );

public:
    // UActorComponent / UPrimitiveComponent
    virtual void OnRegister() override;
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
    virtual UBodySetup* GetBodySetup() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UBodySetup> BodySetup;

    int32 CachedNumSides = 0;

private:
    void EnsureBodySetup();

    bool ShouldAcceptActorByTag(const AActor* Actor, FName Tag, EActorTagFilterMode Mode) const;

    bool SweepOnce(
        const FVector& StartCenter,
        const FVector& EndCenter,
        const FQuat& Rotation,
        FHitResult& OutHit
    );
};