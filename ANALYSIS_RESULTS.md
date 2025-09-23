# BRL-CAD mmesh Planar Decimation Analysis Results

## Problem Statement
BRL-CAD's invocation of mmesh planar decimation was not achieving the same level of reduction as the standalone mmesh planar test. This analysis identifies the root cause and provides a solution.

## BRL-CAD's Current Invocation
```c
mdOperationInit(&mdop);
mdOperationData(&mdop, bot->num_vertices, bot->vertices,
                MD_FORMAT_DOUBLE, 3*sizeof(double), bot->num_faces,
                bot->faces, MD_FORMAT_INT, 3*sizeof(int));
mdOperationStrength(&mdop, fsize);
mdOperationComputeNormals(&mdop, bot->face_normals, MD_FORMAT_DOUBLE, 3*sizeof(double));
mdMeshDecimation(&mdop, 2, MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW | MD_FLAGS_PLANAR_MODE);
```

## Root Cause Analysis Results

Testing with the same data format (DOUBLE vertices, INT triangles) and thread count (2) as BRL-CAD:

| Configuration | Triangle Reduction | Collisions | Performance |
|---------------|-------------------|------------|-------------|
| **PLANAR_MODE only** | **56.7%** (267,364 triangles) | 0 | Fast (13.5s) |
| PLANAR + NORMAL_VERTEX_SPLITTING | 17.6% (508,040 triangles) | 3,199 | Medium (9.7s) |
| PLANAR + TRIANGLE_WINDING_CCW | 27.4% (447,834 triangles) | 6,172 | Slow (18.7s) |
| **BRL-CAD (all flags)** | **28.5%** (441,286 triangles) | **11,318** | Slow (18.8s) |

## Key Findings

1. **Root Cause**: The combination of `MD_FLAGS_NORMAL_VERTEX_SPLITTING` and `MD_FLAGS_TRIANGLE_WINDING_CCW` with `MD_FLAGS_PLANAR_MODE` severely reduces decimation effectiveness.

2. **Data Format Impact**: FLOAT vs DOUBLE and UINT32 vs INT have **NO significant impact** on decimation effectiveness.

3. **Collision Issues**: Combined flags introduce topology collisions (11,318 vs 0 with PLANAR_MODE only).

4. **mdOperationComputeNormals Issue**: This function causes segmentation faults when used as in BRL-CAD's invocation, likely due to incorrect normal buffer setup.

## Recommended Solution for BRL-CAD

**Change the flags from:**
```c
MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW | MD_FLAGS_PLANAR_MODE
```

**To:**
```c
MD_FLAGS_PLANAR_MODE
```

This change will:
- **Double the decimation effectiveness** (28.5% → 56.7% reduction)
- **Eliminate topology collisions** (11,318 → 0 collisions)
- **Improve performance** (18.8s → 13.5s)

## Alternative Configurations

If BRL-CAD requires normal vertex splitting for other reasons, test results show:
- Using only `MD_FLAGS_PLANAR_MODE | MD_FLAGS_NORMAL_VERTEX_SPLITTING` achieves 17.6% reduction
- Using only `MD_FLAGS_PLANAR_MODE | MD_FLAGS_TRIANGLE_WINDING_CCW` achieves 27.4% reduction

However, **MD_FLAGS_PLANAR_MODE alone provides the best results for planar decimation**.

## Additional Notes

1. The `mdOperationComputeNormals()` call should be investigated or removed as it causes crashes.
2. The 2-thread configuration works well and provides good performance.
3. The DOUBLE/INT data format is fine and matches BRL-CAD's existing data structures.

## Test Results Summary

- **Original mmesh test**: 95.1% reduction (29,996 triangles) with target constraints
- **BRL-CAD current**: 28.5% reduction (441,286 triangles) with combined flags
- **BRL-CAD optimized**: 56.7% reduction (267,364 triangles) with PLANAR_MODE only

The optimized configuration achieves **twice the decimation effectiveness** of the current BRL-CAD approach.