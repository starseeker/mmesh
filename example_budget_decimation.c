/* *****************************************************************************
 *
 * Example program demonstrating the triangle budget decimation API
 *
 * This example shows how to use mdMeshDecimationBudget() to decimate a mesh
 * to a target maximum triangle count, with automatic feature size adjustment.
 *
 * Compile: gcc -o example_budget example_budget_decimation.c -I../include -Lsrc -lmmesh -lm -lpthread
 * Run: ./example_budget
 *
 * *****************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "meshdecimation.h"

/* Simple example: generate a UV sphere mesh */
static void generate_sphere(float **vertices, unsigned int **indices, 
                           size_t *vertex_count, size_t *tri_count,
                           int rings, int sectors)
{
  int i, j;
  float *verts;
  unsigned int *inds;
  int v_idx = 0, i_idx = 0;
  
  *vertex_count = rings * sectors;
  *tri_count = 2 * (rings - 1) * sectors;
  
  verts = malloc(*vertex_count * 3 * sizeof(float));
  inds = malloc(*tri_count * 3 * sizeof(unsigned int));
  
  /* Generate sphere vertices */
  for(i = 0; i < rings; i++)
  {
    float theta = M_PI * (float)i / (float)(rings - 1);
    float sin_theta = sinf(theta);
    float cos_theta = cosf(theta);
    
    for(j = 0; j < sectors; j++)
    {
      float phi = 2.0f * M_PI * (float)j / (float)sectors;
      verts[v_idx++] = sin_theta * cosf(phi);
      verts[v_idx++] = cos_theta;
      verts[v_idx++] = sin_theta * sinf(phi);
    }
  }
  
  /* Generate triangle indices */
  for(i = 0; i < rings - 1; i++)
  {
    for(j = 0; j < sectors; j++)
    {
      int next_j = (j + 1) % sectors;
      int curr_ring = i * sectors;
      int next_ring = (i + 1) * sectors;
      
      inds[i_idx++] = curr_ring + j;
      inds[i_idx++] = next_ring + j;
      inds[i_idx++] = next_ring + next_j;
      
      inds[i_idx++] = curr_ring + j;
      inds[i_idx++] = next_ring + next_j;
      inds[i_idx++] = curr_ring + next_j;
    }
  }
  
  *vertices = verts;
  *indices = inds;
}

int main(int argc, char *argv[])
{
  mdOperation op;
  mdBudgetOptions budget_opts;
  float *vertices;
  unsigned int *indices;
  size_t vertex_count, tri_count;
  long max_triangles = 1000;
  int result;
  
  printf("Triangle Budget Decimation Example\n");
  printf("===================================\n\n");
  
  /* Generate a test mesh (sphere with 40x40 resolution) */
  generate_sphere(&vertices, &indices, &vertex_count, &tri_count, 40, 40);
  
  printf("Generated mesh:\n");
  printf("  Vertices: %zu\n", vertex_count);
  printf("  Triangles: %zu\n\n", tri_count);
  
  /* Initialize decimation operation */
  mdOperationInit(&op);
  mdOperationData(&op, vertex_count, vertices, MD_FORMAT_FLOAT, 3 * sizeof(float), 
                  tri_count, indices, MD_FORMAT_UINT32, 3 * sizeof(unsigned int));
  
  /* Configure budget decimation options */
  mdBudgetOptionsInit(&budget_opts);
  budget_opts.maxiterations = 20;      /* Maximum binary search iterations */
  budget_opts.tolerance = 0.05;        /* 5% tolerance from target */
  budget_opts.timelimit = 0;           /* No time limit */
  
  /* Set target triangle budget */
  max_triangles = 1000;  /* Target: reduce from ~6000 to 1000 triangles */
  
  printf("Decimation settings:\n");
  printf("  Target triangles: %ld\n", max_triangles);
  printf("  Max iterations: %d\n", budget_opts.maxiterations);
  printf("  Tolerance: %.1f%%\n\n", budget_opts.tolerance * 100.0);
  
  /* Perform budget-based decimation */
  printf("Decimating mesh...\n");
  result = mdMeshDecimationBudget(&op, max_triangles, 0, 0, &budget_opts);
  
  if(!result)
  {
    printf("ERROR: Decimation failed!\n");
    free(vertices);
    free(indices);
    return 1;
  }
  
  /* Display results */
  printf("\nResults:\n");
  printf("  Final triangles: %ld (target: %ld)\n", 
         budget_opts.finaltricount, max_triangles);
  printf("  Final vertices: %zu\n", op.vertexcount);
  printf("  Iterations: %d\n", budget_opts.iterationcount);
  printf("  Feature size: %.6f\n", budget_opts.finalfeaturesize);
  printf("  Reduction: %.1f%%\n", 
         100.0 * (1.0 - (double)budget_opts.finaltricount / tri_count));
  
  /* Verify budget compliance */
  if(budget_opts.finaltricount <= max_triangles)
  {
    printf("\n✓ SUCCESS: Result is within budget!\n");
  }
  else
  {
    printf("\n✗ WARNING: Result exceeds budget (target may be unreachable)\n");
  }
  
  /* The decimated mesh is now in op.vertex and op.indices */
  /* You can save it, render it, or process it further */
  
  free(vertices);
  free(indices);
  
  return 0;
}
