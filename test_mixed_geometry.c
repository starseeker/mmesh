#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdint.h>

#include "meshdecimation.h"

typedef struct {
    float x, y, z;
} Vertex;

typedef struct {
    int v1, v2, v3;
} Triangle;

typedef struct {
    Vertex *vertices;
    Triangle *triangles;
    size_t vertex_count;
    size_t triangle_count;
} Mesh;

// Simple OBJ loader
int load_obj(const char *filename, Mesh *mesh) {
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
    
    // Allocate memory
    mesh->vertices = malloc(vertex_count * sizeof(Vertex));
    mesh->triangles = malloc(face_count * sizeof(Triangle));
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
            float x, y, z;
            if (sscanf(line, "v %f %f %f", &x, &y, &z) == 3) {
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
    
    fclose(file);
    printf("Loaded %zu vertices and %zu triangles\n", v_idx, f_idx);
    return 1;
}

void analyze_mesh_planarity(const Mesh *mesh) {
    size_t planar_count = 0;
    size_t nonplanar_count = 0;
    float min_z = INFINITY, max_z = -INFINITY;
    
    for (size_t i = 0; i < mesh->vertex_count; i++) {
        if (mesh->vertices[i].z == 0.0f) {
            planar_count++;
        } else {
            nonplanar_count++;
        }
        if (mesh->vertices[i].z < min_z) min_z = mesh->vertices[i].z;
        if (mesh->vertices[i].z > max_z) max_z = mesh->vertices[i].z;
    }
    
    printf("Mesh analysis:\n");
    printf("  Vertices with Z=0 (planar): %zu (%.1f%%)\n", planar_count, 
           100.0 * planar_count / mesh->vertex_count);
    printf("  Vertices with Z≠0 (non-planar): %zu (%.1f%%)\n", nonplanar_count,
           100.0 * nonplanar_count / mesh->vertex_count);
    printf("  Z range: [%.3f, %.3f]\n", min_z, max_z);
}

void status_callback(void *context, const mdStatus *status) {
    printf("  Stage %d: %s - Progress: %.1f%% - Triangles: %ld\n", 
           status->stage, status->stagename, status->progress * 100.0, 
           status->trianglecount);
    // Store final triangle count in context
    if (context && status->stage == 7) { // Stage 7 is "Done"
        *(long*)context = status->trianglecount;
    }
}

int test_decimation_mode(const Mesh *mesh, int flags, const char *mode_name) {
    printf("\n=== Testing %s ===\n", mode_name);
    
    // Set up decimation operation
    mdOperation op;
    mdOperationInit(&op);
    
    // Configure mesh data
    mdOperationData(&op, mesh->vertex_count, mesh->vertices, MD_FORMAT_FLOAT, 
                    sizeof(Vertex), mesh->triangle_count, mesh->triangles, 
                    MD_FORMAT_UINT32, sizeof(Triangle));
    
    // Calculate feature size based on mesh bounds
    float min_x = INFINITY, max_x = -INFINITY;
    float min_y = INFINITY, max_y = -INFINITY;
    for (size_t i = 0; i < mesh->vertex_count; i++) {
        if (mesh->vertices[i].x < min_x) min_x = mesh->vertices[i].x;
        if (mesh->vertices[i].x > max_x) max_x = mesh->vertices[i].x;
        if (mesh->vertices[i].y < min_y) min_y = mesh->vertices[i].y;
        if (mesh->vertices[i].y > max_y) max_y = mesh->vertices[i].y;
    }
    
    float mesh_size = sqrt((max_x - min_x) * (max_x - min_x) + 
                          (max_y - min_y) * (max_y - min_y));
    double feature_size = mesh_size * 0.02; // 2% of mesh size
    
    printf("Feature size: %.3f\n", feature_size);
    
    mdOperationStrength(&op, feature_size);
    
    long final_triangle_count = 0;
    mdOperationStatusCallback(&op, status_callback, &final_triangle_count, 1000);
    
    // Set target to achieve aggressive decimation
    op.targetvertexcountmax = 15000;
    
    // Try decimation
    clock_t start = clock();
    int result = mdMeshDecimation(&op, 1, flags);
    clock_t end = clock();
    
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    if (result == 1) {
        printf("Decimation completed in %.2f seconds\n", elapsed);
        printf("Final triangle count: %zu -> %ld\n", 
               mesh->triangle_count, final_triangle_count);
        printf("Edge reductions performed: %ld\n", op.decimationcount);
        printf("Collision count (topology errors): %ld\n", op.collisioncount);
        return 1;
    } else {
        printf("Decimation FAILED with error code: %d\n", result);
        return 0;
    }
}

int main(int argc, char **argv) {
    const char *filename = "test.obj";
    if (argc > 1) {
        filename = argv[1];
    }
    
    printf("Testing impact of planar mode on mixed geometry: %s\n\n", filename);
    
    // Load mesh
    Mesh mesh;
    if (!load_obj(filename, &mesh)) {
        return 1;
    }
    
    analyze_mesh_planarity(&mesh);
    printf("\nInitial triangle count: %zu\n", mesh.triangle_count);
    
    // Test 1: No flags (baseline)
    int success1 = test_decimation_mode(&mesh, 0, "No flags (baseline)");
    
    // Test 2: Only planar mode
    int success2 = test_decimation_mode(&mesh, MD_FLAGS_PLANAR_MODE, "Planar mode only");
    
    // Test 3: Normal splitting + CCW (BRL-CAD current)
    int success3 = test_decimation_mode(&mesh, 
        MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW,
        "BRL-CAD current flags");
    
    // Test 4: Normal splitting + CCW + Planar (BRL-CAD proposed)
    int success4 = test_decimation_mode(&mesh, 
        MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW | MD_FLAGS_PLANAR_MODE,
        "BRL-CAD proposed flags");
    
    printf("\n=== SUMMARY ===\n");
    printf("No flags:           %s\n", success1 ? "SUCCESS" : "FAILED");
    printf("Planar mode only:   %s\n", success2 ? "SUCCESS" : "FAILED");
    printf("BRL-CAD current:    %s\n", success3 ? "SUCCESS" : "FAILED");
    printf("BRL-CAD proposed:   %s\n", success4 ? "SUCCESS" : "FAILED");
    
    if (success3 && success4) {
        printf("\n✅ Adding MD_FLAGS_PLANAR_MODE appears SAFE for mixed geometry\n");
    } else if (success4 && !success3) {
        printf("\n⚠️  Adding MD_FLAGS_PLANAR_MODE IMPROVES decimation for mixed geometry\n");
    } else if (success3 && !success4) {
        printf("\n❌ Adding MD_FLAGS_PLANAR_MODE BREAKS decimation for mixed geometry\n");
    } else {
        printf("\n❓ Both configurations have issues - needs investigation\n");
    }
    
    // Cleanup
    free(mesh.vertices);
    free(mesh.triangles);
    
    return 0;
}