# Fast caustic design: A fast high contrast freeform optics designer based on OTMap

## Examples

#### Ring:
![ring simulation](data/ring_sim.png)

```
./caustic_design -res 512 -focal_l 1.5 -thickness 0.2 -width 1 -in_trg ../data/ring.png
```

#### Einstein:
![einstein simulation](data/einstein_sim.png)

```
./caustic_design -res 512 -focal_l 1.5 -thickness 0.3 -width 1 -in_trg ../data/einstein.png
```

## Usage

Download the latest build from [Releases](https://github.com/dylanmsu/fast_caustic_design/releases). 

Run the command of one of the examples with the image locations properly filled out. After its complete the 3d model will be located in the directory above the directory of the exe.
## Build from source

This code uses [Eigen](https://eigen.tuxfamily.org), Surface_mesh, and CImg that are already included in the repo/archive.
The only libraries you need to install are [Ceres Solver](http://ceres-solver.org/) for the normal integration and libpng/libjpg for image IO.

It is however highly recommended to install [SuiteSparse/Cholmod](http://faculty.cse.tamu.edu/davis/suitesparse.html) for higher performance.

### Install dependancies on windows:
Install vcpkg:
```bash
$ cd C:\
$ git clone https://github.com/microsoft/vcpkg.git
$ cd vcpkg
$ .\bootstrap-vcpkg.bat
```

Install Ceres solver and its dependancies:
```bash
$ ./vcpkg.exe install ceres[core] --triplet x64-windows-static
```

Install libpng
 and its dependancies:
```bash
$ vcpkg install libpng --triplet x64-windows-static
```

All you then need to do is to clone the repo, configure a build directory with cmake, and then build.

Make sure the path in CMAKE_TOOLCHAIN_FILE is correct.

````bash
$ git clone --recursive git@github.com:dylanmsu/fast_caustic_design.git
$ cd fast_caustic_design
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded -DCMAKE_BUILD_TYPE=Release
$ cmake --build . --config Release
$ ./Release/caustic_design.exe -res 512 -focal_l 1.5 -thickness 0.3 -width 1 -in_trg ../data/einstein.png
````

## How does it work
Creating a lens surface whose shadow matches a target image involves two main steps:

### 1. Figuring out how to move the light
First, we need to determine how light should travel from the lens surface to the target shadow. This is done using a technique called optimal transport (OT). OT is a challenging problem and an area of ongoing research. It gives us a map that tells each point on the lens surface where its light should land on the target image.

Traditionally, this problem is solved using structures called power diagrams (also known as Laguerre-Voronoi diagrams). However, in our case, we use a newer, faster method.

The usage of optimal transport is not just to reduce surface variations. It is actually quite important to have a truly optimal transport plan. According to Brenier's theorem, an optimal transport plan is the gradient of a potential. Without this fact we cannot approximate the inverse gradient (or normal integration) afterwards accurately.

### 2. Shaping the surface to steer the light
Once we have the transport map, the next step is to figure out the exact geometry of the lens surface. For each vertex on the surface, we use the OT map to set the x and y direction of the outgoing ray. The z-direction is chosen based on the focal length.

These rays define how we want the surface to bend the light. Using inverse Snell’s law, we compute the target surface normals needed to steer the rays correctly. Finally, we use a solver (Ceres) to adjust the vertex positions on the lens surface so that the computed normals match the target ones. This is called normal integration and can be thought of as approximating the inverse gradient of the vertex normals.
## Limitation
Currently, the code produces only square lenses, though work is underway to support rectangular lenses. Circular lenses might be possible in the future; however, achieving this will require a complete rewrite of the OTMap solver.

The limitation stems from the fact that the OTMap solver is designed to compute the transport map from an image to a uniform distribution, denoted as T(u->1). Specifically, our optimal transport domain is strictly square, so if we want a circular lens, we need to transport from a circular image towards your target image. 

In this implementation a transport map from a source image u to a target image v T(u->v) is estimated by means of inversion and composition (see Equation 10 in the OTMap paper). This approach is an estimation and not a true optimal transport map. This estimation inadvertently introduces a small curl component into the mapping, so it is no longer purely the gradient of a potential.

Because deriving a heightmap for a lens relies on normal integration, which only utilizes the curl-free component of the mapping, the presence of any curl results in distortions in the caustic lens.

One solution to this issue would be to solve the transport map T(u->1) on a custom domain (think rounded rectangle, circle, ellipse, etc). This requires a rewrite because the current OTMap solver relies on a square domain with quad faces. You could use a triangular mesh as the domain and apply finite element analysis to compute the discrete differential operators. Namely the laplacian and the gradient. The laplacian uses a special stencil, and the gradient is calculated on the dual vertices, so this would not be trivial on a triangle mesh.

A second solution that may be more approachable is modifying the right hand side of equation 11 in the OTMap paper by replacing (h^2) * u(x) with the integral of u(x) / v(T(x)) over the dual cell. This should solve the full Monge-Ampère equation and yield a true L2 optimal transport map T(u->v).

## License

The core of the transport solver is provided under the [GNU Public License v3](https://www.gnu.org/licenses/gpl-3.0.html).

Utilities and applications are released under the [Mozilla Public License 2](https://www.mozilla.org/en-US/MPL/2.0/).

## References

[1] Georges Nader and Gael Guennebaud. _Instant Transport Maps on 2D Grids_. ACM Transactions on Graphics (Proceedings of Siggraph Asia 2018). [[pdf]](https://hal.inria.fr/hal-01884157) [[video]](https://www.youtube.com/watch?v=Ofz4-reJQRk)
