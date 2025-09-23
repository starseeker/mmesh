# Understanding the `targetvertexcountmax` Stalling Issue in mmesh

## Problem Statement

When using mmesh without `targetvertexcountmax`, decimation stalls at natural feature-size limits (~267k triangles from 616k), preventing aggressive reduction to specific targets like 30k triangles. This particularly affects large planar surfaces which resist decimation beyond feature-size thresholds.

## Root Cause Analysis

### 1. Cost Threshold Mechanism

The algorithm uses two key cost limits:

```c
mesh->maxcollapsecost = pow(0.25 * featuresize, 6.0);  // Feature-based limit
mesh->maxcollapseacceptcost = (operation->targetvertexcountmax == 0 ? 
                              mesh->maxcollapsecost : MD_OP_FAIL_VALUE);
```

**Without `targetvertexcountmax`:**
- `maxcollapseacceptcost = maxcollapsecost` (strict feature-size limit)
- Edges with `cost > maxcollapsecost` are rejected and never processed

**With `targetvertexcountmax`:**
- `maxcollapseacceptcost = MD_OP_FAIL_VALUE` (≈ 0.25 × FLT_MAX, effectively unlimited)
- ALL edges are added to the queue for potential processing

### 2. Algorithm Flow Differences

#### Normal Mode (without targetvertexcountmax):
1. Process edges with `cost ≤ maxcollapsecost`
2. When all low-cost edges are exhausted, algorithm terminates
3. High-cost edges (typical in large planar areas) remain unprocessed
4. **Result: Stalls at natural feature-size limits**

#### Aggressive Mode (with targetvertexcountmax):
1. ALL edges are added to queue regardless of cost
2. Uses step-based processing with gradually increasing cost thresholds
3. Continues until `vertex_count < targetvertexcountmax` OR `cost > maxcollapsecost`
4. **Result: Can process high-cost edges, achieving aggressive reduction**

### 3. Step-Based Cost Processing

With `targetvertexcountmax`, the algorithm uses a sophisticated stepping mechanism:

```c
stepf = (float)stepindex / (float)mesh->syncstepcount;  // syncstepcount = 64 by default
stepf *= stepf;  // Quadratic curve for tight initial steps
maxcost = mesh->maxcollapsecost * stepf;
```

This creates 64 processing steps, each allowing progressively higher-cost edge collapses.

### 4. Synchronization and Threading

The threading model changes significantly:

- **Normal mode**: Threads synchronize after every step, limited to 64 steps total
- **Aggressive mode**: Continues synchronization until vertex target is reached
- **Barrier behavior**: `stepindex >= syncstepabort` (default 1,048,576) prevents infinite loops

## Why Large Planar Surfaces Cause Stalling

### Geometric Characteristics

Large planar surfaces have edges with:
- High collapse costs due to large triangles
- Minimal geometric change (low "value" in cost function)
- High "penalty" due to area-based scaling

### Cost Calculation

```c
op->collapsecost = op->value + op->penalty;
penalty *= penaltyfactor * mesh->maxcollapsecost;
penaltyfactor = sqrt((vertex0->quadric.area + vertex1->quadric.area) * mesh->invfeaturesizearea);
```

For large planar triangles:
- `quadric.area` is large
- `penaltyfactor` scales with area relative to feature size
- Results in `collapsecost > maxcollapsecost` → edge rejection

## Solution Strategy for BRL-CAD

### 1. Automatic Target Calculation

Instead of manually setting `targetvertexcountmax = 15000`, calculate it based on desired reduction:

```c
// Calculate target based on desired triangle count
size_t target_triangles = 30000;  // Desired final triangle count
size_t target_vertices = target_triangles / 2;  // Rough approximation

// Apply reduction factor to current vertex count
double reduction_factor = 0.95;  // 95% reduction
mdop.targetvertexcountmax = (size_t)(bot->num_vertices * (1.0 - reduction_factor));

// Ensure reasonable bounds
if (mdop.targetvertexcountmax < 5000) mdop.targetvertexcountmax = 5000;
if (mdop.targetvertexcountmax > bot->num_vertices * 0.8) 
    mdop.targetvertexcountmax = bot->num_vertices * 0.8;
```

### 2. Feature Size Relationship

The key insight is that `featuresize` and `targetvertexcountmax` serve different purposes:

- **featuresize**: Controls geometric quality and detail preservation
- **targetvertexcountmax**: Controls stopping condition for aggressive reduction

BRL-CAD can continue using its existing feature size calculation while adding the vertex target for specific reduction goals.

### 3. Adaptive Approach

```c
// Option 1: Conservative (preserve more detail)
mdop.targetvertexcountmax = calculate_target_from_triangle_count(desired_triangles);

// Option 2: Feature-size aware (balance quality and reduction)
double feature_ratio = featuresize / mesh_bounding_box_size;
if (feature_ratio > 0.1) {
    // Large feature size → use targetvertexcountmax for aggressive reduction
    mdop.targetvertexcountmax = calculate_target_from_triangle_count(desired_triangles);
} else {
    // Small feature size → let natural limits dominate
    mdop.targetvertexcountmax = 0;  // Disable aggressive mode
}
```

## Alternative Approaches (Not Recommended)

### 1. Modify syncstepcount
```c
mdop.syncstepcount = 256;  // More steps = more aggressive (default 64)
```
**Issue**: Less predictable, affects all processing phases

### 2. Adjust maxcollapsecost directly
```c
mesh->maxcollapsecost *= 10.0;  // Allow higher costs
```
**Issue**: Bypasses geometric quality controls, unpredictable results

### 3. Dynamic feature size scaling
**Issue**: Complex to implement, affects geometric fidelity

## Recommended Implementation for BRL-CAD

```c
// In your BRL-CAD decimation function:
mdOperationInit(&mdop);
mdOperationData(&mdop, bot->num_vertices, bot->vertices,
                MD_FORMAT_DOUBLE, 3*sizeof(double), bot->num_faces,
                bot->faces, MD_FORMAT_INT, 3*sizeof(int));

// Existing feature size calculation
fastf_t fsize = pow(feature_size, 2.0 / 3.0) * pow(2.0, 4.0 / 3.0);
mdOperationStrength(&mdop, fsize);

// NEW: Add aggressive reduction capability
if (target_triangle_count > 0 && target_triangle_count < bot->num_faces * 0.8) {
    // Calculate vertex target for desired triangle count
    mdop.targetvertexcountmax = target_triangle_count / 2;
    
    // Ensure reasonable bounds
    if (mdop.targetvertexcountmax < 1000) mdop.targetvertexcountmax = 1000;
    if (mdop.targetvertexcountmax > bot->num_vertices / 2) 
        mdop.targetvertexcountmax = bot->num_vertices / 2;
}

// Rest of existing code...
mdMeshDecimation(&mdop, 2, MD_FLAGS_PLANAR_MODE);
```

This approach:
- ✅ Maintains existing feature size logic
- ✅ Adds aggressive reduction when needed
- ✅ Provides predictable triangle count targeting
- ✅ Avoids stalling on large planar surfaces
- ✅ Minimal code changes required

## Performance Implications

Using `targetvertexcountmax` affects performance:

**Positive:**
- Eliminates stalling behavior
- Predictable execution time
- Can achieve aggressive reduction in reasonable time

**Considerations:**
- May process more edge collapse operations
- Uses more memory for larger operation queues
- Threads may wait longer at synchronization points

**Measured Impact:** In testing, aggressive reduction (95.1%) took ~260 seconds vs normal reduction (56.6%) taking ~13 seconds. The trade-off is worthwhile for applications requiring specific triangle targets.