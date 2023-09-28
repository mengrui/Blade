// Copyright Terry Meng 2022 All Rights Reserved.
#pragma once

#include "Chaos/Vector.h"
#include "Math/Quat.h"
#include "Math/Vector.h"

//--------------------------------------
#define PIf 3.14159265358979323846f
#define LN2f 0.69314718056f

inline float FastNegexpf(float x)
{
	return 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
}

inline float HalflifeToDamping(float halflife, float eps = 1e-5f)
{
	return (4.0f * LN2f) / (halflife + eps);
}

inline void InertializeTransition(
	FVector& off_x,
	FVector& off_v,
	const FVector src_x,
	const FVector src_v,
	const FVector dst_x,
	const FVector dst_v)
{
	off_x = (src_x + off_x) - dst_x;
	off_v = (src_v + off_v) - dst_v;
}

inline void DecaySpringDamperImplicit(
	FVector& x,
	FVector& v,
	const float halflife,
	const float dt)
{
	const float y = HalflifeToDamping(halflife) / 2.0f;
	const FVector j1 = v + x * y;
	const float eydt = FastNegexpf(y * dt);

	x = eydt * (x + j1 * dt);
	v = eydt * (v - j1 * y * dt);
}

inline void InertializeUpdate(
	FVector& out_x,
	FVector& out_v,
	FVector& off_x,
	FVector& off_v,
	const FVector in_x,
	const FVector in_v,
	const float halflife,
	const float dt)
{
	DecaySpringDamperImplicit(off_x, off_v, halflife, dt);
	out_x = in_x + off_x;
	out_v = in_v + off_v;
}

inline FQuat QuatABS(FQuat x)
{
	return x.W < 0.0 ? FQuat(-x.X, -x.Y, -x.Z, -x.W) : x;
}

inline void InertializeTransition(
	FQuat& off_x,
	FVector& off_v,
	const FQuat src_x,
	const FVector src_v,
	const FQuat dst_x,
	const FVector dst_v)
{
	off_x = QuatABS((((off_x * src_x) * dst_x.Inverse())));
	off_v = (off_v + src_v) - dst_v;
}

FORCEINLINE FVector ToRotationVector(const FQuat& Q)
{
	const FQuat RotQ = Q.Log();
	return FVector(RotQ.X * 2.0f, RotQ.Y * 2.0f, RotQ.Z * 2.0f);
}

FORCEINLINE FQuat MakeFromRotationVector(const FVector& RotationVector)
{
	const FQuat RotQ(RotationVector.X * 0.5f, RotationVector.Y * 0.5f, RotationVector.Z * 0.5f, 0.0f);
	return RotQ.Exp();
}

inline void DecaySpringDamperImplicit(
	FQuat& x,
	FVector& v,
	const float halflife,
	const float dt)
{
	float y = HalflifeToDamping(halflife) / 2.0f;

	FVector j0 = ToRotationVector(x);
	FVector j1 = v + j0 * y;

	float eydt = FastNegexpf(y * dt);

	x = MakeFromRotationVector(eydt * (j0 + j1 * dt));
	v = eydt * (v - j1 * y * dt);
}

inline void InertializeUpdate(
	FQuat& out_x,
	FVector& out_v,
	FQuat& off_x,
	FVector& off_v,
	const FQuat in_x,
	const FVector in_v,
	const float halflife,
	const float dt)
{
	DecaySpringDamperImplicit(off_x, off_v, halflife, dt);
	out_x = off_x * in_x;
	out_v = off_v + in_v;
}

// Taken from https://theorangeduck.com/page/spring-roll-call#controllers
inline void SimulationPositionsUpdate(
	FVector& position, FVector& velocity, FVector& acceleration,
	const FVector desired_velocity, const float halflife, const float dt)
{
	float y = HalflifeToDamping(halflife) / 2.0f;
	const FVector j0 = velocity - desired_velocity;
	const FVector j1 = acceleration + j0 * y;
	const float eydt = FastNegexpf(y * dt);

	position += eydt * (((-j1) / (y * y)) + ((-j0 - j1 * dt) / y)) +
		(j1 / (y * y)) + j0 / y + desired_velocity * dt;
	velocity = eydt * (j0 + j1 * dt) + desired_velocity;
	acceleration = eydt * (acceleration - j1 * y * dt);
}