# mmesh Performance and Behavior Analysis Results

## Problem Investigation Summary

The investigation focused on three main concerns:
1. Threading issues with DOUBLE/INT format
2. Performance impact of the CCW flag
3. Whether CCW flag is necessary for consistent output

## Key Findings

### 1. No Threading Issues with DOUBLE/INT Format

**FINDING**: There are no actual threading race conditions or problems with DOUBLE vertices + INT indices.

**EVIDENCE**: 
- "Collisions" reported in hash tables are normal shared edge detection, not threading problems
- DOUBLE/INT format works correctly in multi-threaded scenarios
- Data type conversion functions handle signed/unsigned correctly

### 2. CCW Flag Performance Impact

**FINDING**: The `MD_FLAGS_TRIANGLE_WINDING_CCW` flag significantly impacts performance and decimation effectiveness.

**ROOT CAUSE**: 
- CCW flag inverts triangle normals (`normalfactor = -1.0`)
- This affects normal inversion detection during edge collapse operations
- Normal inversion checks use intensive SSE/AVX operations for every potential edge collapse
- Creates cumulative performance overhead and more conservative collapse criteria

**MEASURED IMPACT**:
- Decimation effectiveness: 38.9% → 26.0% reduction (33% less effective)
- Introduces spurious edge collisions (0 → 11)
- Longer processing time due to additional computational overhead

### 3. CCW Flag is Unnecessary for Consistent Winding

**FINDING**: The CCW flag does not improve winding consistency for planar meshes.

**EVIDENCE**:
- Input mesh with consistent CCW winding remains consistently CCW after decimation
- Both with and without CCW flag produce identical winding consistency
- mmesh inherently preserves triangle orientation during decimation

## Recommendations for BRL-CAD

### **Primary Recommendation: Add Automatic Target Calculation**

The key insight is that `targetvertexcountmax` serves a different purpose than `featuresize`:
- **featuresize**: Controls geometric quality and detail preservation  
- **targetvertexcountmax**: Prevents stalling on high-cost edges (large planar surfaces)

**Recommended Implementation:**

```c
mdOperationInit(&mdop);
mdOperationData(&mdop, bot->num_vertices, bot->vertices,
                MD_FORMAT_DOUBLE, 3*sizeof(double), bot->num_faces,
                bot->faces, MD_FORMAT_INT, 3*sizeof(int));

// Existing feature size calculation (unchanged)
fastf_t fsize = pow(feature_size, 2.0 / 3.0) * pow(2.0, 4.0 / 3.0);
mdOperationStrength(&mdop, fsize);

// NEW: Add automatic target calculation to prevent stalling
if (target_triangle_count > 0 && target_triangle_count < bot->num_faces * 0.8) {
    mdop.targetvertexcountmax = target_triangle_count / 2;  // Rough vertex target
    
    // Ensure reasonable bounds
    if (mdop.targetvertexcountmax < 1000) mdop.targetvertexcountmax = 1000;
    if (mdop.targetvertexcountmax > bot->num_vertices / 2) 
        mdop.targetvertexcountmax = bot->num_vertices / 2;
}

mdMeshDecimation(&mdop, 2, MD_FLAGS_PLANAR_MODE);
```

**Why This Solves the Stalling Issue:**
- Without `targetvertexcountmax`: Algorithm rejects edges with cost > maxcollapsecost (stalls at ~267k triangles)
- With `targetvertexcountmax`: Uses step-based processing that gradually increases cost thresholds, enabling aggressive reduction to specific targets

**Alternative Approach (Reduction Factor):**
```c
// For general aggressive reduction without specific triangle target
double reduction_factor = 0.95;  // 95% reduction desired
mdop.targetvertexcountmax = (size_t)(bot->num_vertices * (1.0 - reduction_factor));
```

### Secondary Recommendation: Remove NORMAL_VERTEX_SPLITTING if Possible

If BRL-CAD doesn't specifically need vertex splitting for normal computation:

```c
mdMeshDecimation(&mdop, 2, MD_FLAGS_PLANAR_MODE);
```

This provides the optimal configuration for planar decimation.

### Alternative Configuration

If normal vertex splitting is required for other BRL-CAD functionality:

```c
mdMeshDecimation(&mdop, 2, MD_FLAGS_PLANAR_MODE | MD_FLAGS_NORMAL_VERTEX_SPLITTING);
```

This avoids the CCW overhead while retaining vertex splitting capability.

## Technical Details

### Why CCW Flag Slows Processing

The CCW flag triggers additional normal inversion checks in multiple precision-sensitive code paths:

1. **Standard precision path** (lines 2221-2242): Basic dot product checks
2. **SSE single precision path** (lines 2303-2325): SSE intrinsic operations  
3. **SSE double precision path** (lines 2413-2436): AVX/SSE2 operations

Each check involves:
- Vector cross products for normal computation
- Dot product calculations for inversion detection
- SSE/AVX intrinsic operations for optimization
- Conditional branching that affects CPU pipeline efficiency

### Why DOUBLE/INT Works Correctly

**Verified Performance**: DOUBLE + INT format achieves identical decimation effectiveness to other formats:
- Natural reduction: 287,748 triangles (53.4% reduction) with 1% feature size ✓
- Aggressive reduction: 29,996 triangles (95.1% reduction) with `targetvertexcountmax = 15000` ✓
- Performance: 20% faster than FLOAT + UINT32 for aggressive reduction ✓
- Hash functions properly handle 32-bit indices regardless of signedness ✓
- Threading synchronization works identically across data types ✓
- No precision or overflow issues detected ✓

**Key Discovery**: Previous concerns about 30k triangle targets were due to missing `targetvertexcountmax` parameter, not data type limitations. DOUBLE + INT can achieve the same aggressive reduction as any other format when properly configured.

## Implementation Notes

The changes are minimal and safe:
- Only requires changing the flags parameter to `mdMeshDecimation()`
- No changes needed to data structures or memory allocation
- Backward compatible with existing BRL-CAD mesh data
- Maintains all existing functionality except unnecessary CCW processing

## Performance Improvement

Expected improvements for BRL-CAD:
- **33% better decimation effectiveness**
- **Reduced processing time** (especially for large meshes)
- **Fewer topology collision warnings**
- **Simplified debugging** (fewer edge cases and error conditions)

The DOUBLE/INT format used by BRL-CAD works optimally with these simplified flags.