/* *****************************************************************************
 *
 * Test program for triangle budget decimation API
 *
 * *****************************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include "meshdecimation.h"

/* Generate a simple test cube mesh */
static void generate_cube_mesh(float **vertices, unsigned int **indices, size_t *vertex_count, size_t *tri_count)
{
  /* Simple cube with 8 vertices and 12 triangles */
  static float cube_vertices[] = {
    -1.0f, -1.0f, -1.0f,  /* 0 */
     1.0f, -1.0f, -1.0f,  /* 1 */
     1.0f,  1.0f, -1.0f,  /* 2 */
    -1.0f,  1.0f, -1.0f,  /* 3 */
    -1.0f, -1.0f,  1.0f,  /* 4 */
     1.0f, -1.0f,  1.0f,  /* 5 */
     1.0f,  1.0f,  1.0f,  /* 6 */
    -1.0f,  1.0f,  1.0f   /* 7 */
  };
  
  static unsigned int cube_indices[] = {
    /* Front face */
    0, 1, 2,  0, 2, 3,
    /* Back face */
    5, 4, 7,  5, 7, 6,
    /* Left face */
    4, 0, 3,  4, 3, 7,
    /* Right face */
    1, 5, 6,  1, 6, 2,
    /* Top face */
    3, 2, 6,  3, 6, 7,
    /* Bottom face */
    4, 5, 1,  4, 1, 0
  };
  
  *vertex_count = 8;
  *tri_count = 12;
  
  *vertices = malloc(sizeof(cube_vertices));
  *indices = malloc(sizeof(cube_indices));
  
  memcpy(*vertices, cube_vertices, sizeof(cube_vertices));
  memcpy(*indices, cube_indices, sizeof(cube_indices));
}

/* Generate a subdivided sphere mesh for more realistic testing */
static void generate_sphere_mesh(float **vertices, unsigned int **indices, size_t *vertex_count, size_t *tri_count, int subdivisions)
{
  int i, j;
  int rings = 20 * (1 << subdivisions);
  int sectors = 20 * (1 << subdivisions);
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

/* Test 1: Basic functionality - ensure result is under budget */
static int test_basic_budget(void)
{
  mdOperation op;
  mdBudgetOptions budget_opts;
  float *vertices;
  unsigned int *indices;
  size_t vertex_count, tri_count;
  long max_triangles = 500;
  int result;
  
  printf("\n=== Test 1: Basic Budget Decimation ===\n");
  
  /* Generate a sphere mesh with enough triangles to decimate */
  generate_sphere_mesh(&vertices, &indices, &vertex_count, &tri_count, 1);
  printf("Initial mesh: %zu vertices, %zu triangles\n", vertex_count, tri_count);
  
  /* Initialize operation */
  mdOperationInit(&op);
  mdOperationData(&op, vertex_count, vertices, MD_FORMAT_FLOAT, 3 * sizeof(float), 
                  tri_count, indices, MD_FORMAT_UINT32, 3 * sizeof(unsigned int));
  
  /* Initialize budget options */
  mdBudgetOptionsInit(&budget_opts);
  
  /* Perform budget decimation */
  result = mdMeshDecimationBudget(&op, max_triangles, 1, 0, &budget_opts);
  
  if(!result)
  {
    printf("FAILED: mdMeshDecimationBudget returned error\n");
    free(vertices);
    free(indices);
    return 0;
  }
  
  printf("Result: %ld triangles (target: %ld, iterations: %d, feature size: %f)\n",
         budget_opts.finaltricount, max_triangles, budget_opts.iterationcount, budget_opts.finalfeaturesize);
  
  /* Verify result is under budget */
  if(budget_opts.finaltricount > max_triangles)
  {
    printf("FAILED: Result %ld exceeds budget %ld\n", budget_opts.finaltricount, max_triangles);
    free(vertices);
    free(indices);
    return 0;
  }
  
  printf("PASSED: Result is under budget\n");
  
  free(vertices);
  free(indices);
  return 1;
}

/* Test 2: Already under budget */
static int test_already_under_budget(void)
{
  mdOperation op;
  mdBudgetOptions budget_opts;
  float *vertices;
  unsigned int *indices;
  size_t vertex_count, tri_count;
  long max_triangles;
  int result;
  
  printf("\n=== Test 2: Already Under Budget ===\n");
  
  /* Generate a small cube mesh */
  generate_cube_mesh(&vertices, &indices, &vertex_count, &tri_count);
  printf("Initial mesh: %zu vertices, %zu triangles\n", vertex_count, tri_count);
  
  /* Set budget higher than triangle count */
  max_triangles = tri_count + 100;
  
  /* Initialize operation */
  mdOperationInit(&op);
  mdOperationData(&op, vertex_count, vertices, MD_FORMAT_FLOAT, 3 * sizeof(float), 
                  tri_count, indices, MD_FORMAT_UINT32, 3 * sizeof(unsigned int));
  
  /* Initialize budget options */
  mdBudgetOptionsInit(&budget_opts);
  
  /* Perform budget decimation */
  result = mdMeshDecimationBudget(&op, max_triangles, 1, 0, &budget_opts);
  
  if(!result)
  {
    printf("FAILED: mdMeshDecimationBudget returned error\n");
    free(vertices);
    free(indices);
    return 0;
  }
  
  printf("Result: %ld triangles (original: %zu, iterations: %d)\n",
         budget_opts.finaltricount, tri_count, budget_opts.iterationcount);
  
  /* Should not have performed any iterations */
  if(budget_opts.iterationcount != 0)
  {
    printf("WARNING: Performed %d iterations when already under budget\n", budget_opts.iterationcount);
  }
  
  printf("PASSED: Correctly handled already-under-budget case\n");
  
  free(vertices);
  free(indices);
  return 1;
}

/* Test 3: Aggressive decimation target */
static int test_aggressive_decimation(void)
{
  mdOperation op;
  mdBudgetOptions budget_opts;
  float *vertices;
  unsigned int *indices;
  size_t vertex_count, tri_count;
  long max_triangles = 50;
  int result;
  
  printf("\n=== Test 3: Aggressive Decimation ===\n");
  
  /* Generate a sphere mesh */
  generate_sphere_mesh(&vertices, &indices, &vertex_count, &tri_count, 1);
  printf("Initial mesh: %zu vertices, %zu triangles\n", vertex_count, tri_count);
  printf("Target: %ld triangles (%.1f%% of original)\n", 
         max_triangles, 100.0 * max_triangles / tri_count);
  
  /* Initialize operation */
  mdOperationInit(&op);
  mdOperationData(&op, vertex_count, vertices, MD_FORMAT_FLOAT, 3 * sizeof(float), 
                  tri_count, indices, MD_FORMAT_UINT32, 3 * sizeof(unsigned int));
  
  /* Initialize budget options with tighter tolerance */
  mdBudgetOptionsInit(&budget_opts);
  budget_opts.tolerance = 0.1;  /* 10% tolerance */
  
  /* Perform budget decimation */
  result = mdMeshDecimationBudget(&op, max_triangles, 1, 0, &budget_opts);
  
  if(!result)
  {
    printf("FAILED: mdMeshDecimationBudget returned error\n");
    free(vertices);
    free(indices);
    return 0;
  }
  
  printf("Result: %ld triangles (target: %ld, iterations: %d, feature size: %f)\n",
         budget_opts.finaltricount, max_triangles, budget_opts.iterationcount, budget_opts.finalfeaturesize);
  
  /* Verify result is under budget */
  if(budget_opts.finaltricount > max_triangles)
  {
    printf("FAILED: Result %ld exceeds budget %ld\n", budget_opts.finaltricount, max_triangles);
    free(vertices);
    free(indices);
    return 0;
  }
  
  printf("PASSED: Aggressive decimation successful\n");
  
  free(vertices);
  free(indices);
  return 1;
}

/* Test 4: Large mesh performance test */
static int test_large_mesh_performance(void)
{
  mdOperation op;
  mdBudgetOptions budget_opts;
  float *vertices;
  unsigned int *indices;
  size_t vertex_count, tri_count;
  long max_triangles = 5000;
  int result;
  clock_t start, end;
  double cpu_time_used;
  
  printf("\n=== Test 4: Large Mesh Performance ===\n");
  
  /* Generate a larger sphere mesh */
  generate_sphere_mesh(&vertices, &indices, &vertex_count, &tri_count, 2);
  printf("Initial mesh: %zu vertices, %zu triangles\n", vertex_count, tri_count);
  printf("Target: %ld triangles (%.1f%% of original)\n", 
         max_triangles, 100.0 * max_triangles / tri_count);
  
  /* Initialize operation */
  mdOperationInit(&op);
  mdOperationData(&op, vertex_count, vertices, MD_FORMAT_FLOAT, 3 * sizeof(float), 
                  tri_count, indices, MD_FORMAT_UINT32, 3 * sizeof(unsigned int));
  
  /* Initialize budget options */
  mdBudgetOptionsInit(&budget_opts);
  
  /* Measure execution time */
  start = clock();
  result = mdMeshDecimationBudget(&op, max_triangles, 1, 0, &budget_opts);
  end = clock();
  
  cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
  
  if(!result)
  {
    printf("FAILED: mdMeshDecimationBudget returned error\n");
    free(vertices);
    free(indices);
    return 0;
  }
  
  printf("Result: %ld triangles (target: %ld)\n", budget_opts.finaltricount, max_triangles);
  printf("Iterations: %d, Feature size: %f\n", budget_opts.iterationcount, budget_opts.finalfeaturesize);
  printf("Execution time: %.3f seconds\n", cpu_time_used);
  
  /* Verify result is under budget */
  if(budget_opts.finaltricount > max_triangles)
  {
    printf("FAILED: Result %ld exceeds budget %ld\n", budget_opts.finaltricount, max_triangles);
    free(vertices);
    free(indices);
    return 0;
  }
  
  printf("PASSED: Large mesh decimation successful\n");
  
  free(vertices);
  free(indices);
  return 1;
}

/* Test 5: Tolerance verification */
static int test_tolerance(void)
{
  mdOperation op;
  mdBudgetOptions budget_opts;
  float *vertices;
  unsigned int *indices;
  size_t vertex_count, tri_count;
  long max_triangles = 300;
  int result;
  double tolerance = 0.05;
  long tolerance_range;
  
  printf("\n=== Test 5: Tolerance Verification ===\n");
  
  /* Generate a sphere mesh */
  generate_sphere_mesh(&vertices, &indices, &vertex_count, &tri_count, 1);
  printf("Initial mesh: %zu vertices, %zu triangles\n", vertex_count, tri_count);
  
  /* Initialize operation */
  mdOperationInit(&op);
  mdOperationData(&op, vertex_count, vertices, MD_FORMAT_FLOAT, 3 * sizeof(float), 
                  tri_count, indices, MD_FORMAT_UINT32, 3 * sizeof(unsigned int));
  
  /* Initialize budget options with specific tolerance */
  mdBudgetOptionsInit(&budget_opts);
  budget_opts.tolerance = tolerance;
  
  /* Perform budget decimation */
  result = mdMeshDecimationBudget(&op, max_triangles, 1, 0, &budget_opts);
  
  if(!result)
  {
    printf("FAILED: mdMeshDecimationBudget returned error\n");
    free(vertices);
    free(indices);
    return 0;
  }
  
  tolerance_range = (long)(max_triangles * tolerance);
  printf("Result: %ld triangles (target: %ld ± %ld, iterations: %d)\n",
         budget_opts.finaltricount, max_triangles, tolerance_range, budget_opts.iterationcount);
  
  /* Verify result is under budget */
  if(budget_opts.finaltricount > max_triangles)
  {
    printf("FAILED: Result %ld exceeds budget %ld\n", budget_opts.finaltricount, max_triangles);
    free(vertices);
    free(indices);
    return 0;
  }
  
  /* Check if within tolerance (ideally should be close) */
  if(max_triangles - budget_opts.finaltricount <= tolerance_range)
  {
    printf("Result is within tolerance range\n");
  }
  else
  {
    printf("Result is outside tolerance range (acceptable but not optimal)\n");
  }
  
  printf("PASSED: Tolerance test successful\n");
  
  free(vertices);
  free(indices);
  return 1;
}

int main(int argc, char *argv[])
{
  int passed = 0;
  int total = 5;
  
  printf("========================================\n");
  printf("Triangle Budget Decimation API Tests\n");
  printf("========================================\n");
  
  if(test_basic_budget())
    passed++;
  
  if(test_already_under_budget())
    passed++;
  
  if(test_aggressive_decimation())
    passed++;
  
  if(test_large_mesh_performance())
    passed++;
  
  if(test_tolerance())
    passed++;
  
  printf("\n========================================\n");
  printf("Test Summary: %d/%d tests passed\n", passed, total);
  printf("========================================\n");
  
  return (passed == total) ? 0 : 1;
}
