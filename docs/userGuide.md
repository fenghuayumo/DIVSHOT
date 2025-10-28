## User Guide

Welcome to the DIVSHOT User Guide.

DIVSHOT is an open source, 3d gaussian splat training and editor client. You can use it to training, view, and edit 3D Gaussian Splats. 

## Hardware Requirements

Currently only supports Windows systems with NVIDIA RTX 20 series graphics cards and above (CUDA 7.0 compute capability or higher). AMD graphics card support will be added in the future. 

Since training Splats relies on gradient descent backpropagation, it requires significant VRAM and RAM. Please ensure you have at least 24GB RAM and 6GB VRAM. We recommend 32GB RAM and 16GB VRAM. If you want to train larger Gaussian scenes, you will need even more memory and VRAM. Currently, the system has been tested with 2000 2K-sized images on 32GB RAM and 16GB VRAM, and it runs very well. If your scene data is larger, please upgrade your hardware accordingly.
## Installing DIVSHOT

DIVSHOT is a desktop app so you need to install it.

https://github.com/fenghuayumo/DIVSHOT/releases

When you have downloaded the installation package, double-click to run it, and you will see the installation interface as shown in the image. ![DIVSHOT](/screenshots/install_div.png?raw=true) 
Then simply click Next through the installation process. During the installation, you can choose the installation path. 

**Note:** After installation is complete, a desktop shortcut icon will appear on your desktop. Double-click it to launch the application. However, on some computers, you may encounter permission access issues that cause the application to crash immediately. In this case, you can run it as administrator, or you can launch it using the command line. Simply navigate to the installation directory and find `diverse_launch.exe`, then type `diverse_launch` in the command line and press Enter to run it. This will open the editor viewer interface. You can also add parameters after `diverse_launch` to run training directly via command line, allowing you to train splats in batch mode. For more details, you can use `diverse_launch --help` to view available options as shown in the image. ![DIVSHOT](/screenshots/lauch.png?raw=true) 
you can input cmd parameters to lauch the cli train app as the image shows: ![DIVSHOT](/screenshots/cli_example.png?raw=true)

## Training Splats

DIVSHOT can training 3d gaussian splats from multiview images.
1. Darg a video file/image folder to windows, Then it will create a GaussianTraining Component
2. Select the `File` > `New GaussianSplat`, then select image file location and camerapose location 
as the image shown: ![DIVSHOT](/screenshots/new_splats.png?raw=true) 

## Loading Splats

DIVSHOT loads splats from .ply files. Only .ply files containing 3D Gaussian Splat data can be loaded. If you attempt to load any other type of data from a .ply file, it will fail.

There are three ways that you can load a .ply file:

1. Drag and drop one or more .ply files from your file system into DIVSHOT's client area.
2. Select the `File` > `Import` menu item and select one or more .ply files from your file system.

## Saving Splats

To save the currently loaded scene, select the `File` > `Export` or `Export SelectSplats` menu items. 

* **ply**: original 3d gaussian splat format
* **compressed ply**: A lightweight, compressed format that is far smaller than the equivalent uncompressed .ply file. It quantizes splat data and drops spherical harmonics from the output file. See [this article](https://blog.playcanvas.com/compressing-gaussian-splats/) for more details on the format.
* **splat File**: Another compressed format, although not as efficient as the compressed ply format.
* **spz File**: scaniverse ply format, See https://github.com/nianticlabs/spz 

## Controlling the Camera

The camera controls in DIVSHOT are as follows:

### Mouse Controls

| Control                                         | Description                     |
| ----------------------------------------------- | ------------------------------- |
| Right Mouse Button                              | Orbit camera around focal point |
| Middle Mouse Button                             | Pan camera (translate view)     |
| Middle Mouse Button + Right Mouse Button        | Pan camera (alternative)        |
| Mouse Scroll Wheel                              | Dolly/Zoom camera               |

### Keyboard Controls

| Control                                         | Description                     |
| ----------------------------------------------- | ------------------------------- |
| W/S Keys                                        | Dolly camera forwards/backwards |
| A/D Keys                                        | Strafe camera left/right        |
| Q Key                                           | Move camera downward            |
| E Key                                           | Move camera upward              |
| F Key                                           | Focus on selected object        |
| Shift Key                                       | Accelerate movement (3x speed)  |

**Note:** The Q/E keys move the camera up and down in world space, making it easy to adjust the camera height. The Shift key works with all camera operations including keyboard movement, mouse panning, and mouse zoom for faster navigation.

## Editing Splats
You can select the sceneview toolbar "Splat", then select the "Edit" submenu item, and the sceneview will display the splat edit toolbar as shown in the figure. ![DIVSHOT](/screenshots/splat_edit.png?raw=true)

You can select the corresponding selection tool, such as rectangle selection, then use the mouse to select splats. The selected Splats will be displayed in yellow. You can then use the Delete key to remove these Splats. You can also use the invert selection tool (in the Edit toolbar menu item), which will invert the selection so that unselected Splats will be shown in yellow, as shown in the figure: ![DIVSHOT](/screenshots/splat_edit1.png?raw=true) This way you can quickly clean up unwanted Splats and achieve very clean results.
## KeyFrame

You can double-click the left mouse button on the KeyFrame timeline to add a keyframe at that time mark. A yellow circle will appear to indicate this is a keyframe. You can also drag the keyframe cursor (blue vertical line) to a specific time mark, then click the "+" or "-" buttons above to add or delete keyframes. Alternatively, you can right-click to open a context menu to add or delete keyframes. You can then use the play and fast-forward controls to preview the keyframe animation. Finally, you can click the Export button at the top to export the keyframe video, as shown in the figure: ![DIVSHOT](/screenshots/keyframe_panel.png?raw=true) 

## Inspector

This panel displays component information for entities in the hierarchy panel. For example, when you select a Gaussian entity, the Inspector panel will show the editable and viewable properties of that GaussianSplats.