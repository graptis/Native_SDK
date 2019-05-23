/*!*********************************************************************************************************************
\File         OGLESIMGTextureFilterCubic.cpp
\Title        Introducing the POD 3D file format
\Author       PowerVR by Imagination, Developer Technology Team
\Copyright    Copyright (c) Imagination Technologies Limited.
\brief        Shows how to use IMG_texture_filter_cubic extension
***********************************************************************************************************************/
//Main include file for the PVRShell library. Use this file when you will not be linking the PVRApi library.
#include "PVRShell/PVRShell.h"
//Main include file for the PVRAssets library.
#include "PVRAssets/PVRAssets.h"
//The OpenGL ES bindings used throughout this SDK. Use by calling the ES functions from the gl::namespace.
//The functions are loaded dynamically at first call and require no linking in the application.
// (So, glTextImage2D becomes gl::TexImage2D)
#include "PVRUtils/PVRUtilsGles.h"

// Index to bind the attributes to vertex shaders
const uint32_t VertexArray = 0;

//Shader files
const char FragShaderSrcFile[] = "FragShader.fsh";
const char VertShaderSrcFile[] = "VertShader.vsh";

/*!*********************************************************************************************************************
\brief Class implementing the pvr::Shell functions.
***********************************************************************************************************************/
class OGLESIMGTextureFilterCubic : public pvr::Shell
{
	pvr::EglContext _context;

	std::vector<glm::vec3> _vertices;

	GLuint _quadVbo;
	GLuint _tex, _cubicTex;

	// Group shader programs and their uniform locations together
	struct
	{
		GLuint handle;
		uint32_t uiMVPMatrixLoc;
		uint32_t uiWidthLoc;
	} _ShaderProgram;

	// Variables to handle the animation in a time-based manner
	float _frame;
	glm::mat4 _projection;
	
	// UIRenderer class used to display text
	pvr::ui::UIRenderer _uiRenderer;
	
public:
	//pvr::Shell implementation.
	virtual pvr::Result initApplication();
	virtual pvr::Result initView();
	virtual pvr::Result releaseView();
	virtual pvr::Result quitApplication();
	virtual pvr::Result renderFrame();

	bool loadShaders();
	bool LoadVbos();
};

/*!*********************************************************************************************************************
\return Result::Success if no error occurred
\brief  Code in initApplication() will be called by Shell once per run, before the rendering context is created.
Used to initialize variables that are not dependent on it (e.g. external modules, loading meshes, etc.).
If the rendering context is lost, InitApplication() will not be called again.
***********************************************************************************************************************/
pvr::Result OGLESIMGTextureFilterCubic::initApplication()
{
	// Initialize variables used for the animation
	_frame = 0;
	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Result::Success if no error occurred
\brief  Code in initView() will be called by Shell upon initialization or after a change in the rendering context.
    Used to initialize variables that are dependent on the rendering context (e.g. textures, vertex buffers, etc.)
***********************************************************************************************************************/
pvr::Result OGLESIMGTextureFilterCubic::initView()
{
	_context = pvr::createEglContext();
	_context->init(getWindow(), getDisplay(), getDisplayAttributes(), pvr::Api::OpenGLES2);

	bool isCubicSupported = gl::isGlExtensionSupported("GL_IMG_texture_filter_cubic");

	if (!isCubicSupported)
	{
		this->setExitMessage("Extension 'GL_IMG_texture_filter_cubic' is not supported on your platform.");
		return pvr::Result::UnknownError;
	}
	
	_quadVbo = 0;

	if (!LoadVbos())
	{
		this->setExitMessage("Unable to load vbos");
		return pvr::Result::UnknownError;
	}

	//	Load and compile the shaders & link programs
	if (!loadShaders())
	{
		this->setExitMessage("Unable to load shaders");
		return pvr::Result::UnknownError;
	}
	
	if (!_uiRenderer.init(getWidth(), getHeight(), isFullScreen(), _context->getApiVersion() == pvr::Api::OpenGLES2))
	{
		setExitMessage("Error: Failed to initialize the UIRenderer\n");
		return pvr::Result::NotInitialized;
	}

	_uiRenderer.getDefaultTitle()->setText("IMGTextureFilterCubic");
	_uiRenderer.getDefaultTitle()->commitUpdates();
	_uiRenderer.getDefaultControls()->setText("Left: Bilinear Filtering.\nRight: Cubic Filtering.");
	_uiRenderer.getDefaultControls()->commitUpdates();

	//  Set OpenGL ES render states needed for this training course
	// Enable backface culling and depth test
	gl::CullFace(GL_BACK);
	gl::Enable(GL_CULL_FACE);
	gl::Enable(GL_DEPTH_TEST);

	// Use a nice bright blue as clear color
	gl::ClearColor(0.00, 0.70, 0.67, 1.0f);

	_projection = glm::perspectiveFov(45.0f, (float)this->getWidth(), (float)this->getHeight(), 0.1f, 250.0f);

	std::vector<unsigned char> img(this->getWidth() * this->getHeight() * 4);

	int size = 2;
	int half = size / 2;

	for (int y = 0; y < this->getHeight(); ++y)
	{
		for (int x = 0; x < this->getWidth(); ++x)
		{
			if (x % size < half && y % size < half)
			{
				img[(x + y * this->getWidth()) * 4 + 0] = 255;
				img[(x + y * this->getWidth()) * 4 + 1] = 0;
				img[(x + y * this->getWidth()) * 4 + 2] = 0;
				img[(x + y * this->getWidth()) * 4 + 3] = 255;
			}
			else if (x % size >= half && y % size < half)
			{
				img[(x + y * this->getWidth()) * 4 + 0] = 0;
				img[(x + y * this->getWidth()) * 4 + 1] = 255;
				img[(x + y * this->getWidth()) * 4 + 2] = 0;
				img[(x + y * this->getWidth()) * 4 + 3] = 255;
			}
			else if (x % size < half && y % size >= half)
			{
				img[(x + y * this->getWidth()) * 4 + 0] = 0;
				img[(x + y * this->getWidth()) * 4 + 1] = 0;
				img[(x + y * this->getWidth()) * 4 + 2] = 255;
				img[(x + y * this->getWidth()) * 4 + 3] = 255;
			}
			else
			{
				img[(x + y * this->getWidth()) * 4 + 0] = 255;
				img[(x + y * this->getWidth()) * 4 + 1] = 0;
				img[(x + y * this->getWidth()) * 4 + 2] = 255;
				img[(x + y * this->getWidth()) * 4 + 3] = 255;
			}
		}
	}

	gl::GenTextures(1, &_cubicTex);
	gl::BindTexture(GL_TEXTURE_2D, _cubicTex);
	gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_CUBIC_IMG);
	gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, this->getWidth(), this->getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
	gl::GenerateMipmap(GL_TEXTURE_2D);

	gl::GenTextures(1, &_tex);
	gl::BindTexture(GL_TEXTURE_2D, _tex);
	gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	gl::TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gl::TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, this->getWidth(), this->getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
	gl::GenerateMipmap(GL_TEXTURE_2D);

	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Result::Success if no error occurred
\brief  Main rendering loop function of the program. The shell will call this function every _frame.
***********************************************************************************************************************/
pvr::Result OGLESIMGTextureFilterCubic::renderFrame()
{
	// Set up the view and _projection matrices from the camera
	glm::mat4 mView;

	gl::Disable(GL_CULL_FACE);
	gl::Disable(GL_DEPTH_TEST);
	gl::Enable(GL_BLEND);

	// We can build the model view matrix from the camera position, target and an up vector.
	// For this we use glm::lookAt()
	mView = glm::lookAt(glm::vec3(0.0, 0.1, 1.0), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));
	glm::mat4 vp = _projection * mView;

	//////////////////////////////////////////////////////
	//Blit to screen

	gl::Viewport(0, 0, this->getWidth(), this->getHeight());
	gl::BindFramebuffer(GL_FRAMEBUFFER, _context->getOnScreenFbo());
	gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Use shader program
	gl::UseProgram(_ShaderProgram.handle);
	gl::UniformMatrix4fv(_ShaderProgram.uiMVPMatrixLoc, 1, GL_FALSE, glm::value_ptr(vp));

	gl::ActiveTexture(GL_TEXTURE0);
	gl::BindTexture(GL_TEXTURE_2D, _tex);
	gl::ActiveTexture(GL_TEXTURE1);
	gl::BindTexture(GL_TEXTURE_2D, _cubicTex);

	gl::EnableVertexAttribArray(VertexArray);
	gl::BindBuffer(GL_ARRAY_BUFFER, _quadVbo);
	gl::VertexAttribPointer(VertexArray, 3, GL_FLOAT, GL_FALSE, 0, 0);
	gl::DrawArrays(GL_TRIANGLES, 0, 6);

	gl::DisableVertexAttribArray(VertexArray);
	gl::BindBuffer(GL_ARRAY_BUFFER, 0);
	
	//UIRENDERER
	{
		// record the commands
		_uiRenderer.beginRendering();

		_uiRenderer.getSdkLogo()->render();
		_uiRenderer.getDefaultTitle()->render();
		_uiRenderer.getDefaultControls()->render();
		_uiRenderer.getDefaultDescription()->render();
		_uiRenderer.endRendering();
	}
	
	if (this->shouldTakeScreenshot())
	{
		pvr::utils::takeScreenshot(this->getScreenshotFileName(), this->getWidth(), this->getHeight());
	}

	_context->swapBuffers();

	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Result::Success if no error occurred
\brief  Code in releaseView() will be called by PVRShell when the application quits or before a change in the rendering context.
***********************************************************************************************************************/
pvr::Result OGLESIMGTextureFilterCubic::releaseView()
{
	_uiRenderer.release();
	
	gl::DeleteBuffers(1, &_quadVbo);

	gl::DeleteProgram(_ShaderProgram.handle);

	return pvr::Result::Success;
}

/*!*********************************************************************************************************************
\return Result::Success if no error occurred
\brief  Code in quitApplication() will be called by Shell once per run, just before exiting the program.
If the rendering context is lost, quitApplication() will not be called.
***********************************************************************************************************************/
pvr::Result OGLESIMGTextureFilterCubic::quitApplication() { return pvr::Result::Success; }

/*!*********************************************************************************************************************
\brief  Loads the mesh data required for this training course into vertex buffer objects
\return Return true if no error occurred
***********************************************************************************************************************/
bool OGLESIMGTextureFilterCubic::LoadVbos()
{
	{
		_vertices.clear();
		_vertices.reserve(6);

		_vertices.push_back(glm::vec3(-10.0f, 0.0f, -5.0f));
		_vertices.push_back(glm::vec3(10.0f, 0.0f, -5.0f));
		_vertices.push_back(glm::vec3(-10.0f, 0.0f, 5.0f));

		_vertices.push_back(glm::vec3(-10.0f, 0.0f, 5.0f));
		_vertices.push_back(glm::vec3(10.0f, 0.0f, -5.0f));
		_vertices.push_back(glm::vec3(10.0f, 0.0f, 5.0f));

		if (_quadVbo)
		{
			gl::DeleteBuffers(1, &_quadVbo);
			_quadVbo = 0;
		}

		gl::GenBuffers(1, &_quadVbo);
		gl::BindBuffer(GL_ARRAY_BUFFER, _quadVbo);
		gl::BufferData(GL_ARRAY_BUFFER, _vertices.size() * sizeof(glm::vec3), _vertices.data(), GL_STATIC_DRAW);
	}

	gl::BindBuffer(GL_ARRAY_BUFFER, 0);

	return true;
}

/*!*********************************************************************************************************************
\brief  Loads and compiles the shaders and links the shader programs required for this training course
\return Return true if no error occurred
***********************************************************************************************************************/
bool OGLESIMGTextureFilterCubic::loadShaders()
{
	// Load and compile the shaders from files.
	const char* attributes[] = { "inVertex" };
	const uint16_t attributeIndices[] = { 0 };

	if (!(_ShaderProgram.handle = pvr::utils::createShaderProgram(*this, VertShaderSrcFile, FragShaderSrcFile, attributes, attributeIndices, 1)))
	{
		setExitMessage("Unable to create default program (%s, %s)", VertShaderSrcFile, FragShaderSrcFile);
		return false;
	}

	gl::UseProgram(_ShaderProgram.handle);
	// Store the location of uniforms for later use
	_ShaderProgram.uiMVPMatrixLoc = gl::GetUniformLocation(_ShaderProgram.handle, "MVPMatrix");
	_ShaderProgram.uiWidthLoc = gl::GetUniformLocation(_ShaderProgram.handle, "WindowWidth");

	gl::Uniform1f(_ShaderProgram.uiWidthLoc, this->getWidth());
	
	gl::Uniform1i(gl::GetUniformLocation(_ShaderProgram.handle, "tex"), 0);
	gl::Uniform1i(gl::GetUniformLocation(_ShaderProgram.handle, "cubicTex"), 1);

	return true;
}

/*!*********************************************************************************************************************
\return auto ptr to the demo supplied by the user
\brief  This function must be implemented by the user of the shell.
    The user should return its Shell object defining the behaviour of the application.
***********************************************************************************************************************/
std::unique_ptr<pvr::Shell> pvr::newDemo() {  return std::unique_ptr<pvr::Shell>(new OGLESIMGTextureFilterCubic()); }
