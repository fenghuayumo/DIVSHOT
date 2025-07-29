# SplatX User Guide

Welcome to the SplatX User Guide.

SplatX is an open source, 3d gaussian splat training and editor client. You can use it to training, view, and edit 3D Gaussian Splats.

## Installing Splatx

SplatX is a desktop app so you need to install it.

https://github.com/fenghuayumo/SplatX/releases

## Training Splats

SplatX can training 3d gaussian splats from multiview images.
1. Darg a video file/image folder to windows, Then it will create a GaussianTraining Component
2. Select the `File` > `New GaussianSplat`, then select image file location and camerapose location 

## Loading Splats

SplatX loads splats from .ply files. Only .ply files containing 3D Gaussian Splat data can be loaded. If you attempt to load any other type of data from a .ply file, it will fail.

There are three ways that you can load a .ply file:

1. Drag and drop one or more .ply files from your file system into SplatX's client area.
2. Select the `File` > `Import` menu item and select one or more .ply files from your file system.

## Saving Splats

To save the currently loaded scene, select the `File` > `Export` or `Export SelectSplats` menu items. 


* **Ply**: original 3d gaussian splat format
* **Compressed Ply**: A lightweight, compressed format that is far smaller than the equivalent uncompressed .ply file. It quantizes splat data and drops spherical harmonics from the output file. See [this article](https://blog.playcanvas.com/compressing-gaussian-splats/) for more details on the format.
* **Splat File**: Another compressed format, although not as efficient as the compressed ply format.
* **spz File**: scaniverse ply format, See https://github.com/nianticlabs/spz 


## Controlling the Camera

The camera controls in SplatX are as follows:

| Control                                         | Description                     |
| ----------------------------------------------- | ------------------------------- |
| Right Mouse Button                              | Orbit camera                    |
| Middle Mouse Button                             | Dolly camera                    |
| Middle Mouse Button + Right Mouse Button        | Pan camera                      |
| A/D Arrow Keys                                  | Strafe camera left/right        |
| W/S Arrow Keys                                  | Dolly camera forwards/backwards |
| F Key                                           | Focus Object                 |