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
} AggressiveMesh;

// Simple OBJ loader
int load_obj_aggressive(const char *filename, AggressiveMesh *mesh) {
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
    double feature_factor; // Percentage of mesh size for feature_size
    int use_target;
    size_t target_vertices;
} AggressiveTest;

void status_callback_aggressive(void *context, const mdStatus *status) {
    printf("  %s - Stage %d: %s - Progress: %.1f%% - Triangles: %ld\n", 
            (const char*)context, status->stage, status->stagename, 
            status->progress * 100.0, status->trianglecount);
}

void run_aggressive_test(AggressiveMesh *mesh, AggressiveTest *test, double mesh_size) {
    printf("\n===== %s =====\n", test->name);
    printf("Flags: %s\n", test->flag_desc);
    
    double feature_size = mesh_size * test->feature_factor;
    printf("Feature size: %.3f (%.1f%% of mesh size)\n", feature_size, test->feature_factor * 100);
    
    if (test->use_target) {
        printf("Target vertex count: %zu\n", test->target_vertices);
    } else {
        printf("No target vertex count (unlimited decimation)\n");
    }
    
    mdOperation op;
    mdOperationInit(&op);
    
    mdOperationData(&op, mesh->vertex_count, mesh->vertices, MD_FORMAT_DOUBLE, 
            3*sizeof(double), mesh->triangle_count, mesh->triangles, 
            MD_FORMAT_INT, 3*sizeof(int));
    op.vertexalloc = mesh->vertex_alloc;
    
    mdOperationStrength(&op, feature_size);
    
    if (test->use_target) {
        op.targetvertexcountmax = test->target_vertices;
    }
    
    mdOperationStatusCallback(&op, status_callback_aggressive, (void*)test->name, 2000);
    
    clock_t start = clock();
    int result = mdMeshDecimation(&op, 2, test->flags);
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;

    if (result) {
        double reduction = 100.0 * (mesh->triangle_count - op.tricount) / mesh->triangle_count;
        printf("SUCCESS: %zu -> %zu triangles (%.1f%% reduction) in %.2f seconds\n", 
                mesh->triangle_count, op.tricount, reduction, elapsed);
        printf("Edge reductions: %ld, Collisions: %ld\n", op.decimationcount, op.collisioncount);
        
        // Compare to original test target of ~30k triangles
        if (op.tricount <= 30000) {
            printf("✓ REACHED TARGET: Under 30,000 triangles like original test!\n");
        } else {
            printf("⚠ Did not reach 30k target (still %zu triangles away)\n", op.tricount - 30000);
        }
    } else {
        printf("FAILED\n");
    }
}

int main(int argc, char **argv) {
    const char *filename = "test.obj";
    if (argc > 1) {
        filename = argv[1];
    }

    printf("===== AGGRESSIVE BRL-CAD METHOD TEST =====\n");
    printf("Testing file: %s\n", filename);
    printf("Using more aggressive parameters to match original test effectiveness\n");
    printf("Target: Reach ~30,000 triangles like original test (95.1%% reduction)\n\n");

    // Load mesh
    AggressiveMesh mesh;
    if (!load_obj_aggressive(filename, &mesh)) {
        return 1;
    }

    printf("Loaded %zu vertices and %zu triangles\n", mesh.vertex_count, mesh.triangle_count);

    // Calculate mesh size like original test
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

    printf("Mesh bounds: X[%.3f, %.3f], Y[%.3f, %.3f]\n", 
            min_x, max_x, min_y, max_y);
    printf("Mesh size: %.3f\n", mesh_size);

    // Test progressively more aggressive BRL-CAD configurations
    AggressiveTest tests[] = {
        // Start with current best BRL-CAD approach
        {"1. Current Best BRL-CAD", MD_FLAGS_PLANAR_MODE, "MD_FLAGS_PLANAR_MODE", 0.05, 0, 0},
        
        // Match original test's 2% feature size
        {"2. BRL-CAD + Aggressive Feature Size", MD_FLAGS_PLANAR_MODE, "MD_FLAGS_PLANAR_MODE", 0.02, 0, 0},
        
        // Add target vertex count like original test
        {"3. BRL-CAD + Aggressive + Target 15k", MD_FLAGS_PLANAR_MODE, "MD_FLAGS_PLANAR_MODE", 0.02, 1, 15000},
        
        // Try even smaller feature size
        {"4. BRL-CAD + Very Aggressive (1%)", MD_FLAGS_PLANAR_MODE, "MD_FLAGS_PLANAR_MODE", 0.01, 1, 15000},
        
        // Try smaller target
        {"5. BRL-CAD + Smaller Target (10k)", MD_FLAGS_PLANAR_MODE, "MD_FLAGS_PLANAR_MODE", 0.02, 1, 10000},
        
        // For comparison: What original test config would do with BRL-CAD data format
        {"6. Original Test Config + BRL-CAD Format", MD_FLAGS_PLANAR_MODE, "MD_FLAGS_PLANAR_MODE", 0.02, 1, 15000},
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    
    for (int i = 0; i < num_tests; i++) {
        run_aggressive_test(&mesh, &tests[i], mesh_size);
    }

    printf("\n===== AGGRESSIVE ANALYSIS =====\n");
    printf("The tests above show how aggressive parameter tuning can make\n");
    printf("the BRL-CAD method approach the original test's 95.1%% reduction.\n");
    printf("Target: Reduce 616,892 triangles to ~30,000 (95.1%% reduction)\n");
    printf("\nKey parameters for aggressive decimation:\n");
    printf("- Smaller feature size (1-2%% vs 5%% of mesh size)\n");
    printf("- Target vertex count (10,000-15,000 vertices)\n");
    printf("- Use MD_FLAGS_PLANAR_MODE only (no additional flags)\n");

    // Cleanup
    free(mesh.vertices);
    free(mesh.triangles);

    return 0;
}