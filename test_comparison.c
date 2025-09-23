#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#include "meshdecimation.h"

typedef struct {
    float x, y, z;
} FloatVertex;

typedef struct {
    double x, y, z;
} DoubleVertex;

typedef struct {
    int v1, v2, v3;
} IntTriangle;

typedef struct {
    uint32_t v1, v2, v3;
} UIntTriangle;

typedef struct {
    FloatVertex *float_vertices;
    UIntTriangle *uint_triangles;
    DoubleVertex *double_vertices;
    IntTriangle *int_triangles;
    DoubleVertex *vertex_normals;
    size_t vertex_count;
    size_t triangle_count;
    size_t vertex_alloc;
} ComparisonMesh;

// Simple OBJ loader that loads data in both formats
int load_obj_comparison(const char *filename, ComparisonMesh *mesh) {
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

    printf("Found %zu vertices and %zu faces in OBJ file\n", vertex_count, face_count);

    // Allocate memory for both formats
    size_t extra_vertices = vertex_count / 4; // 25% extra for vertex splitting
    mesh->float_vertices = malloc((vertex_count + extra_vertices) * sizeof(FloatVertex));
    mesh->uint_triangles = malloc(face_count * sizeof(UIntTriangle));
    mesh->double_vertices = malloc((vertex_count + extra_vertices) * sizeof(DoubleVertex));
    mesh->int_triangles = malloc(face_count * sizeof(IntTriangle));
    mesh->vertex_normals = malloc((vertex_count + extra_vertices) * sizeof(DoubleVertex));
    
    if (!mesh->float_vertices || !mesh->uint_triangles || !mesh->double_vertices || 
        !mesh->int_triangles || !mesh->vertex_normals) {
        printf("Failed to allocate memory\n");
        fclose(file);
        return 0;
    }

    // Second pass: load data in both formats
    rewind(file);
    size_t v_idx = 0, f_idx = 0;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
            double x, y, z;
            if (sscanf(line, "v %lf %lf %lf", &x, &y, &z) == 3) {
                // Store as float
                mesh->float_vertices[v_idx].x = (float)x;
                mesh->float_vertices[v_idx].y = (float)y;
                mesh->float_vertices[v_idx].z = (float)z;
                // Store as double
                mesh->double_vertices[v_idx].x = x;
                mesh->double_vertices[v_idx].y = y;
                mesh->double_vertices[v_idx].z = z;
                v_idx++;
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            int v1, v2, v3;
            if (sscanf(line, "f %d %d %d", &v1, &v2, &v3) == 3) {
                // OBJ uses 1-based indexing, convert to 0-based
                // Store as uint32_t
                mesh->uint_triangles[f_idx].v1 = (uint32_t)(v1 - 1);
                mesh->uint_triangles[f_idx].v2 = (uint32_t)(v2 - 1);
                mesh->uint_triangles[f_idx].v3 = (uint32_t)(v3 - 1);
                // Store as int
                mesh->int_triangles[f_idx].v1 = v1 - 1;
                mesh->int_triangles[f_idx].v2 = v2 - 1;
                mesh->int_triangles[f_idx].v3 = v3 - 1;
                f_idx++;
            }
        }
    }

    mesh->vertex_count = vertex_count;
    mesh->triangle_count = face_count;
    mesh->vertex_alloc = vertex_count + extra_vertices;

    // Initialize vertex normals to zero
    for (size_t i = 0; i < mesh->vertex_alloc; i++) {
        mesh->vertex_normals[i].x = 0.0;
        mesh->vertex_normals[i].y = 0.0;
        mesh->vertex_normals[i].z = 0.0;
    }

    fclose(file);
    printf("Loaded %zu vertices and %zu triangles\n", v_idx, f_idx);
    return 1;
}

void analyze_mesh_planarity(const ComparisonMesh *mesh) {
    size_t planar_count = 0;
    double min_z = INFINITY, max_z = -INFINITY;

    for (size_t i = 0; i < mesh->vertex_count; i++) {
        if (mesh->double_vertices[i].z == 0.0) {
            planar_count++;
        }
        if (mesh->double_vertices[i].z < min_z) min_z = mesh->double_vertices[i].z;
        if (mesh->double_vertices[i].z > max_z) max_z = mesh->double_vertices[i].z;
    }

    printf("Mesh analysis:\n");
    printf("  Vertices with Z=0: %zu (%.1f%%)\n", planar_count, 
            100.0 * planar_count / mesh->vertex_count);
    printf("  Z range: [%.3f, %.3f]\n", min_z, max_z);
}

void status_callback_original(void *context, const mdStatus *status) {
    printf("  Original - Stage %d: %s - Progress: %.1f%% - Triangles: %ld\n", 
            status->stage, status->stagename, status->progress * 100.0, 
            status->trianglecount);
    if (context && status->stage == 7) { // Stage 7 is "Done"
        *(long*)context = status->trianglecount;
    }
}

void status_callback_brlcad(void *context, const mdStatus *status) {
    printf("  BRL-CAD - Stage %d: %s - Progress: %.1f%% - Triangles: %ld\n", 
            status->stage, status->stagename, status->progress * 100.0, 
            status->trianglecount);
    if (context && status->stage == 7) { // Stage 7 is "Done"
        *(long*)context = status->trianglecount;
    }
}

int main(int argc, char **argv) {
    const char *filename = "test.obj";
    if (argc > 1) {
        filename = argv[1];
    }

    printf("===== MMESH PLANAR DECIMATION COMPARISON =====\n");
    printf("Testing file: %s\n", filename);
    printf("Comparing Original Test vs BRL-CAD Compatible approach\n\n");

    // Load mesh
    ComparisonMesh mesh;
    if (!load_obj_comparison(filename, &mesh)) {
        return 1;
    }

    analyze_mesh_planarity(&mesh);
    printf("\nInitial triangle count: %zu\n", mesh.triangle_count);

    // Calculate feature size based on mesh bounds
    double min_x = INFINITY, max_x = -INFINITY;
    double min_y = INFINITY, max_y = -INFINITY;
    for (size_t i = 0; i < mesh.vertex_count; i++) {
        if (mesh.double_vertices[i].x < min_x) min_x = mesh.double_vertices[i].x;
        if (mesh.double_vertices[i].x > max_x) max_x = mesh.double_vertices[i].x;
        if (mesh.double_vertices[i].y < min_y) min_y = mesh.double_vertices[i].y;
        if (mesh.double_vertices[i].y > max_y) max_y = mesh.double_vertices[i].y;
    }

    double mesh_size = sqrt((max_x - min_x) * (max_x - min_x) + 
            (max_y - min_y) * (max_y - min_y));
    double feature_size = mesh_size * 0.05; // 5% of mesh size

    printf("\nMesh bounds: X[%.3f, %.3f], Y[%.3f, %.3f]\n", 
            min_x, max_x, min_y, max_y);
    printf("Mesh size: %.3f, Feature size: %.3f\n\n", mesh_size, feature_size);

    // ===== TEST 1: Original Test Approach =====
    printf("===== TEST 1: ORIGINAL TEST APPROACH =====\n");
    printf("Format: FLOAT vertices, UINT32 triangles, 1 thread, PLANAR_MODE only\n");
    
    mdOperation op1;
    mdOperationInit(&op1);
    mdOperationData(&op1, mesh.vertex_count, mesh.float_vertices, MD_FORMAT_FLOAT, 
            sizeof(FloatVertex), mesh.triangle_count, mesh.uint_triangles, 
            MD_FORMAT_UINT32, sizeof(UIntTriangle));
    mdOperationStrength(&op1, feature_size);
    op1.targetvertexcountmax = 15000; // Like original test
    
    long final_count1 = 0;
    mdOperationStatusCallback(&op1, status_callback_original, &final_count1, 1000);

    clock_t start1 = clock();
    int result1 = mdMeshDecimation(&op1, 1, MD_FLAGS_PLANAR_MODE);
    clock_t end1 = clock();
    double elapsed1 = ((double)(end1 - start1)) / CLOCKS_PER_SEC;

    printf("Original approach result: %s\n", result1 ? "SUCCESS" : "FAILED");
    if (result1) {
        printf("  Time: %.2f seconds\n", elapsed1);
        printf("  Triangles: %zu -> %ld (%.1f%% reduction)\n", 
                mesh.triangle_count, final_count1,
                100.0 * (mesh.triangle_count - final_count1) / mesh.triangle_count);
        printf("  Edge reductions: %ld\n", op1.decimationcount);
        printf("  Collisions: %ld\n", op1.collisioncount);
    }

    printf("\n");

    // ===== TEST 2: BRL-CAD Compatible Approach =====
    printf("===== TEST 2: BRL-CAD COMPATIBLE APPROACH =====\n");
    printf("Format: DOUBLE vertices, INT triangles, 2 threads, combined flags\n");
    
    mdOperation op2;
    mdOperationInit(&op2);
    mdOperationData(&op2, mesh.vertex_count, mesh.double_vertices, MD_FORMAT_DOUBLE, 
            3*sizeof(double), mesh.triangle_count, mesh.int_triangles, 
            MD_FORMAT_INT, 3*sizeof(int));
    op2.vertexalloc = mesh.vertex_alloc;
    mdOperationStrength(&op2, feature_size);
    // Note: NOT calling mdOperationComputeNormals due to segfault issue
    
    long final_count2 = 0;
    mdOperationStatusCallback(&op2, status_callback_brlcad, &final_count2, 1000);

    clock_t start2 = clock();
    int result2 = mdMeshDecimation(&op2, 2, MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW | MD_FLAGS_PLANAR_MODE);
    clock_t end2 = clock();
    double elapsed2 = ((double)(end2 - start2)) / CLOCKS_PER_SEC;

    printf("BRL-CAD compatible result: %s\n", result2 ? "SUCCESS" : "FAILED");
    if (result2) {
        printf("  Time: %.2f seconds\n", elapsed2);
        printf("  Triangles: %zu -> %ld (%.1f%% reduction)\n", 
                mesh.triangle_count, final_count2,
                100.0 * (mesh.triangle_count - final_count2) / mesh.triangle_count);
        printf("  Edge reductions: %ld\n", op2.decimationcount);
        printf("  Collisions: %ld\n", op2.collisioncount);
    }

    printf("\n");

    // ===== COMPARISON SUMMARY =====
    printf("===== COMPARISON SUMMARY =====\n");
    if (result1 && result2) {
        printf("Both approaches succeeded\n");
        printf("Triangle reduction:\n");
        printf("  Original: %zu -> %ld (%.1f%% reduction)\n", 
                mesh.triangle_count, final_count1,
                100.0 * (mesh.triangle_count - final_count1) / mesh.triangle_count);
        printf("  BRL-CAD:  %zu -> %ld (%.1f%% reduction)\n", 
                mesh.triangle_count, final_count2,
                100.0 * (mesh.triangle_count - final_count2) / mesh.triangle_count);
        
        if (final_count1 != final_count2) {
            printf("DIFFERENCE: %ld triangles difference between approaches\n", 
                    labs(final_count1 - final_count2));
            printf("  Original %s more aggressive than BRL-CAD compatible\n",
                    final_count1 < final_count2 ? "is" : "is not");
        } else {
            printf("IDENTICAL: Both approaches produced same triangle count\n");
        }

        printf("Performance:\n");
        printf("  Original: %.2f seconds\n", elapsed1);
        printf("  BRL-CAD:  %.2f seconds (%.1fx %s)\n", elapsed2, 
                elapsed2/elapsed1, elapsed2 > elapsed1 ? "slower" : "faster");
    } else {
        printf("One or both approaches failed\n");
        printf("  Original: %s\n", result1 ? "SUCCESS" : "FAILED");
        printf("  BRL-CAD:  %s\n", result2 ? "SUCCESS" : "FAILED");
    }

    printf("\nKey differences identified:\n");
    printf("1. Data types: FLOAT/UINT32 vs DOUBLE/INT\n");
    printf("2. Stride calculation: sizeof(struct) vs 3*sizeof(type)\n");
    printf("3. Flags: PLANAR_MODE only vs combined flags\n");
    printf("4. Thread count: 1 vs 2\n");
    printf("5. Vertex allocation: automatic vs manual setting\n");
    printf("6. Normal computation: none vs mdOperationComputeNormals (causes segfault)\n");

    // Cleanup
    free(mesh.float_vertices);
    free(mesh.uint_triangles);
    free(mesh.double_vertices);
    free(mesh.int_triangles);
    free(mesh.vertex_normals);

    return 0;
}