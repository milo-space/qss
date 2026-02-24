#include "CylinderConvexTraceComponent.h"

#include "PhysicsEngine/AggregateGeom.h"
#include "Engine/World.h"

UCylinderConvexTraceComponent::UCylinderConvexTraceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SetMobility(EComponentMobility::Movable);

    SetHiddenInGame(true);
    SetVisibility(false, true);
    bCastDynamicShadow = false;
    CastShadow = false;

    SetSimulatePhysics(false);
    SetEnableGravity(false);

    SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SetGenerateOverlapEvents(false);
    SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
}

void UCylinderConvexTraceComponent::OnRegister()
{
    Super::OnRegister();

    // 重要：OnRegisterはエディタ操作や再登録で何度も呼ばれ得ます。
    // 自動ビルドは「未生成なら作る」だけに留めます。
    if (bAutoBuildOnRegister)
    {
        BuildUnitPrismConvex(NumSides);
    }
}

void UCylinderConvexTraceComponent::EnsureBodySetup()
{
    if (BodySetup)
    {
        return;
    }

    BodySetup = NewObject<UBodySetup>(this, NAME_None, RF_Transient);

    BodySetup->bGenerateMirroredCollision = false;
    BodySetup->bDoubleSidedGeometry = false;

    // Simple(Convex) をクエリに使う
    BodySetup->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseSimpleAsComplex;
}

UBodySetup* UCylinderConvexTraceComponent::GetBodySetup()
{
    EnsureBodySetup();
    return BodySetup;
}

void UCylinderConvexTraceComponent::BuildUnitPrismConvex(const int32 InNumSides)
{
    EnsureBodySetup();

    const int32 ClampedSides = FMath::Clamp(InNumSides, 3, 128);

    // 既に同じ角数で構築済みなら何もしない
    if (CachedNumSides == ClampedSides && BodySetup->AggGeom.ConvexElems.Num() == 1)
    {
        return;
    }

    CachedNumSides = ClampedSides;

    BodySetup->AggGeom.ConvexElems.Reset();

    // 単位多角柱：
    // - 半径 1（XY）
    // - 高さ 1（Z: -0.5..+0.5）
    const float Radius = 1.0f;
    const float HalfHeight = 0.5f;

    TArray<FVector> Verts;
    Verts.Reserve(ClampedSides * 2);

    for (int32 i = 0; i < ClampedSides; ++i)
    {
        const float A = (2.0f * PI) * (static_cast<float>(i) / static_cast<float>(ClampedSides));
        const float X = FMath::Cos(A) * Radius;
        const float Y = FMath::Sin(A) * Radius;

        Verts.Add(FVector(X, Y, -HalfHeight)); // bottom
        Verts.Add(FVector(X, Y, +HalfHeight)); // top
    }

    FKConvexElem Convex;
    Convex.VertexData = MoveTemp(Verts);
    Convex.UpdateElemBox();
    BodySetup->AggGeom.ConvexElems.Add(MoveTemp(Convex));

    // 物理データを作り直し（クック）
    BodySetup->InvalidatePhysicsData();
    BodySetup->CreatePhysicsMeshes();

    // BodyInstance更新
    RecreatePhysicsState();
}

void UCylinderConvexTraceComponent::ConfigureCollision(const ECollisionChannel ObjectType, const ECollisionResponse ResponseToAll)
{
    SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    SetCollisionObjectType(ObjectType);
    SetCollisionResponseToAllChannels(ResponseToAll);
    SetGenerateOverlapEvents(false);
}

FPrimitiveSceneProxy* UCylinderConvexTraceComponent::CreateSceneProxy()
{
    // 描画しない
    return nullptr;
}

FBoxSphereBounds UCylinderConvexTraceComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    // 単位多角柱（半径1・高さ1）をスケールで使用する前提
    const FVector Scale = GetComponentScale();

    // 半径1、高さ0.5 をスケールした概算
    const FVector Extents = FVector(1.0f, 1.0f, 0.5f) * Scale.GetAbs();
    const FVector Origin = LocalToWorld.GetLocation();

    return FBoxSphereBounds(FBox(Origin - Extents, Origin + Extents));
}

bool UCylinderConvexTraceComponent::ShouldAcceptActorByTag(const AActor* Actor, const FName Tag, const EActorTagFilterMode Mode) const
{
    if (Tag.IsNone())
    {
        return true; // フィルタ無効
    }

    if (!Actor)
    {
        // Actorが取れないケースは稀。安全側で：
        // Include: 不採用（条件満たし得ない）
        // Exclude: 採用（除外タグ判定不能＝除外しない）
        return (Mode == EActorTagFilterMode::Exclude);
    }

    const bool bHasTag = Actor->ActorHasTag(Tag);
    return (Mode == EActorTagFilterMode::Include) ? bHasTag : !bHasTag;
}

bool UCylinderConvexTraceComponent::SweepOnce(
    const FVector& StartCenter,
    const FVector& EndCenter,
    const FQuat& Rotation,
    FHitResult& OutHit
)
{
    // Startにテレポート（この段階ではスイープしない）
    SetWorldLocationAndRotation(StartCenter, Rotation, false, nullptr, ETeleportType::TeleportPhysics);

    const FVector Delta = EndCenter - StartCenter;

    FHitResult Hit;
    MoveComponent(
        Delta,
        Rotation,
        /*bSweep*/ true,
        &Hit,
        MOVECOMP_NoFlags,
        ETeleportType::None
    );

    if (Hit.bBlockingHit)
    {
        OutHit = Hit;
        return true;
    }
    return false;
}

bool UCylinderConvexTraceComponent::CylinderTraceFromTransform(
    const FTransform& Transform,
    const float TraceDistance,
    const float Radius,
    const float HalfLength,
    const FName Tag,
    const EActorTagFilterMode FilterMode,
    FHitResult& OutHit
)
{
    // 形状未構築なら構築（OnRegister無効時や初期化順対策）
    BuildUnitPrismConvex(NumSides);

    // 寸法（単位多角柱をスケール）
    SetWorldScale3D(FVector(Radius, Radius, 2.0f * HalfLength));

    const FVector Start = Transform.GetLocation();
    const FVector Dir = -Transform.GetUnitAxis(EAxis::Z); // ローカルZ-方向
    const FVector End = Start + Dir * TraceDistance;
    const FQuat Rot = Transform.GetRotation();

    // Move ignore の状態をこの関数内に閉じる
    ClearMoveIgnoreActors();

    // 反復探索（フィルタ不一致のActorを一時的に無視して次候補へ）
    const int32 IterMax = FMath::Clamp(MaxFilterIterations, 1, 256);

    for (int32 Iter = 0; Iter < IterMax; ++Iter)
    {
        FHitResult Hit;
        if (!SweepOnce(Start, End, Rot, Hit))
        {
            // もうヒットなし
            ClearMoveIgnoreActors();
            return false;
        }

        AActor* HitActor = Hit.GetActor();

        if (ShouldAcceptActorByTag(HitActor, Tag, FilterMode))
        {
            OutHit = Hit;
            ClearMoveIgnoreActors();
            return true;
        }

        // 不一致ならそのActorを無視して再スイープ
        if (HitActor)
        {
            IgnoreActorWhenMoving(HitActor, true);
        }
        else
        {
            // Actorが取れないのに不一致扱いの場合、進めても同じHitを繰り返す恐れがあるため保険で少し進める
            // （ただしこのケースは通常発生しません）
            const FVector NewStart = FMath::Lerp(Start, End, FMath::Clamp(Hit.Time, 0.0f, 1.0f)) + Dir * AdvanceEpsilonCm;
            // Startを変えない設計なので、ここで詰んだら諦める
            ClearMoveIgnoreActors();
            return false;
        }
    }

    ClearMoveIgnoreActors();
    return false;
}