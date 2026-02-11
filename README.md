# mmesh

`mmesh` is a C library developed by Alexis Naveros, designed to support meshing
algorithms and routines.

The decimation algorithm provides robust mesh simplification, reducing the
number of vertices and triangles while preserving the mesh's overall shape and
features. It is based on the use of quadric error metrics (QEM), a widely
adopted method in geometry processing.  The implementation maintains a queue of
candidate collapses, always selecting the operation with the lowest geometric
error.  It also applies heuristics to maintain important features and prevent
excessive distortion, such as compactness and boundary weighting.

## Triangle Budget Decimation

In addition to the standard feature-size-based decimation, mmesh now provides a
**triangle budget decimation API** that automatically adjusts decimation parameters
to meet a specified maximum triangle count. This API uses binary search to find
the optimal feature size that produces a mesh with triangle count ≤ the target budget
while maximizing quality (getting as close to the budget as possible).

### Key Features

- **Automatic parameter tuning**: No need to manually guess feature size values
- **Guaranteed budget compliance**: Result always has ≤ max_triangles (when achievable)
- **Quality optimization**: Maximizes triangle count within budget for best quality
- **Configurable iteration control**: Adjustable max iterations, tolerance, and time limits
- **Robust fallback handling**: Handles edge cases like impossible budgets gracefully

### Example Usage

```c
#include "meshdecimation.h"

mdOperation op;
mdBudgetOptions budget_opts;

/* Initialize operation with your mesh data */
mdOperationInit(&op);
mdOperationData(&op, vertex_count, vertices, MD_FORMAT_FLOAT, vertex_stride,
                tri_count, indices, MD_FORMAT_UINT32, indices_stride);

/* Set budget decimation options */
mdBudgetOptionsInit(&budget_opts);
budget_opts.maxiterations = 20;     /* Maximum binary search iterations */
budget_opts.tolerance = 0.05;       /* 5% tolerance from target */

/* Decimate to target triangle count */
long max_triangles = 5000;
int result = mdMeshDecimationBudget(&op, max_triangles, 0, 0, &budget_opts);

if(result) {
    printf("Decimated to %ld triangles (target: %ld, iterations: %d)\n",
           budget_opts.finaltricount, max_triangles, budget_opts.iterationcount);
}
```

See `include/meshdecimation.h` for complete API documentation.

## Mesh Optimization

The mesh optimization routines reorder and optimize triangle meshes for
improved vertex cache locality, reducing rendering costs in real-time graphics
pipelines. The core goals are to minimize the Average Cache Miss Rate (ACMR)
and optimize the order of triangle indices for GPU efficiency.
The optimizer approach is inspired by approaches such as Forsyth's and Tipsy's
algorithms but includes custom scoring and parallelization strategies.

