/*
 * main.cc
 * Copyright (C) 2019 Marc Kirchner
 *
 * Distributed under terms of the MIT license.
 */

#include <cfloat>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>

#include "camera.h"
#include "dielectric.h"
#include "diffuselight.h"
#include "hit.h"
#include "lambertian.h"
#include "material.h"
#include "metal.h"
#include "objcollection.h"
#include "ray.h"
#include "sphere.h"
#include "utils.h"
#include "vec3.h"
#include <stdio.h>
#include <omp.h>

const int BYTES_PER_PIXEL = 3; /// red, green, & blue
const int FILE_HEADER_SIZE = 14;
const int INFO_HEADER_SIZE = 40;
void generateBitmapImage(unsigned char* image, int height, int width, char* imageFileName);
unsigned char* createBitmapFileHeader(int height, int stride);
unsigned char* createBitmapInfoHeader(int height, int width);

const Vec3 WHITE = Vec3(1.0, 1.0, 1.0);
const Vec3 BLUE = Vec3(0.2, 0.5, 1.0);

void generateBitmapImage (unsigned char* image, int height, int width, char* imageFileName)
{
    int widthInBytes = width * BYTES_PER_PIXEL;

    unsigned char padding[3] = {0, 0, 0};
    int paddingSize = (4 - (widthInBytes) % 4) % 4;

    int stride = (widthInBytes) + paddingSize;

    FILE* imageFile = fopen(imageFileName, "wb");

    unsigned char* fileHeader = createBitmapFileHeader(height, stride);
    fwrite(fileHeader, 1, FILE_HEADER_SIZE, imageFile);

    unsigned char* infoHeader = createBitmapInfoHeader(height, width);
    fwrite(infoHeader, 1, INFO_HEADER_SIZE, imageFile);

    int i;
    for (i = 0; i < height; i++) {
        fwrite(image + (i*widthInBytes), BYTES_PER_PIXEL, width, imageFile);
        fwrite(padding, 1, paddingSize, imageFile);
    }

    fclose(imageFile);
}

unsigned char* createBitmapFileHeader (int height, int stride)
{
    int fileSize = FILE_HEADER_SIZE + INFO_HEADER_SIZE + (stride * height);

    static unsigned char fileHeader[] = {
        0,0,     /// signature
        0,0,0,0, /// image file size in bytes
        0,0,0,0, /// reserved
        0,0,0,0, /// start of pixel array
    };

    fileHeader[ 0] = (unsigned char)('B');
    fileHeader[ 1] = (unsigned char)('M');
    fileHeader[ 2] = (unsigned char)(fileSize      );
    fileHeader[ 3] = (unsigned char)(fileSize >>  8);
    fileHeader[ 4] = (unsigned char)(fileSize >> 16);
    fileHeader[ 5] = (unsigned char)(fileSize >> 24);
    fileHeader[10] = (unsigned char)(FILE_HEADER_SIZE + INFO_HEADER_SIZE);

    return fileHeader;
}

unsigned char* createBitmapInfoHeader (int height, int width)
{
    static unsigned char infoHeader[] = {
        0,0,0,0, /// header size
        0,0,0,0, /// image width
        0,0,0,0, /// image height
        0,0,     /// number of color planes
        0,0,     /// bits per pixel
        0,0,0,0, /// compression
        0,0,0,0, /// image size
        0,0,0,0, /// horizontal resolution
        0,0,0,0, /// vertical resolution
        0,0,0,0, /// colors in color table
        0,0,0,0, /// important color count
    };

    infoHeader[ 0] = (unsigned char)(INFO_HEADER_SIZE);
    infoHeader[ 4] = (unsigned char)(width      );
    infoHeader[ 5] = (unsigned char)(width >>  8);
    infoHeader[ 6] = (unsigned char)(width >> 16);
    infoHeader[ 7] = (unsigned char)(width >> 24);
    infoHeader[ 8] = (unsigned char)(height      );
    infoHeader[ 9] = (unsigned char)(height >>  8);
    infoHeader[10] = (unsigned char)(height >> 16);
    infoHeader[11] = (unsigned char)(height >> 24);
    infoHeader[12] = (unsigned char)(1);
    infoHeader[14] = (unsigned char)(BYTES_PER_PIXEL*8);

    return infoHeader;
}


Vec3 color(const Ray& r, const ObjCollection& world, int n) {
    ObjCollection::IndexedHit indexedHit;
    if (world.getHit(r, 0.001, FLT_MAX, indexedHit)) {
        size_t index = indexedHit.first;
        Hit hit = indexedHit.second;
        Ray scattered;
        Vec3 attenuation;
        Material::Ptr material = world.getObject(index)->getMaterial();
        Vec3 emission = material->emit(0.0, 0.0, Vec3(0.0, 0.0, 0.0));
        if (n < 25 && material->scatter(r, hit, attenuation, scattered)) {
                return emission + attenuation * color(scattered, world, n+1);
        } else {
            return emission;
        }
    } else {
        return Vec3(0.0, 0.0, 0.0);
        // return Vec3(0.1, 0.15, 0.25);
    }
}

ObjCollection createWorld() {
    ObjCollection world;
    // material
    Vec3 albedo = Vec3(0.04, 0.4, 0.14);
    Material::Ptr lambertianGreen = std::make_shared<Lambertian>(albedo);
    albedo = Vec3(0.99, 0.87, 0.15);
    Material::Ptr lambertianBlue = std::make_shared<Lambertian>(albedo);
    albedo = Vec3(0.8, 0.6, 0.2);
    float fuzz = 0.1;
    Material::Ptr gold = std::make_shared<Metal>(albedo, fuzz);
    albedo = Vec3(0.8, 0.8, 0.8);
    Material::Ptr silver = std::make_shared<Metal>(albedo);
    float ri = 1.5;
    Material::Ptr glass = std::make_shared<Dielectric>(ri);
    // left sphere
    Vec3 center = Vec3(-0.55, 0.0, -1.5);
    float radius = 0.5;
    world.addObject(std::make_shared<Sphere>(gold, center, 0.5));
    // right sphere
    center = Vec3(0.55, 0.0, -1.5);
    radius = 0.5;
    world.addObject(std::make_shared<Sphere>(silver, center, radius));
        // small sphere
    center = Vec3(0.1, -0.3, -1.05);
    radius = 0.2;
    world.addObject(std::make_shared<Sphere>(glass, center, radius));
    // ground sphere
    center = Vec3(0.0, -100.5, -1.0);
    radius = 100.0;
    world.addObject(std::make_shared<Sphere>(lambertianGreen, center, radius));
    // light
    center = Vec3(-1.1, 50.0, 1.05);
    radius = 20;
    Vec3 color = 3*Vec3(1.0, 1.0, 1.0);
    Material::Ptr light = std::make_shared<DiffuseLight>(color);
    world.addObject(std::make_shared<Sphere>(light, center, radius));
    center = Vec3(1.1, 50.0, -1.05);
    radius = 10;
    world.addObject(std::make_shared<Sphere>(light, center, radius));
    return world;
}

void writeImage() {
    int nx = 1920; //width
    int ny = 1080; //height
    int ns = 100; //number of samples

		unsigned char image[ny][nx][BYTES_PER_PIXEL];
    char* imageFileName = (char*) "bitmapImage.bmp";

    // camera position and create world
    Vec3 origin(1.0, 0.5, 1.0);
    Vec3 lookAt(0.0, 0.0, -1.5);
    Vec3 vup(0.0, 1.0, 0.0);
    Camera cam(origin, lookAt, vup, 40, float(nx)/float(ny),
               0.1, (origin-lookAt).norm());
    ObjCollection world = createWorld();

#pragma omp parallel for
    for (int y = ny-1; y >=0; --y) {
        for (int x = 0; x < nx; ++x) {
            Vec3 px(0.0, 0.0, 0.0);
            for (int s = 0; s < ns; ++s) {
                float u = float(x + drand48())/float(nx);
                float v = float(y + drand48())/float(ny);
                Ray r = cam.getRay(u, v);
                px += color(r, world, 0);
            }
            px = px / ns;
            // approximate gamma correction
            px = Vec3(fmin(255.0, 255.00*sqrt(px[0])),
                      fmin(255.0, 255.00*sqrt(px[1])),
                      fmin(255.0, 255.00*sqrt(px[2])));
           // std::cout << int(px[0]) << " " << int(px[1]) << " "
           //           << int(px[2]) << "\n";
					 image[y][x][2]=(unsigned char)(px[0]);
					 image[y][x][1]=(unsigned char)(px[1]);
					 image[y][x][0]=(unsigned char)(px[2]);
        }
    }
		generateBitmapImage((unsigned char*) image, ny, nx, imageFileName);
		printf("Image generated!!");
}

int main(int argc, char* argv[]) {
		omp_set_num_threads(4);
    writeImage();
    return 0;
}


