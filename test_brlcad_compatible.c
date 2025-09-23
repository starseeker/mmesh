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
    DoubleVertex *vertex_normals; // Changed from face_normals to vertex_normals
    size_t vertex_count;
    size_t triangle_count;
    size_t vertex_alloc; // Total allocated vertex space
} BRLCADMesh;

// Simple OBJ loader that matches BRL-CAD data structure
int load_obj_brlcad_format(const char *filename, BRLCADMesh *mesh) {
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

    // Allocate memory - extra space for vertex splitting during normal computation
    size_t extra_vertices = vertex_count / 4; // Allocate 25% extra vertices for splitting
    mesh->vertices = malloc((vertex_count + extra_vertices) * sizeof(DoubleVertex));
    mesh->triangles = malloc(face_count * sizeof(IntTriangle));
    mesh->vertex_normals = malloc((vertex_count + extra_vertices) * sizeof(DoubleVertex)); // Allocate for all vertices including extra
    if (!mesh->vertices || !mesh->triangles || !mesh->vertex_normals) {
        printf("Failed to allocate memory\n");
        fclose(file);
        return 0;
    }
    
    printf("Allocated %zu vertices (+ %zu extra for splitting), %zu triangles, %zu vertex normals\n", 
           vertex_count, extra_vertices, face_count, vertex_count + extra_vertices);

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
    printf("Loaded %zu vertices and %zu triangles\n", v_idx, f_idx);
    return 1;
}

void compute_initial_face_normals(BRLCADMesh *mesh) {
    printf("Initializing vertex normals array (will be computed by mmesh)...\n");
    // Initialize vertex normals to zero - let mmesh compute them
    for (size_t i = 0; i < mesh->vertex_alloc; i++) {
        mesh->vertex_normals[i].x = 0.0;
        mesh->vertex_normals[i].y = 0.0;
        mesh->vertex_normals[i].z = 0.0;
    }
}

void analyze_mesh_planarity_double(const BRLCADMesh *mesh) {
    size_t planar_count = 0;
    double min_z = INFINITY, max_z = -INFINITY;

    for (size_t i = 0; i < mesh->vertex_count; i++) {
        if (mesh->vertices[i].z == 0.0) {
            planar_count++;
        }
        if (mesh->vertices[i].z < min_z) min_z = mesh->vertices[i].z;
        if (mesh->vertices[i].z > max_z) max_z = mesh->vertices[i].z;
    }

    printf("Mesh analysis:\n");
    printf("  Vertices with Z=0: %zu (%.1f%%)\n", planar_count, 
            100.0 * planar_count / mesh->vertex_count);
    printf("  Z range: [%.3f, %.3f]\n", min_z, max_z);
}

void status_callback_brlcad(void *context, const mdStatus *status) {
    printf("  Stage %d: %s - Progress: %.1f%% - Triangles: %ld\n", 
            status->stage, status->stagename, status->progress * 100.0, 
            status->trianglecount);
    // Store final triangle count in context
    if (context && status->stage == 7) { // Stage 7 is "Done"
        *(long*)context = status->trianglecount;
    }
}

int main(int argc, char **argv) {
    const char *filename = "test.obj";
    if (argc > 1) {
        filename = argv[1];
    }

    printf("Testing BRL-CAD compatible mesh decimation with file: %s\n", filename);
    printf("Replicating BRL-CAD invocation exactly\n\n");

    // Load mesh in BRL-CAD compatible format
    BRLCADMesh mesh;
    if (!load_obj_brlcad_format(filename, &mesh)) {
        return 1;
    }

    // Compute initial face normals like BRL-CAD would have
    compute_initial_face_normals(&mesh);

    analyze_mesh_planarity_double(&mesh);
    printf("\nInitial triangle count: %zu\n", mesh.triangle_count);

    // Calculate feature size based on mesh bounds
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
    double fsize = mesh_size * 0.05; // Start with 5% of mesh size for more conservative decimation

    printf("\nMesh bounds: X[%.3f, %.3f], Y[%.3f, %.3f]\n", 
            min_x, max_x, min_y, max_y);
    printf("Mesh size: %.3f, Feature size (fsize): %.3f\n", mesh_size, fsize);

    // Set up decimation operation exactly like BRL-CAD
    mdOperation mdop;
    mdOperationInit(&mdop);
    
    printf("Setting up mdOperation with BRL-CAD parameters:\n");
    printf("  Vertices: %zu, format=MD_FORMAT_DOUBLE, stride=%zu\n", 
           mesh.vertex_count, 3*sizeof(double));
    printf("  Triangles: %zu, format=MD_FORMAT_INT, stride=%zu\n", 
           mesh.triangle_count, 3*sizeof(int));
    
    // Use exact same parameters as BRL-CAD  
    mdOperationData(&mdop, mesh.vertex_count, mesh.vertices,
                    MD_FORMAT_DOUBLE, 3*sizeof(double), mesh.triangle_count,
                    mesh.triangles, MD_FORMAT_INT, 3*sizeof(int));
    
    // Set vertex allocation size for normal vertex splitting
    mdop.vertexalloc = mesh.vertex_alloc;
    printf("  Vertex allocation: %zu (extra %zu for splitting)\n", mdop.vertexalloc, mesh.vertex_alloc - mesh.vertex_count);
    
    mdOperationStrength(&mdop, fsize);
    // Skip mdOperationComputeNormals to test if this is causing the segfault
    // mdOperationComputeNormals(&mdop, mesh.vertex_normals, MD_FORMAT_DOUBLE, 3*sizeof(double));

    long final_triangle_count = 0;
    mdOperationStatusCallback(&mdop, status_callback_brlcad, &final_triangle_count, 1000);

    printf("\nAttempting BRL-CAD compatible decimation...\n");
    printf("Flags: MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW | MD_FLAGS_PLANAR_MODE\n");
    printf("Thread count: 2 (like BRL-CAD)\n");

    // Execute exactly like BRL-CAD
    clock_t start = clock();
    int result = mdMeshDecimation(&mdop, 2, MD_FLAGS_NORMAL_VERTEX_SPLITTING | MD_FLAGS_TRIANGLE_WINDING_CCW | MD_FLAGS_PLANAR_MODE);
    clock_t end = clock();

    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;

    if (result == 1) {
        printf("\nBRL-CAD compatible decimation completed successfully in %.2f seconds!\n", elapsed);
        printf("Final triangle count: %zu -> %ld\n", 
                mesh.triangle_count, final_triangle_count);
        printf("Edge reductions performed: %ld\n", mdop.decimationcount);
        printf("Collision count (topology errors): %ld\n", mdop.collisioncount);

        if (final_triangle_count < 30000) {
            printf("SUCCESS: Achieved target of <30,000 triangles!\n");
        } else {
            printf("INFO: Final count %ld triangles (no specific target set)\n", final_triangle_count);
        }
    } else {
        printf("\nBRL-CAD compatible decimation FAILED with error code: %d\n", result);
        
        // Try with just planar mode for comparison
        printf("Trying with only MD_FLAGS_PLANAR_MODE for comparison...\n");
        
        mdOperationInit(&mdop);
        mdOperationData(&mdop, mesh.vertex_count, mesh.vertices,
                        MD_FORMAT_DOUBLE, 3*sizeof(double), mesh.triangle_count,
                        mesh.triangles, MD_FORMAT_INT, 3*sizeof(int));
        mdOperationStrength(&mdop, fsize);
        mdOperationComputeNormals(&mdop, mesh.vertex_normals, MD_FORMAT_DOUBLE, 3*sizeof(double));
        mdOperationStatusCallback(&mdop, status_callback_brlcad, NULL, 1000);

        start = clock();
        result = mdMeshDecimation(&mdop, 2, MD_FLAGS_PLANAR_MODE);
        end = clock();
        elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;

        if (result == 1) {
            printf("Planar-only decimation completed in %.2f seconds\n", elapsed);
            printf("Edge reductions: %ld\n", mdop.decimationcount);
            printf("This suggests the issue is with the combined flags\n");
        } else {
            printf("Planar-only decimation also failed with error: %d\n", result);
        }
    }

    // Cleanup
    free(mesh.vertices);
    free(mesh.triangles);
    free(mesh.vertex_normals);

    return 0;
}