# Coplanar Triangle Decimation Issue Analysis

## Problem Description

@starseeker observed that small triangles densely packed on the interior of a planar face have unexpectedly high collapse costs, preventing their decimation. This is counterintuitive since these triangles should be the "lowest hanging fruit" for simplification.

## Root Cause Analysis

### 1. Area-Based Penalty Calculation

The core issue is in the penalty calculation formula:

```c
// Line 2950 in meshdecimation.c
penaltyfactor = sqrt((vertex0->quadric.area + vertex1->quadric.area) * mesh->invfeaturesizearea);
penalty *= penaltyfactor * mesh->maxcollapsecost;
```

Where:
- `vertex0->quadric.area` and `vertex1->quadric.area` = accumulated triangle area for each vertex
- `mesh->invfeaturesizearea = 1.0 / (featuresize * featuresize)`
- `mesh->maxcollapsecost = pow(0.25 * featuresize, 6.0)`

### 2. Quadric Area Accumulation

Each triangle contributes its area to the quadric of its vertices:

```c
// Line 1443 in mdTriangleComputeQuadric()
area = mdfsqrt(MD_VectorDotProduct(plane, plane));  // Triangle area (cross product magnitude)
mathQuadricInit(&q, plane[0], plane[1], plane[2], plane[3], area);
```

### 3. The Penalty Scaling Problem

For vertices on dense planar surfaces:

1. **High Area Accumulation**: Vertices in dense triangle areas accumulate large `quadric.area` values
2. **Area-to-Feature Ratio**: `penaltyfactor = sqrt(total_area * invfeaturesizearea)` becomes large
3. **Cost Amplification**: The penalty gets multiplied by `maxcollapsecost`, making total cost very high

### 4. Mathematical Analysis

For a planar surface with many small triangles:
- Individual triangle area: small
- Accumulated vertex area: large (sum of many triangles)
- Feature size: typically based on overall mesh dimensions
- Result: `penaltyfactor` grows with triangle density, penalizing dense areas

## The Paradox

**Expected Behavior**: Small coplanar triangles should have low collapse cost (no geometric change)

**Actual Behavior**: Small coplanar triangles in dense areas have high collapse cost due to area accumulation

## Potential Solutions

### 1. Planar-Aware Penalty Adjustment

Detect coplanar triangles and reduce their area penalty:

```c
// Modified penalty calculation for coplanar cases
if (is_coplanar_collapse(vertex0, vertex1, threshold)) {
    // Reduce area penalty for coplanar collapses
    penaltyfactor *= 0.1;  // or other reduction factor
}
```

### 2. Area Normalization by Triangle Count

Instead of raw area accumulation, normalize by triangle density:

```c
// Alternative approach: area per triangle
double avg_triangle_area0 = vertex0->quadric.area / vertex0->triangle_count;
double avg_triangle_area1 = vertex1->quadric.area / vertex1->triangle_count;
penaltyfactor = sqrt((avg_triangle_area0 + avg_triangle_area1) * mesh->invfeaturesizearea);
```

### 3. Geometric Change-Based Cost

Prioritize geometric change over area accumulation:

```c
// Check if collapse maintains planarity
double geometric_change = calculate_surface_deviation(collapse);
if (geometric_change < planarity_threshold) {
    penalty *= 0.01;  // Very low penalty for true coplanar collapses
}
```

### 4. Configuration Flag for Coplanar Optimization

Add a new flag specifically for aggressive coplanar simplification:

```c
if (mesh->operationflags & MD_FLAGS_AGGRESSIVE_COPLANAR) {
    // Use alternative penalty calculation for coplanar areas
    penalty = calculate_coplanar_penalty(vertex0, vertex1, mesh);
}
```

## Recommended Implementation

The most practical solution would be option 3 - geometric change-based cost adjustment:

```c
static mdf mdMeshEdgeCollapsePenalty(mdMesh *mesh, mdi v0, mdi v1, mdf *collapsepoint) {
    // ... existing penalty calculation ...
    
    // NEW: Check for coplanar collapse
    if (mesh->operationflags & MD_FLAGS_PLANAR_MODE) {
        mdf geometric_change = calculate_planarity_deviation(mesh, v0, v1, collapsepoint);
        if (geometric_change < mesh->coplanar_threshold) {
            // Dramatic penalty reduction for truly coplanar collapses
            penalty *= 0.01;
        }
    }
    
    return penalty;
}
```

This approach:
- ✅ Preserves existing behavior for non-planar cases
- ✅ Dramatically reduces cost for truly coplanar collapses
- ✅ Uses geometric criteria rather than arbitrary area thresholds
- ✅ Can be controlled via configuration parameters
- ✅ Specifically addresses the "lowest hanging fruit" scenario

## Impact Assessment

**Benefits**:
- Small coplanar triangles get decimated first (as expected)
- Better reduction effectiveness on planar surfaces
- More intuitive behavior for CAD/architectural models

**Considerations**:
- May affect surface quality if threshold is too aggressive
- Requires careful tuning of planarity threshold
- Could impact performance (additional geometric calculations)

## Integration with BRL-CAD

For BRL-CAD integration, this could be combined with the targetvertexcountmax solution:

```c
// Enable aggressive coplanar decimation
mdop.operationflags |= MD_FLAGS_PLANAR_MODE | MD_FLAGS_AGGRESSIVE_COPLANAR;
mdop.coplanar_threshold = 1e-6;  // Tight planarity tolerance
mdop.targetvertexcountmax = target_triangle_count / 2;  // Target-based stopping
```

This provides both targeted reduction capability and improved coplanar surface handling.