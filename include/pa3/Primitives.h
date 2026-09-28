#pragma once
#include "Mesh.h"
namespace pa3::primitives {
MeshData cube();
MeshData sphere(unsigned slices = 48, unsigned stacks = 24);
MeshData cylinder(bool cone = false, unsigned slices = 48);
MeshData torus(unsigned rings = 64, unsigned sides = 24);
}
