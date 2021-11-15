/// \file OculusVROpenGLWidget.h
/// \brief Declare a C++ class for Oculus VR display in a Qt OpenGL widget.
/// \author Stephane DORVAL

#ifndef __OCULUSVROPENGLWIDGET_H__
#define __OCULUSVROPENGLWIDGET_H__

//Include the Oculus SDK
#include "OVR_CAPI_GL.h"
#include "Extras/OVR_Math.h"

#include <QOpenGLWidget>
#include <QOpenGLFunctions_4_5_core>
#include <QTimer>

using namespace OVR;

// Deactivate mirroring with FBO (from Oculus SDK Samples) because it doesn't work in this QOpenGLWidget.
/// \todo Obviously, I missed something. I will retry later.
//#define MIRRORING_WITH_FBO

/// Axis indices
#define _X	0
#define	_Y	1
#define _Z	2

/// Angles indices
#define _YAW	0
#define _PITCH	1
#define _ROLL	2

/// \class OculusVROpenGLWidget
/// \brief Define a widget which renders a scene in the Oculus headset and in the widget on demand.
/// It is mostly inspired by the Oculus Tiny Room (GL) sample from Oculus SDK.
class OculusVROpenGLWidget :
	public QOpenGLWidget, 
	public QOpenGLFunctions_4_5_Core
{
	Q_OBJECT

public:

	/// \class OculusTextureBuffer
	/// \brief Define buffers for eyes textures handling.
	class OVRTexBuffer : public QOpenGLFunctions_4_5_Core
	{
	public:
		/// Running oculus session
		ovrSession m_session;

		/// Color texture chain
		ovrTextureSwapChain m_colorTexChain;

		/// Depth texture chain
		ovrTextureSwapChain m_depthTexChain;

		/// Corresponding frame buffer object ID
		GLuint m_fboId;

		/// Conresponding texture size
		Sizei m_texSize;

		/// Constructor
		/// \param session Running oculus session
		/// \param size Texture size.
		/// \param sampleCount
		OVRTexBuffer(ovrSession session, Sizei size, int sampleCount);

		/// Destructor
		~OVRTexBuffer();

		/// \return Texture size
		Sizei GetSize() const;

		/// Initialize texture rendering
		void SetAndClearRenderSurface();

		/// Clean texture rendering
		void UnsetRenderSurface();

		/// Send texture to oculus device
		void Commit();
	};

private:

	enum TargetRendering {
		Headset,
		Widget
	};

	/// Retrieve the default adapter.
	ovrGraphicsLuid GetDefaultAdapterLuid();

	/// Compare two ovrGraphicsLuid addresses.
	int Compare(const ovrGraphicsLuid& lhs, const ovrGraphicsLuid& rhs);

	/// Parent widget
	QWidget* m_parentWidget;

	/// Eyes textures
	OVRTexBuffer *m_eyeRenderTexture[2];

	/// Index of frame
	long long m_frameIndex;

	/// Current running oculus session
	ovrSession m_session;

	/// Adapter ID
	ovrGraphicsLuid m_luid;

	/// Head mounted display description
	ovrHmdDesc m_hmdDesc;

	/// Window size
	ovrSizei m_windowSize;

	/// Eyes translation vector
	Vector3f m_eyesTranslation;

	/// Eyes rotation angles: yaw, pitch, roll
	Vector3f m_eyesRotations;

	/// Timer
	QTimer m_timer;

	/// Mirroring activation status
	bool m_showInWidget;

	/// Controllers activation
	bool m_enableControllers;

#ifdef MIRRORING_WITH_FBO
	// ////  Mirroring  ////

	/// Mirror texture description
	ovrMirrorTextureDesc m_mirrorDesc;

	/// OVR mirror texture
	ovrMirrorTexture m_mirrorTexture;

	/// OpenGL mirror texture Id
	GLuint m_mirrorTexId;

	/// Mirror FBO Id
	GLuint m_mirrorFBO;
#endif

	/// Initialize the Oculus VR device
	void InitializeOculusVR();

	/// Render the scene in head set or widget
	/// \param sessionStatus The session status
	/// \param i_target Headset or Widget
	void Render(ovrSessionStatus sessionStatus, TargetRendering i_target);

#ifdef MIRRORING_WITH_FBO
	/// Configure buffers for mirroring. 
	/// \note This function must be called in the initializeGL() function.
	void InitializeMirroring();

	/// Method to render the mirror FBO to the screen.
	/// \note This function must be called in the paintGL() function.
	void RenderMirroring();
#endif

public:

	/// Construct the OVR OpenGL widget
	/// \param parent The parent widget
	/// \param showInWidget Mirroring activation.
	/// \param enableControllers Activation of Oculus remote controllers handling.
	/// \param enableControllersRendering Display controllers.
    OculusVROpenGLWidget(
		QWidget *parent = nullptr,
		bool showInWidget = true,
		bool enableControllers = true);

	/// \Destructor
    ~OculusVROpenGLWidget();

	/// Method to initialize a scene rendering.
	/// \note Must be implemented. Called in initializeGL() method.
	virtual void InitializeRendering() = 0;

	/// Method to update the scene (update animations for example)
	/// \note Must be implemented. Called in paintGL() method.
	virtual void UpdateRendering(ovrSessionStatus sessionStatus) = 0;

	/// Method to render the scene.
	/// \param sessionStatus The running Oculus session status
	/// \param eye Gives to which eye to render (left or right)
	/// \param view The model view matrix.
	/// \param projection The projection matrix.
	/// \note Must be implemented. Called in paintGL() method.
	virtual void Render(ovrSessionStatus sessionStatus, ovrEyeType eye, Matrix4f view, Matrix4f projection) = 0;

	/// Send signal of controller state.
	Q_SIGNAL void signalControllerState(ovrInputState i_controlState);

	/// \return The running session.
	ovrSession Session();

	/// \brief	Translate eyes positions by the vector (i_deltaX, i_deltaY, i_deltaZ).
	/// \param	i_deltaX	Translation value on X axis.
	/// \param	i_deltaY	Translation value on Y axis.
	/// \param	i_deltaZ	Translation value on Z axis.
	void TranslateEyes(float i_deltaX, float i_deltaY, float i_deltaZ);

	/// \brief Reset eyes position to (0.0, 0.0, 0.0).
	void ResetEyesPositions();

	/// \return The eyes translation vector.
	Vector3f GetTranslations();

	/// \brief	Rotate eyes by the angles (i_yaw, i_pitch, i_roll).
	/// \param	i_yaw	Rotation angle value on Y axis.
	/// \param	i_pitch	Rotation angle value on X axis.
	/// \param	i_roll	Rotation angle value on Z axis.
	void RotateEyes(float i_yaw, float i_pitch, float i_roll);

	/// \brief Reset eyes rotations to (0.0, 0.0, 0.0).
	void ResetEyesRotaions();

	/// \return The eyes Euler's angles in vector (yaw, pitch, roll).
	Vector3f GetRotations();

protected:

	// From QOpenGLWidget...

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
};

#endif // __OCULUSVROPENGLWIDGET_H__
