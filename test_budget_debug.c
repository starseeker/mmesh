/* Debug test for budget decimation */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "meshdecimation.h"

static void generate_sphere_mesh(float **vertices, unsigned int **indices, size_t *vertex_count, size_t *tri_count)
{
  int i, j;
  int rings = 20;
  int sectors = 20;
  float *verts;
  unsigned int *inds;
  int v_idx = 0, i_idx = 0;
  
  *vertex_count = rings * sectors;
  *tri_count = 2 * (rings - 1) * sectors;
  
  verts = malloc(*vertex_count * 3 * sizeof(float));
  inds = malloc(*tri_count * 3 * sizeof(unsigned int));
  
  for(i = 0; i < rings; i++)
  {
    float theta = M_PI * (float)i / (float)(rings - 1);
    float sin_theta = sinf(theta);
    float cos_theta = cosf(theta);
    
    for(j = 0; j < sectors; j++)
    {
      float phi = 2.0f * M_PI * (float)j / (float)sectors;
      float sin_phi = sinf(phi);
      float cos_phi = cosf(phi);
      
      verts[v_idx++] = sin_theta * cos_phi;
      verts[v_idx++] = cos_theta;
      verts[v_idx++] = sin_theta * sin_phi;
    }
  }
  
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

int main(void)
{
  mdOperation op;
  mdBudgetOptions budget_opts;
  float *vertices;
  unsigned int *indices;
  size_t vertex_count, tri_count;
  long max_triangles = 200;
  int result;
  
  printf("Debug test for budget decimation\n\n");
  
  generate_sphere_mesh(&vertices, &indices, &vertex_count, &tri_count);
  printf("Initial mesh: %zu vertices, %zu triangles\n", vertex_count, tri_count);
  printf("Target: %ld triangles\n\n", max_triangles);
  
  mdOperationInit(&op);
  mdOperationData(&op, vertex_count, vertices, MD_FORMAT_FLOAT, 3 * sizeof(float), 
                  tri_count, indices, MD_FORMAT_UINT32, 3 * sizeof(unsigned int));
  
  mdBudgetOptionsInit(&budget_opts);
  budget_opts.maxiterations = 10;  /* Reduced for debugging */
  
  result = mdMeshDecimationBudget(&op, max_triangles, 1, 0, &budget_opts);
  
  printf("Result:\n");
  printf("  Success: %d\n", result);
  printf("  Iterations: %d\n", budget_opts.iterationcount);
  printf("  Final feature size: %f\n", budget_opts.finalfeaturesize);
  printf("  Final triangle count: %ld\n", budget_opts.finaltricount);
  
  if(result && budget_opts.finaltricount > 0)
  {
    printf("  Reduction: %.1f%%\n", 100.0 * (1.0 - (double)budget_opts.finaltricount / tri_count));
    if(budget_opts.finaltricount <= max_triangles)
    {
      printf("  PASSED: Under budget!\n");
    }
    else
    {
      printf("  FAILED: Exceeds budget!\n");
    }
  }
  
  free(vertices);
  free(indices);
  
  return result ? 0 : 1;
}
