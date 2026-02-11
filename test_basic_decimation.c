/* Simple test to verify basic decimation works */
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
  
  /* Generate vertices */
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
  
  /* Generate indices */
  for(i = 0; i < rings - 1; i++)
  {
    for(j = 0; j < sectors; j++)
    {
      int next_j = (j + 1) % sectors;
      int curr_ring = i * sectors;
      int next_ring = (i + 1) * sectors;
      
      /* First triangle */
      inds[i_idx++] = curr_ring + j;
      inds[i_idx++] = next_ring + j;
      inds[i_idx++] = next_ring + next_j;
      
      /* Second triangle */
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
  float *vertices;
  unsigned int *indices;
  size_t vertex_count, tri_count;
  int result;
  
  printf("Testing basic decimation API\n");
  
  generate_sphere_mesh(&vertices, &indices, &vertex_count, &tri_count);
  printf("Initial mesh: %zu vertices, %zu triangles\n", vertex_count, tri_count);
  
  mdOperationInit(&op);
  mdOperationData(&op, vertex_count, vertices, MD_FORMAT_FLOAT, 3 * sizeof(float), 
                  tri_count, indices, MD_FORMAT_UINT32, 3 * sizeof(unsigned int));
  
  /* Try different feature sizes */
  double feature_sizes[] = {0.001, 0.01, 0.1, 0.5, 1.0};
  int i;
  
  for(i = 0; i < 5; i++)
  {
    /* Make a copy of the data */
    float *verts_copy = malloc(vertex_count * 3 * sizeof(float));
    unsigned int *inds_copy = malloc(tri_count * 3 * sizeof(unsigned int));
    memcpy(verts_copy, vertices, vertex_count * 3 * sizeof(float));
    memcpy(inds_copy, indices, tri_count * 3 * sizeof(unsigned int));
    
    mdOperationInit(&op);
    mdOperationData(&op, vertex_count, verts_copy, MD_FORMAT_FLOAT, 3 * sizeof(float), 
                    tri_count, inds_copy, MD_FORMAT_UINT32, 3 * sizeof(unsigned int));
    op.featuresize = feature_sizes[i];
    
    printf("\nTesting feature size: %f\n", feature_sizes[i]);
    result = mdMeshDecimation(&op, 1, 0);
    
    if(result)
    {
      printf("  Result: %zu triangles (%.1f%% reduction)\n", 
             op.tricount, 100.0 * (1.0 - (double)op.tricount / tri_count));
    }
    else
    {
      printf("  FAILED\n");
    }
    
    free(verts_copy);
    free(inds_copy);
  }
  
  free(vertices);
  free(indices);
  
  return 0;
}
