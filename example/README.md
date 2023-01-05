# Examples in this folder

## 01_hello_triangle

A simple example showing how to render an RGB triangle.
![hello_triangle](media/hello_triangle.png "A large triangle with red, blue, and green vertices with a violet background")

## 02_deferred

An implementation of a basic deferred renderer with a single directional light. This example also implements the paper Reflective Shadow Maps.
![deferred](media/deferred.png "Cornell box-like scene with a single light coming from the viewer and color from the walls softly bleeding onto the others")

## 03_gltf_viewer

A program that demonstrates the loading and rendering of glTF scene files using tinygltf and Fwog. Sponza glTF not included.
![gltf_viewer](media/gltf_viewer.png "View of the atrium in Sponza from below, with the sun illuminating the center of the ground floor")

## 04_volumetric

A ray-marched volumetric fog implementation using a frustum-aligned 3D grid. Supports fog shadows and local lights.
![volumetric](media/volumetric0.png "A forest scene featuring a cube of fog and some local lights illuminating it")

## 05_gpu_driven

An example using bindless textures, GPU-driven fragment shader occlusion culling, and indirect multidraw to minimize draw calls.
![gpu_driven](media/gpu_driven.png "A forest scene with wireframe bounding boxes around each object")
