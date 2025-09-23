#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#include "meshdecimation.h"

typedef struct {
    double x, y, z;
} DoubleVertex;

typedef struct {
    int v1, v2, v3;
} IntTriangle;

typedef struct {
    DoubleVertex *vertices;
    IntTriangle *triangles;
    size_t vertex_count;
    size_t triangle_count;
    size_t vertex_alloc;
} OptimalMesh;

// Simple OBJ loader
int load_obj_optimal(const char *filename, OptimalMesh *mesh) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        printf("Failed to open file: %s\n", filename);
        return 0;
    }

    // First pass: count vertices and faces
    char line[256];
    size_t vertex_count = 0, face_count = 0;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
            vertex_count++;
        } else if (line[0] == 'f' && line[1] == ' ') {
            face_count++;
        }
    }

    // Allocate memory
    size_t extra_vertices = vertex_count / 4; // 25% extra for vertex splitting
    mesh->vertices = malloc((vertex_count + extra_vertices) * sizeof(DoubleVertex));
    mesh->triangles = malloc(face_count * sizeof(IntTriangle));
    
    if (!mesh->vertices || !mesh->triangles) {
        printf("Failed to allocate memory\n");
        fclose(file);
        return 0;
    }

    // Second pass: load data
    rewind(file);
    size_t v_idx = 0, f_idx = 0;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
            double x, y, z;
            if (sscanf(line, "v %lf %lf %lf", &x, &y, &z) == 3) {
                mesh->vertices[v_idx].x = x;
                mesh->vertices[v_idx].y = y;
                mesh->vertices[v_idx].z = z;
                v_idx++;
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            int v1, v2, v3;
            if (sscanf(line, "f %d %d %d", &v1, &v2, &v3) == 3) {
                // OBJ uses 1-based indexing, convert to 0-based
                mesh->triangles[f_idx].v1 = v1 - 1;
                mesh->triangles[f_idx].v2 = v2 - 1;
                mesh->triangles[f_idx].v3 = v3 - 1;
                f_idx++;
            }
        }
    }

    mesh->vertex_count = vertex_count;
    mesh->triangle_count = face_count;
    mesh->vertex_alloc = vertex_count + extra_vertices;

    fclose(file);
    return 1;
}

void run_optimal_test(OptimalMesh *mesh, double feature_factor, double mesh_size) {
    double feature_size = mesh_size * feature_factor;
    
    mdOperation op;
    mdOperationInit(&op);
    
    mdOperationData(&op, mesh->vertex_count, mesh->vertices, MD_FORMAT_DOUBLE, 
            3*sizeof(double), mesh->triangle_count, mesh->triangles, 
            MD_FORMAT_INT, 3*sizeof(int));
    op.vertexalloc = mesh->vertex_alloc;
    
    mdOperationStrength(&op, feature_size);
    
    clock_t start = clock();
    int result = mdMeshDecimation(&op, 2, MD_FLAGS_PLANAR_MODE);
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;

    if (result) {
        double reduction = 100.0 * (mesh->triangle_count - op.tricount) / mesh->triangle_count;
        printf("%.1f%% feature: %zu -> %zu triangles (%.1f%% reduction) in %.1fs, %ld collisions\n", 
                feature_factor * 100, mesh->triangle_count, op.tricount, reduction, elapsed, op.collisioncount);
    } else {
        printf("%.1f%% feature: FAILED\n", feature_factor * 100);
    }
}

int main(int argc, char **argv) {
    const char *filename = "test.obj";
    if (argc > 1) {
        filename = argv[1];
    }

    printf("===== OPTIMAL FEATURE SIZE SEARCH =====\n");
    printf("Finding the optimal feature size for BRL-CAD method\n\n");

    // Load mesh
    OptimalMesh mesh;
    if (!load_obj_optimal(filename, &mesh)) {
        return 1;
    }

    printf("Loaded %zu vertices and %zu triangles\n", mesh.vertex_count, mesh.triangle_count);

    // Calculate mesh size
    double min_x = INFINITY, max_x = -INFINITY;
    double min_y = INFINITY, max_y = -INFINITY;
    for (size_t i = 0; i < mesh.vertex_count; i++) {
        if (mesh.vertices[i].x < min_x) min_x = mesh.vertices[i].x;
        if (mesh.vertices[i].x > max_x) max_x = mesh.vertices[i].x;
        if (mesh.vertices[i].y < min_y) min_y = mesh.vertices[i].y;
        if (mesh.vertices[i].y > max_y) max_y = mesh.vertices[i].y;
    }

    double mesh_size = sqrt((max_x - min_x) * (max_x - min_x) + 
            (max_y - min_y) * (max_y - min_y));

    printf("Mesh size: %.3f\n\n", mesh_size);

    // Test feature sizes from 1% to 10%
    double feature_factors[] = {0.01, 0.015, 0.02, 0.025, 0.03, 0.035, 0.04, 0.045, 0.05, 0.055, 0.06, 0.07, 0.08, 0.09, 0.10};
    int num_tests = sizeof(feature_factors) / sizeof(feature_factors[0]);
    
    printf("Testing feature sizes to find optimal for BRL-CAD method:\n");
    printf("(Using BRL-CAD format: DOUBLE vertices, INT triangles, 2 threads, PLANAR_MODE only)\n\n");
    
    for (int i = 0; i < num_tests; i++) {
        run_optimal_test(&mesh, feature_factors[i], mesh_size);
    }

    printf("\n===== RECOMMENDATIONS =====\n");
    printf("Based on the results above, identify the feature size that gives:\n");
    printf("1. Best triangle reduction percentage\n");
    printf("2. Fewest collisions\n");
    printf("3. Reasonable performance\n");
    printf("\nThis optimal feature size should be used in BRL-CAD for best planar decimation.\n");

    // Cleanup
    free(mesh.vertices);
    free(mesh.triangles);

    return 0;
}