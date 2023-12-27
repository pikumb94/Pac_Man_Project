
#pragma once
#include "CoreMinimal.h"

namespace GridConstants
{
	const float GridSize = 100.f;
}

// Snaps a generic vector in a volumetric grid of CustomGridSize. If the CustomGridSize is not specified or is invalid
// the GridSize constant is used
inline FVector VectorGridSnap(const FVector& Point, float CustomGridSize = 0.f)
{
	FVector SnappedVector;
	if (CustomGridSize > 0.f) {
		SnappedVector.X = FMath::GridSnap(Point.X, CustomGridSize);
		SnappedVector.Y = FMath::GridSnap(Point.Y, CustomGridSize);
		SnappedVector.Z = FMath::GridSnap(Point.Z, CustomGridSize);
	}
	else {
		SnappedVector.X = FMath::GridSnap(Point.X, GridConstants::GridSize);
		SnappedVector.Y = FMath::GridSnap(Point.Y, GridConstants::GridSize);
		SnappedVector.Z = FMath::GridSnap(Point.Z, GridConstants::GridSize);
	}
	
	return SnappedVector;
	
}