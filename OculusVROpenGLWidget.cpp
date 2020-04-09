/// \file OculusVROpenGLWidget.cpp
/// \brief Implémente the C++ class for Oculus VR display in a Qt OpenGL widget declared in OculusVROpenGLWidget.h.
/// \author Stephane DORVAL

#include "OculusVROpenGLWidget.h"

#include <QMessageBox>
#include <QApplication>
#include <QDebug>

#if defined(_WIN32)
#include <dxgi.h> // for GetDefaultAdapterLuid
#pragma comment(lib, "dxgi.lib")
#endif




// ////////////////////////////////////////////////////////////////////////////////////////////////
//
// OCULUS OPENGL WIDGET
// 

OculusVROpenGLWidget::OculusVROpenGLWidget(
	QWidget *parent, 
	bool showInWidget,
	bool enableControllers) :
	QOpenGLWidget(parent),
	m_showInWidget(showInWidget),
	m_frameIndex(0),
	m_parentWidget(parent),
	m_enableControllers(enableControllers),
	m_initialBodyPos(0.0f, 0.0f, -5.0f)
#ifdef	MIRRORING_WITH_FBO
	,m_mirrorTexture(nullptr),
	m_mirrorFBO(0),
	m_mirrorTexId(0)
#endif
{
	InitializeOculusVR();
	resize(m_hmdDesc.Resolution.w, m_hmdDesc.Resolution.h);

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
	m_timer.setInterval(10);
	m_timer.start();
}


OculusVROpenGLWidget::~OculusVROpenGLWidget()
{
	m_timer.stop();
#ifdef	MIRRORING_WITH_FBO
	if (m_mirrorFBO) glDeleteFramebuffers(1, &m_mirrorFBO);
	if (m_mirrorTexture) ovr_DestroyMirrorTexture(m_session, m_mirrorTexture);
	if (m_mirrorTexId) glDeleteTextures(1, &m_mirrorTexId);
#endif
	for (int eye = 0; eye < 2; ++eye)
	{
		delete m_eyeRenderTexture[eye];
	}
	if ( m_session ) ovr_Destroy(m_session);
	ovr_Shutdown();
}


void OculusVROpenGLWidget::InitializeOculusVR()
{
	// Initialize Oculus device
	ovrInitParams initParams = { ovrInit_RequestVersion, OVR_MINOR_VERSION, NULL, 0, 0 };
	ovrResult result = ovr_Initialize(&initParams);
	if (OVR_FAILURE(result)) {
		ovrErrorInfo errorInfo;
		ovr_GetLastErrorInfo(&errorInfo);
		qDebug() << QString("ovr_Initialize failed: %1").arg(errorInfo.ErrorString);
		QMessageBox::critical(this, QApplication::applicationName(), QString("ovr_Initialize failed: %1").arg(errorInfo.ErrorString));
		return;
	}

	m_eyeRenderTexture[0] = m_eyeRenderTexture[1] = nullptr;

	// Create the session
	result = ovr_Create(&m_session, &m_luid);
	if (OVR_FAILURE(result))
	{
		ovrErrorInfo errorInfo;
		ovr_GetLastErrorInfo(&errorInfo);
		qDebug() << QString("ovr_Create failed: %1").arg(errorInfo.ErrorString);
		QMessageBox::critical(this, QApplication::applicationName(), QString("ovr_Create failed: %1").arg(errorInfo.ErrorString));
		return;
	}

	if (Compare(m_luid, GetDefaultAdapterLuid())) // If luid that the Rift is on is not the default adapter LUID...
	{
		qDebug() << "OpenGL supports only the default graphics adapter.";
		QMessageBox::critical(this, QApplication::applicationName(), QString("OpenGL supports only the default graphics adapter."));
	}

	m_hmdDesc = ovr_GetHmdDesc(m_session);

	// Note: the mirror window can be any size, for this sample we use 1/2 the HMD resolution
	m_windowSize = m_hmdDesc.Resolution;
}


int OculusVROpenGLWidget::Compare(const ovrGraphicsLuid& lhs, const ovrGraphicsLuid& rhs)
{
	return memcmp(&lhs, &rhs, sizeof(ovrGraphicsLuid));
}


ovrGraphicsLuid OculusVROpenGLWidget::GetDefaultAdapterLuid()
{
	ovrGraphicsLuid luid = ovrGraphicsLuid();

#if defined(_WIN32)
	IDXGIFactory* factory = nullptr;

	if (SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(&factory))))
	{
		IDXGIAdapter* adapter = nullptr;

		if (SUCCEEDED(factory->EnumAdapters(0, &adapter)))
		{
			DXGI_ADAPTER_DESC desc;

			adapter->GetDesc(&desc);
			memcpy(&luid, &desc.AdapterLuid, sizeof(luid));
			adapter->Release();
		}

		factory->Release();
	}
#endif

	return luid;
}


void OculusVROpenGLWidget::initializeGL()
{
	initializeOpenGLFunctions();

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	// Make eye render buffers
	for (int eye = 0; eye < 2; ++eye)
	{
		ovrSizei idealTextureSize = ovr_GetFovTextureSize(m_session, ovrEyeType(eye), m_hmdDesc.DefaultEyeFov[eye], 1);
		m_eyeRenderTexture[eye] = new OVRTexBuffer(m_session, idealTextureSize, 1);

		if (!m_eyeRenderTexture[eye]->m_colorTexChain || !m_eyeRenderTexture[eye]->m_depthTexChain)
		{
			qDebug() << "Failed to create eyes textures.";
		}
	}

#ifdef MIRRORING_WITH_FBO
	if (m_showInWidget)
		InitializeMirroring();
#endif

	// Turn off vsync to let the compositor do its magic
	context()->format().setSwapInterval(0);

	// FloorLevel will give tracking poses where the floor height is 0
	ovr_SetTrackingOriginType(m_session, ovrTrackingOrigin_FloorLevel);

	InitializeRendering();	
}


void OculusVROpenGLWidget::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);
}


ovrSession OculusVROpenGLWidget::Session()
{
	return m_session;
}

Vector3f OculusVROpenGLWidget::GetInitialBodyPosition()
{
	return m_initialBodyPos;
}

void OculusVROpenGLWidget::Render(ovrSessionStatus sessionStatus, TargetRendering i_target)
{
	// touch
	//double ftiming = ovr_GetPredictedDisplayTime(m_session, 0);
	//ovrTrackingState trackingState = ovr_GetTrackingState(m_session, ftiming, ovrTrue);


	static float Yaw(3.141592f);

	// Call ovr_GetRenderDesc each frame to get the ovrEyeRenderDesc, as the returned values (e.g. HmdToEyePose) may change at runtime.
	ovrEyeRenderDesc eyeRenderDesc[2];
	eyeRenderDesc[0] = ovr_GetRenderDesc(m_session, ovrEye_Left, m_hmdDesc.DefaultEyeFov[0]);
	eyeRenderDesc[1] = ovr_GetRenderDesc(m_session, ovrEye_Right, m_hmdDesc.DefaultEyeFov[1]);

	// Get eye poses, feeding in correct IPD offset
	ovrPosef EyeRenderPose[2];
	ovrPosef HmdToEyePose[2] = { eyeRenderDesc[0].HmdToEyePose,
								 eyeRenderDesc[1].HmdToEyePose };

	double sensorSampleTime;    // sensorSampleTime is fed into the layer later
	long long frameIndex = (i_target == Widget) ? (m_frameIndex - 1) : m_frameIndex;
	ovr_GetEyePoses(m_session, frameIndex, ovrTrue, HmdToEyePose, EyeRenderPose, &sensorSampleTime);

	ovrTimewarpProjectionDesc posTimewarpProjectionDesc = {};

	// Render Scene to Eye Buffers
	for (int eye = 0; eye < 2; ++eye)
	{
		if (i_target == Widget)
		{
			// Set up viewport of widget
			int w = ( (m_parentWidget == nullptr) ? m_windowSize.w : m_parentWidget->width()) / 2;
			int h = (m_parentWidget == nullptr) ? m_windowSize.h : m_parentWidget->height();
			if (eye == 0)
				glViewport(0, 0, w, h);
			else
				glViewport(w, 0, w, h);
		}

		if (i_target == Headset)
		{
			// Switch to eye render target
			m_eyeRenderTexture[eye]->SetAndClearRenderSurface();
		}

		// Get view and projection matrices
		Matrix4f rollPitchYaw = Matrix4f::RotationY(Yaw);
		Matrix4f finalRollPitchYaw = rollPitchYaw * Matrix4f(EyeRenderPose[eye].Orientation);
		Vector3f finalUp = finalRollPitchYaw.Transform(Vector3f(0, 1, 0));
		Vector3f finalForward = finalRollPitchYaw.Transform(Vector3f(0, 0, -1));
		Vector3f shiftedEyePos = m_initialBodyPos + rollPitchYaw.Transform(EyeRenderPose[eye].Position);

		Matrix4f view = Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);
		Matrix4f proj = ovrMatrix4f_Projection(m_hmdDesc.DefaultEyeFov[eye], 0.2f, 1000.0f, ovrProjection_None);
		if (i_target == Headset)
			posTimewarpProjectionDesc = ovrTimewarpProjectionDesc_FromProjection(proj, ovrProjection_None);

		// Render world
		Render(sessionStatus, eye == 0 ? ovrEye_Left : ovrEye_Right, view, proj);

		if (i_target == Widget)
			continue;

		// Avoids an error when calling SetAndClearRenderSurface during next iteration.
		// Without this, during the next while loop iteration SetAndClearRenderSurface
		// would bind a framebuffer with an invalid COLOR_ATTACHMENT0 because the texture ID
		// associated with COLOR_ATTACHMENT0 had been unlocked by calling wglDXUnlockObjectsNV.
		m_eyeRenderTexture[eye]->UnsetRenderSurface();

		// Commit changes to the textures so they get picked up frame
		m_eyeRenderTexture[eye]->Commit();
	}

	if (i_target == Widget)
		return;

	// Do distortion rendering, Present and flush/sync

	ovrLayerEyeFovDepth ld = {};
	ld.Header.Type = ovrLayerType_EyeFovDepth;
	ld.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;   // Because OpenGL.
	ld.ProjectionDesc = posTimewarpProjectionDesc;
	ld.SensorSampleTime = sensorSampleTime;

	for (int eye = 0; eye < 2; ++eye)
	{
		ld.ColorTexture[eye] = m_eyeRenderTexture[eye]->m_colorTexChain;
		ld.DepthTexture[eye] = m_eyeRenderTexture[eye]->m_depthTexChain;
		ld.Viewport[eye] = Recti(m_eyeRenderTexture[eye]->GetSize());
		ld.Fov[eye] = m_hmdDesc.DefaultEyeFov[eye];
		ld.RenderPose[eye] = EyeRenderPose[eye];
	}

	ovrLayerHeader* layers = &ld.Header;
	ovrResult result = ovr_SubmitFrame(m_session, m_frameIndex, nullptr, &layers, 1);
	// exit the rendering loop if submit returns an error, will retry on ovrError_DisplayLost
	if (!OVR_SUCCESS(result))
	{
		ovrErrorInfo errorInfo;
		ovr_GetLastErrorInfo(&errorInfo);
		qDebug() << QString("ovr_SubmitFrame failed: %1").arg(errorInfo.ErrorString);
	}

	m_frameIndex++;
}


void OculusVROpenGLWidget::paintGL()
{
	ovrSessionStatus sessionStatus;
	ovr_GetSessionStatus(m_session, &sessionStatus);
	if (sessionStatus.ShouldQuit)
	{
		m_timer.stop();
		return;
	}

	if (sessionStatus.ShouldRecenter)
		ovr_RecenterTrackingOrigin(m_session);

	if (m_enableControllers)
	{
		ovrInputState inputState;
		ovrResult result = ovr_GetInputState(m_session, ovrControllerType_Touch, &inputState);
		if (!OVR_SUCCESS(result))
		{
			ovrErrorInfo errorInfo;
			ovr_GetLastErrorInfo(&errorInfo);
			qDebug() << QString("ovr_GetInputState for ovrControllerType_Touch failed: %1").arg(errorInfo.ErrorString);
		}
		else
		{
			emit signalControllerState(inputState);
		}
	}

	if (sessionStatus.IsVisible)
	{		
		UpdateRendering(sessionStatus);
		Render(sessionStatus, Headset);
	}

	if (m_showInWidget)
	{
#ifdef MIRRORING_WITH_FBO
		RenderMirroring();
#else
		makeCurrent();
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		Render(sessionStatus, Widget);
		doneCurrent();
#endif
	}
}


#ifdef MIRRORING_WITH_FBO

void OculusVROpenGLWidget::InitializeMirroring()
{
	memset(&m_mirrorDesc, 0, sizeof(m_mirrorDesc));
	m_mirrorDesc.Width = m_windowSize.w;
	m_mirrorDesc.Height = m_windowSize.h;
	m_mirrorDesc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;

	// Create mirror texture and an FBO used to copy mirror texture to back buffer
	ovrResult result = ovr_CreateMirrorTextureWithOptionsGL(m_session, &m_mirrorDesc, &m_mirrorTexture);
	if (!OVR_SUCCESS(result))
	{
		ovrErrorInfo errorInfo;
		ovr_GetLastErrorInfo(&errorInfo);
		qDebug() << QString("ovr_SubmitFrame failed: %1").arg(errorInfo.ErrorString);
	}

	// Configure the mirror read buffer
	ovr_GetMirrorTextureBufferGL(m_session, m_mirrorTexture, &m_mirrorTexId);

	glGenFramebuffers(1, &m_mirrorFBO);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_mirrorFBO);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_mirrorTexId, 0);
	glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}


void OculusVROpenGLWidget::RenderMirroring()
{
	// Blit mirror texture to back buffer
	glBindFramebuffer(GL_READ_FRAMEBUFFER, m_mirrorFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	GLint w = m_windowSize.w;
	GLint h = m_windowSize.h;
	glBlitFramebuffer(0, h, w, 0,
		0, 0, w, h,
		GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

#endif






// ////////////////////////////////////////////////////////////////////////////////////////////////
//
// OCULUS TEXTURES
// 

OculusVROpenGLWidget::OVRTexBuffer::OVRTexBuffer(ovrSession session, Sizei size, int sampleCount) :
	m_session(session),
	m_colorTexChain(nullptr),
	m_depthTexChain(nullptr),
	m_fboId(0),
	m_texSize(0, 0)
{
	initializeOpenGLFunctions();
	assert(sampleCount <= 1); // The code doesn't currently handle MSAA textures.

	m_texSize = size;

	// This texture isn't necessarily going to be a rendertarget, but it usually is.
	assert(session); // No HMD? A little odd.

	ovrTextureSwapChainDesc desc = {};
	desc.Type = ovrTexture_2D;
	desc.ArraySize = 1;
	desc.Width = size.w;
	desc.Height = size.h;
	desc.MipLevels = 1;
	desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.SampleCount = sampleCount;
	desc.StaticImage = ovrFalse;

	{
		ovrResult result = ovr_CreateTextureSwapChainGL(m_session, &desc, &m_colorTexChain);

		int length = 0;
		ovr_GetTextureSwapChainLength(session, m_colorTexChain, &length);

		if (OVR_SUCCESS(result))
		{
			for (int i = 0; i < length; ++i)
			{
				GLuint chainTexId;
				ovr_GetTextureSwapChainBufferGL(m_session, m_colorTexChain, i, &chainTexId);
				glBindTexture(GL_TEXTURE_2D, chainTexId);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
		}
	};

	desc.Format = OVR_FORMAT_D32_FLOAT;

	{
		ovrResult result = ovr_CreateTextureSwapChainGL(m_session, &desc, &m_depthTexChain);

		int length = 0;
		ovr_GetTextureSwapChainLength(session, m_depthTexChain, &length);

		if (OVR_SUCCESS(result))
		{
			for (int i = 0; i < length; ++i)
			{
				GLuint chainTexId;
				ovr_GetTextureSwapChainBufferGL(m_session, m_depthTexChain, i, &chainTexId);
				glBindTexture(GL_TEXTURE_2D, chainTexId);

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
		}
	}

	glGenFramebuffers(1, &m_fboId);
}

OculusVROpenGLWidget::OVRTexBuffer::~OVRTexBuffer()
{
	if (m_colorTexChain)
	{
		ovr_DestroyTextureSwapChain(m_session, m_colorTexChain);
		m_colorTexChain = nullptr;
	}
	if (m_depthTexChain)
	{
		ovr_DestroyTextureSwapChain(m_session, m_depthTexChain);
		m_depthTexChain = nullptr;
	}
	if (m_fboId)
	{
		glDeleteFramebuffers(1, &m_fboId);
		m_fboId = 0;
	}
};

Sizei OculusVROpenGLWidget::OVRTexBuffer::GetSize() const
{
	return m_texSize;
};

void OculusVROpenGLWidget::OVRTexBuffer::SetAndClearRenderSurface()
{
	GLuint curColorTexId;
	GLuint curDepthTexId;
	{
		int curIndex;
		ovr_GetTextureSwapChainCurrentIndex(m_session, m_colorTexChain, &curIndex);
		ovr_GetTextureSwapChainBufferGL(m_session, m_colorTexChain, curIndex, &curColorTexId);
	}
	{
		int curIndex;
		ovr_GetTextureSwapChainCurrentIndex(m_session, m_depthTexChain, &curIndex);
		ovr_GetTextureSwapChainBufferGL(m_session, m_depthTexChain, curIndex, &curDepthTexId);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, m_fboId);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, curColorTexId, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, curDepthTexId, 0);

	glViewport(0, 0, m_texSize.w, m_texSize.h);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_FRAMEBUFFER_SRGB);
};

void OculusVROpenGLWidget::OVRTexBuffer::UnsetRenderSurface()
{
	glBindFramebuffer(GL_FRAMEBUFFER, m_fboId);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
};

void OculusVROpenGLWidget::OVRTexBuffer::Commit()
{
	ovr_CommitTextureSwapChain(m_session, m_colorTexChain);
	ovr_CommitTextureSwapChain(m_session, m_depthTexChain);
};