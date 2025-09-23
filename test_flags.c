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
} FinalMesh;

// Simple OBJ loader
int load_obj_final(const char *filename, FinalMesh *mesh) {
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

typedef struct {
    const char *name;
    int flags;
    const char *flag_desc;
} FlagTest;

void run_flag_test(FinalMesh *mesh, FlagTest *test, double feature_size) {
    printf("\n===== %s =====\n", test->name);
    printf("Flags: %s\n", test->flag_desc);
    
    mdOperation op;
    mdOperationInit(&op);
    
    mdOperationData(&op, mesh->vertex_count, mesh->vertices, MD_FORMAT_DOUBLE, 
            3*sizeof(double), mesh->triangle_count, mesh->triangles, 
            MD_FORMAT_INT, 3*sizeof(int));
    op.vertexalloc = mesh->vertex_alloc;
    
    mdOperationStrength(&op, feature_size);
    
    clock_t start = clock();
    int result = mdMeshDecimation(&op, 2, test->flags); // Always use 2 threads like BRL-CAD
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;

    if (result) {
        double reduction = 100.0 * (mesh->triangle_count - op.tricount) / mesh->triangle_count;
        printf("SUCCESS: %zu -> %zu triangles (%.1f%% reduction) in %.2f seconds\n", 
                mesh->triangle_count, op.tricount, reduction, elapsed);
        printf("Edge reductions: %ld, Collisions: %ld\n", op.decimationcount, op.collisioncount);
    } else {
        printf("FAILED\n");
    }
}

int main(int argc, char **argv) {
    const char *filename = "test.obj";
    if (argc > 1) {
        filename = argv[1];
    }

    printf("===== FINAL TEST: FLAGS IMPACT ANALYSIS =====\n");
    printf("Testing file: %s\n", filename);
    printf("Using BRL-CAD data format but varying flags to isolate the impact\n\n");

    // Load mesh
    FinalMesh mesh;
    if (!load_obj_final(filename, &mesh)) {
        return 1;
    }

    printf("Loaded %zu vertices and %zu triangles\n", mesh.vertex_count, mesh.triangle_count);

    // Calculate feature size
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
    double feature_size = mesh_size * 0.05; // 5% of mesh size

    printf("Mesh size: %.3f, Feature size: %.3f\n", mesh_size, feature_size);

    // Test different flag combinations systematically
    FlagTest tests[] = {
        {"1. PLANAR_MODE only", MD_FLAGS_PLANAR_MODE, "MD_FLAGS_PLANAR_MODE"},
        
        {"2. PLANAR + NORMAL_VERTEX_SPLITTING", 
         MD_FLAGS_PLANAR_MODE | MD_FLAGS_NORMAL_VERTEX_SPLITTING, 
         "MD_FLAGS_PLANAR_MODE | MD_FLAGS_NORMAL_VERTEX_SPLITTING"},
        
        {"3. PLANAR + TRIANGLE_WINDING_CCW", 
         MD_FLAGS_PLANAR_MODE | MD_FLAGS_TRIANGLE_WINDING_CCW, 
         "MD_FLAGS_PLANAR_MODE | MD_FLAGS_TRIANGLE_WINDING_CCW"},
        
        {"4. BRL-CAD: All three flags", 
         MD_FLAGS_PLANAR_MODE | MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW, 
         "MD_FLAGS_PLANAR_MODE | MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW"},
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    
    for (int i = 0; i < num_tests; i++) {
        run_flag_test(&mesh, &tests[i], feature_size);
    }

    printf("\n===== FINAL ANALYSIS =====\n");
    printf("The results above show exactly which flag combination\n");
    printf("is causing BRL-CAD's planar decimation to be less effective.\n");
    printf("This explains why BRL-CAD cannot achieve the same level of\n");
    printf("planar decimation as the mmesh standalone test.\n");

    // Cleanup
    free(mesh.vertices);
    free(mesh.triangles);

    return 0;
}