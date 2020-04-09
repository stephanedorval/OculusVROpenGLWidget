# OculusVROpenGLWidget

## Description
A Qt OpenGL widget for Oculus VR.

## Installation
Copy the OculusVROpenGLWidget C++ class source files to your projet.
* OculusVROpenGLWidget.h
* OculusVROpenGLWidget.cpp

## Dependencies
OculusVROpenGLWidget class depends on:
* Qt framework >= 5.5
* Oculus SDK >= 1.43.0 (not tested with previous versions)
   
## Usage
Define your own widget which inherits from OculusVROpenGLWidget. Then, there are 3 methods
to implement:
* **InitializeRendering()** which is called in the initializeGL() method of QOpenGLWidget.
Here you should initialize your own scene.
* **UpdateRendering(...)** which is called int the paintGL() method of QOpenGLWidget before
scene rendering to each eye. Here you should update the animated parts of the scene relative
to both eyes.
* **Render(...)** which is called in the paintGL() method of QOpenGLWidget.
Here you should render your scene. It is called for each eye.

The controllers actions are notified by the signal **signalControllerState**.

Controllers actions and mirroring to the window can be deactivated at build time thanks to
constructor parameters.

## Licence
This OculusVROpenGLWidget C++ class is licensed with the GNU GPLv3 licence.  
See LICENCE file.