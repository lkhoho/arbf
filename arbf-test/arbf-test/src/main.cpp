//
//  main.cpp
//  arbf-test
//
//  Created by Ke Liu on 2/19/16.
//  Copyright (c) 2016 Ke Liu. All rights reserved.
//

#include <memory>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>
#include <tuple>
#include <array>
#include <iostream>
#include <fstream>
#include "json/json.h"
#include "../include/ARBFInterpolator.h"
#include "../include/Config.h"
#include "../include/Common.h"
#include "../include/PPMImage.h"
#include "../include/RawivImage.h"
#include "../include/Mesh.h"
#include "../include/MeshFactory.h"
#include "../include/MeshNeighborhood.h"

using namespace std;
//using namespace boost;

//void initLogger(const string& loggerName)
//{
//    Config::loggerName = "logs/";
//    
//    string info_log = Config::loggerName + Config::loggerINFO;
//    string warning_log = Config::loggerName + Config::loggerWARNING;
//    string error_log = Config::loggerName + Config::loggerERROR;
//    string fatal_log = Config::loggerName + Config::loggerFATAL;
//    
//    google::InitGoogleLogging(loggerName.c_str()); // glog init
//    google::SetLogDestination(google::INFO, info_log.c_str());
//    google::SetLogDestination(google::WARNING, warning_log.c_str());
//    google::SetLogDestination(google::ERROR, error_log.c_str());
//    google::SetLogDestination(google::FATAL, fatal_log.c_str());
//}

int main(int argc, const char **argv) {
    if (argc != 2) {
        fprintf(stderr, "ERROR: incorrect command format. Usage: arbf-test <config_filename>\n");
        return EXIT_FAILURE;
    }

    // read configurations
    Json::Value root, constants, runConfigs;
    ifstream is(argv[1], ifstream::binary);
    is >> root;
    runConfigs = root["run_configs"];
    string projDir = runConfigs.get("project_dir", "").asString();
    string meshFilename = runConfigs.get("mesh_filename", "").asString();
    string disturbanceDirectionFilename = runConfigs.get("disturbance_direction_filename", "").asString();
    Config::neighborhoodSize = runConfigs.get("neighborhood_size", 1).asInt();
    Config::problemDim = runConfigs.get("problem_dim", 2).asInt();
    Config::numEvalPoints = runConfigs.get("evaluation_points", 1000).asUInt();
    Config::disturbanceFactor = runConfigs.get("disturbance_factor", 0.0f).asFloat();
    Config::isDisturbanceEnabled = runConfigs.get("enable_disturbance", false).asBool();
    Config::isDebugEnabled = runConfigs.get("enable_debug", false).asBool();
    Config::setInterpolationScheme(runConfigs.get("interpolation_scheme", "global").asString());
    Config::setBasisFunctionType(runConfigs.get("basis_function_type", "IMQ").asString());
    Config::setMeshType(runConfigs.get("mesh_type", "Tetrahedron").asString());

    constants = root["constants"] ;
    Config::epsilon = constants.get("EPSILON", 1e-3).asDouble();
    Config::c0 = constants.get("C0", 0.2).asDouble();

    if (Config::neighborhoodSize != 0 && Config::neighborhoodSize != 1 && Config::neighborhoodSize != 2) {
        fprintf(stderr, "Error: incorrect neighborhood size.\n\t0: no neighborhood, 1: 1-ring, 2: 2-ring.\n");
        return EXIT_FAILURE;
    }

    if (Config::problemDim != 2 && Config::problemDim != 3) {
        fprintf(stderr, "Error: incorrect model dimension.\n\t2: use 2D model, 3: use 3D model.\n");
        return EXIT_FAILURE;
    }

    clock_t start, span, total;
    start = total = clock();

    // read mesh
    string file_path = projDir + "/data/" + meshFilename;
//    unique_ptr<MeshFactory> meshFactory(new TriMeshFactory());

    unique_ptr<MeshFactory> meshFactory(nullptr);
    unique_ptr<Mesh> mesh(nullptr);
    switch (Config::meshType) {
        case Config::MeshType::Tetrahedron:
            meshFactory = unique_ptr<MeshFactory>(new TetMeshFactory());
            mesh = meshFactory->createMeshFromFile(file_path.c_str());
            printf("\tnv = %d, nf = %d\n", mesh->getNumVertices(), mesh->getNumTriangleFaces());
            break;
        case Config::MeshType::Hexahedron:
            meshFactory = unique_ptr<MeshFactory>(new HexMeshFactory());
            mesh = meshFactory->createMeshFromFile(file_path.c_str());
            printf("\tnv = %d, nf = %d\n", mesh->getNumVertices(), mesh->getNumQuadrangleFaces());
            break;
    }
//    MeshFactoryWithEnlargedBoundingBox boundingBoxFactory;
//    boundingBoxFactory.setMeshFactory(std::move(meshFactory));
//    unique_ptr<Mesh> mesh = boundingBoxFactory.createMeshFromFile(file_path.c_str());
    span = clock() - start;
    total += span;
    printf("Reading mesh ... %lf ms\n", (double) span * 1e3 / CLOCKS_PER_SEC);

    start = clock();
    unique_ptr<BasisFunction> basisFunction(nullptr);
    switch (Config::basisType) {
        case Config::BasisFunctionType::MQ:
            basisFunction = unique_ptr<BasisFunction>(new MQBasisFunction());
            break;
        case Config::BasisFunctionType::Gaussian:
            basisFunction = unique_ptr<BasisFunction>(new GaussianBasisFunction());
            break;
        case Config::BasisFunctionType::TPS:
            basisFunction = unique_ptr<BasisFunction>(new TPSBasisFunction());
            break;
        case Config::BasisFunctionType::IMQ:
        default:
            basisFunction = unique_ptr<BasisFunction>(new IMQBasisFunction());
            break;
    }
    span = clock() - start;
    total += span;
    printf("Setting basis function ... %lf ms\n", (double) span * 1e3 / CLOCKS_PER_SEC);

    ARBFInterpolator interpolator;
    interpolator.setMesh(mesh.get());
    interpolator.setBasisFunction(basisFunction.get());
    
	// find neighboring vertices for each vertex and neighboring faces for each face (center)
    start = clock();
    unique_ptr<MeshNeighborhood> neighbor1Ring(new OneRingMeshNeighborhood());
    neighbor1Ring->computeVertexNeighbors(mesh.get());
    neighbor1Ring->computeFaceNeighbors(mesh.get());
    span = clock() - start;
    total += span;
    printf("Finding 1-ring neighborhood ... %lf ms\n", (double) span * 1e3 / CLOCKS_PER_SEC);

    if (Config::neighborhoodSize == 2) {
        start = clock();
        unique_ptr<MeshNeighborhood> neighbor2Ring(new TwoRingMeshNeighborhood(move(neighbor1Ring).get()));
        neighbor2Ring->computeVertexNeighbors(mesh.get());
        neighbor2Ring->computeFaceNeighbors(mesh.get());
        span = clock() - start;
        total += span;
        printf("Finding 2-ring neighborhood ... %lf ms\n", (double) span * 1e3 / CLOCKS_PER_SEC);
    }

    // make disturbance for all vertices in mesh
    if (Config::isDisturbanceEnabled) {
        start = clock();
        // read directions
        DirectionSamplings dirs(string(projDir + "/" + disturbanceDirectionFilename).c_str());
        // start disturbance
        auto vertices = mesh->getVertices();
        auto vv = neighbor1Ring->getVertexToVertexNeighborhood();
        srand(time(NULL));

        for (unsigned v = 0; v < mesh->getNumVertices(); v++) {
            double p0[] = { vertices[v].x, vertices[v].y, vertices[v].z }; // current vertex
            auto nb = vv[v]; // neighbors for vertex v

            // compute average distance of 1-ring neighboring vertices
            double sum = 0, dist = 0;
            for (auto it = nb.begin(); it != nb.end(); it++) {
                double p1[] = { vertices[*it].x, vertices[*it].y, vertices[*it].z };
                sum += interpolator.computeDistance(p0, p1);
            }
            dist = sum / nb.size();

            // get random direction
            int r = rand() % dirs.getNumDirections();
            auto dir = dirs[r];
            p0[0] += Config::disturbanceFactor * dist * dir[0];
            p0[1] += Config::disturbanceFactor * dist * dir[1];
            p0[2] += Config::disturbanceFactor * dist * dir[2];
            Vertex vertex(vertices[v]);
            vertex.x = p0[0];
            vertex.y = p0[1];
            vertex.z = p0[2];
            mesh->updateVertexAt(v, vertex);
        }

        mesh->update();
        span = clock() - start;
        total += span;
        printf("Add disturbance to mesh ... %lf ms\n", (double) span * 1e3 / CLOCKS_PER_SEC);
    }
    
    // compute metrics
//    start = clock();
//    interpolator.computeTetrahedronMetrics();
//    span = clock() - start;
//    total += span;
//    printf("Computing tetrahedron metrics ... %lf ms\nr", (double) span * 1e3 / CLOCKS_PER_SEC);

    // interpolate
    start = clock();
    if (Config::interpolationScheme == Config::InterpolationScheme::local) {
        interpolator.interpolate_local(Config::numEvalPoints);
    } else {
        interpolator.interpolate_global(Config::numEvalPoints);
    }
    ARBFInterpolator::InterpolateResult result = interpolator.getResult();
    span = clock() - start;
    total += span;
    printf("Interpolating ... %lf ms\n", (double) span * 1e3 / CLOCKS_PER_SEC);

    // export result
    start = clock();
    if (Config::problemDim == 2) {
        PPMImage* output = new PPMImage(get<2>(result)[0], get<2>(result)[1]);
        output->setMaxIntensity(get<1>(result));
        string output_path = projDir + "/" + "output.ppm";
        output->setPath(output_path.c_str());
        output->setImageData(get<0>(result).data());
        output->write();
    } else {
        unsigned dim[] = { static_cast<unsigned>(get<2>(result)[0]),
                           static_cast<unsigned>(get<2>(result)[1]),
                           static_cast<unsigned>(get<2>(result)[2]) };
        float mins[] = { (float) mesh->getMinX(), (float) mesh->getMinY(), (float) mesh->getMinZ() };
        float maxs[] = { (float) mesh->getMaxX(), (float) mesh->getMaxY(), (float) mesh->getMaxZ() };
        RawivImage* output = new RawivImage(dim, mins, maxs);
        string output_path = projDir + "/" + "output.rawiv";
        output->setPath(output_path.c_str());
        
        float *data = new float[dim[0]*dim[1]*dim[2]];
        
        for (int k = 0; k < dim[2]; k++) {
            for (int j = 0; j < dim[1]; j++) {
                for (int i = 0; i < dim[0]; i++) {
                    data[k*dim[1]*dim[0] + j*dim[0] + i] = (float) get<0>(result).data()[k*dim[1]*dim[0] + j*dim[0] + i];
                }
            }
        }
        
        output->setImageData(data);
        printf("\tdim = [%d, %d, %d]\n", dim[0], dim[1], dim[2]);
        printf("\tmins = [%f, %f, %f]\n", mins[0], mins[1], mins[2]);
        printf("\tmaxs = [%f, %f, %f]\n", maxs[0], maxs[1], maxs[2]);
        auto minmax = minmax_element(get<0>(result).begin(), get<0>(result).end());
        printf("\tmin = %f, max = %f\n", *minmax.first, *minmax.second);
        output->write();
        delete [] data;
    }
    
    span = clock() - start;
    total += span;
    printf("Exporting result ... %lf ms\n", (double) span * 1e3 / CLOCKS_PER_SEC);

    printf("Total running time ... %lf seconds.\n", (double) total / CLOCKS_PER_SEC);
	return EXIT_SUCCESS;
}
