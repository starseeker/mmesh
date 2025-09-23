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
    size_t vertex_count;
    size_t triangle_count;
    size_t vertex_alloc;
} IsolationMesh;

// Simple OBJ loader
int load_obj_isolation(const char *filename, IsolationMesh *mesh) {
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

    // Allocate memory for both formats
    size_t extra_vertices = vertex_count / 4; // 25% extra for vertex splitting
    mesh->float_vertices = malloc((vertex_count + extra_vertices) * sizeof(FloatVertex));
    mesh->uint_triangles = malloc(face_count * sizeof(UIntTriangle));
    mesh->double_vertices = malloc((vertex_count + extra_vertices) * sizeof(DoubleVertex));
    mesh->int_triangles = malloc(face_count * sizeof(IntTriangle));
    
    if (!mesh->float_vertices || !mesh->uint_triangles || !mesh->double_vertices || !mesh->int_triangles) {
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

    fclose(file);
    return 1;
}

typedef struct {
    const char *name;
    int flags;
    int threads;
    int use_double;
    int use_target;
} TestConfig;

void run_isolation_test(IsolationMesh *mesh, TestConfig *config, double feature_size) {
    printf("\n===== %s =====\n", config->name);
    
    mdOperation op;
    mdOperationInit(&op);
    
    if (config->use_double) {
        mdOperationData(&op, mesh->vertex_count, mesh->double_vertices, MD_FORMAT_DOUBLE, 
                3*sizeof(double), mesh->triangle_count, mesh->int_triangles, 
                MD_FORMAT_INT, 3*sizeof(int));
        op.vertexalloc = mesh->vertex_alloc;
        printf("Using DOUBLE vertices, INT triangles (BRL-CAD style)\n");
    } else {
        mdOperationData(&op, mesh->vertex_count, mesh->float_vertices, MD_FORMAT_FLOAT, 
                sizeof(FloatVertex), mesh->triangle_count, mesh->uint_triangles, 
                MD_FORMAT_UINT32, sizeof(UIntTriangle));
        printf("Using FLOAT vertices, UINT32 triangles (mmesh style)\n");
    }
    
    mdOperationStrength(&op, feature_size);
    
    if (config->use_target) {
        op.targetvertexcountmax = 15000;
        printf("Target vertex count: 15000\n");
    }
    
    printf("Flags: ");
    if (config->flags & MD_FLAGS_PLANAR_MODE) printf("PLANAR_MODE ");
    if (config->flags & MD_FLAGS_NORMAL_VERTEX_SPLITTING) printf("NORMAL_VERTEX_SPLITTING ");
    if (config->flags & MD_FLAGS_TRIANGLE_WINDING_CCW) printf("TRIANGLE_WINDING_CCW ");
    printf("\nThreads: %d\n", config->threads);
    
    clock_t start = clock();
    int result = mdMeshDecimation(&op, config->threads, config->flags);
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

    printf("===== ISOLATION TEST: IDENTIFY KEY FACTOR =====\n");
    printf("Testing file: %s\n", filename);
    printf("Systematically varying parameters to isolate the cause\n\n");

    // Load mesh
    IsolationMesh mesh;
    if (!load_obj_isolation(filename, &mesh)) {
        return 1;
    }

    printf("Loaded %zu vertices and %zu triangles\n", mesh.vertex_count, mesh.triangle_count);

    // Calculate feature size
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

    printf("Mesh size: %.3f, Feature size: %.3f\n", mesh_size, feature_size);

    // Define test configurations to isolate the problem
    TestConfig tests[] = {
        // Original mmesh test (baseline)
        {"1. BASELINE: Original mmesh test", MD_FLAGS_PLANAR_MODE, 1, 0, 1},
        
        // Test data format impact
        {"2. DOUBLE format, same flags/threads", MD_FLAGS_PLANAR_MODE, 1, 1, 1},
        
        // Test thread count impact  
        {"3. Original + 2 threads", MD_FLAGS_PLANAR_MODE, 2, 0, 1},
        
        // Test additional flags impact
        {"4. Original + NORMAL_VERTEX_SPLITTING", MD_FLAGS_PLANAR_MODE | MD_FLAGS_NORMAL_VERTEX_SPLITTING, 1, 0, 1},
        {"5. Original + TRIANGLE_WINDING_CCW", MD_FLAGS_PLANAR_MODE | MD_FLAGS_TRIANGLE_WINDING_CCW, 1, 0, 1},
        {"6. Original + both extra flags", MD_FLAGS_PLANAR_MODE | MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW, 1, 0, 1},
        
        // Test target impact
        {"7. Original without target", MD_FLAGS_PLANAR_MODE, 1, 0, 0},
        
        // Full BRL-CAD style
        {"8. FULL BRL-CAD: Double + combined flags + 2 threads", MD_FLAGS_PLANAR_MODE | MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW, 2, 1, 0},
    };
    
    int num_tests = sizeof(tests) / sizeof(tests[0]);
    
    for (int i = 0; i < num_tests; i++) {
        run_isolation_test(&mesh, &tests[i], feature_size);
    }

    printf("\n===== ANALYSIS COMPLETE =====\n");
    printf("Compare results above to identify which parameter change\n");
    printf("causes the dramatic reduction in decimation effectiveness.\n");

    // Cleanup
    free(mesh.float_vertices);
    free(mesh.uint_triangles);
    free(mesh.double_vertices);
    free(mesh.int_triangles);

    return 0;
}