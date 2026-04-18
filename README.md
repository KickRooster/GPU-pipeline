# GPU Pipeline

A DirectX 12 renderer with Nanite-like virtualized geometry and PBR lighting.

## Features

- **Mesh Shaders**: Hardware-accelerated mesh processing pipeline
- **Nanite Geometry**: Continuous LOD with automatic level selection
- **PBR Rendering**: Image-based lighting with physically-based materials
- **Bindless Textures**: Efficient texture management using descriptor arrays
- **CDLOD Terrain**: CDLOD terrain supports
  
## Build Requirements

- Windows 10 (20H1 or later) / Windows 11
- Visual Studio 2019 or later
- GPU with Mesh Shader support (NVIDIA RTX series, AMD RDNA2+)

## Getting Started

Clone with submodules:
```bash
git clone --recurse-submodules git@github.com:KickRooster/GPU-pipeline.git
```

Build and run in Visual Studio.

**Note**: This project uses Git LFS for large binary files. Make sure you have [Git LFS](https://git-lfs.github.com/) installed before cloning.

## Third-party Libraries

- [imgui](https://github.com/ocornut/imgui)
- [assimp](https://github.com/assimp/assimp)
- [meshoptimizer](https://github.com/zeux/meshoptimizer)
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)
- [stb](https://github.com/nothings/stb)

https://github.com/user-attachments/assets/0f0a4a92-9ca2-4c1b-9dc9-7649f4c06e17

https://github.com/user-attachments/assets/8d66e8ca-9b4c-4207-830b-3f813249dd35

https://github.com/user-attachments/assets/6dbd3177-da90-4a61-ba38-8367357bdd58

https://github.com/user-attachments/assets/308836f3-46cd-46bf-98df-ee9ea0a24ea9

https://github.com/user-attachments/assets/2091994f-fc86-48e4-a55e-83a13107410a
