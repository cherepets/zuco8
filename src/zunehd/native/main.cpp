//----------------------------------------------------------------------------------
// File:		tegra_khr_apps_sdk\samples\opengles20\src\win_egl_simple\win_egl_simple.cpp
// SDK Version: 1.0.0 
// Email:	   tegradev@nvidia.com
// Forum:	   http://developer.nvidia.com/tegra/forum
//
// Copyright (c) 2007-2010 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA OR ITS SUPPLIERS
// BE  LIABLE  FOR  ANY  SPECIAL,  INCIDENTAL,  INDIRECT,  OR  CONSEQUENTIAL DAMAGES
// WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS OF BUSINESS PROFITS,
// BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS)
// ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
//
//----------------------------------------------------------------------------------

/*
 * win_egl_simple.cpp
 *
 * Just one triangle, drawn using only WinCE, EGL, and GL ES 2.0
 * 
 */

#include <assert.h>
#include <compclient.h>
#include <float.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <math.h>
#include <stdio.h>
#include <windows.h>
#include <zdkinput.h>
#include <zdkgl.h>
#include <zdksystem.h>

#define FRAGMENT_SHADER_LOCATION "\\gametitle\\584E07D1\\Content\\fragment.nvbf"
#define VERTEX_SHADER_LOCATION   "\\gametitle\\584E07D1\\Content\\vertex.nvbv"

// WinCE SDK doesn't seem to declare PI
const float NV_PI = 3.14159265358979323846f;

// We should use VBOs, but for this tiny example, we go for simplicity over
// efficiency.  But all real rendering apps should use VBOs
#define ROOT_3_OVER_2 0.8660254f
#define ROOT_3_OVER_6 (ROOT_3_OVER_2/3.0f)
static GLfloat s_vert[6] = { 0.5f, -ROOT_3_OVER_6,
						    -0.5f, -ROOT_3_OVER_6,
						     0.0f,  ROOT_3_OVER_2 - ROOT_3_OVER_6 };
static GLfloat s_col [12] = { 1.0, 0.0, 0.0, 1.0,
							  0.0, 1.0, 0.0, 1.0,
							  0.0, 0.0, 1.0, 1.0 };

// Cached handles to the shader program and to the rotation uniform
// inside of the shader program
static GLuint s_programObj = 0;
static GLuint s_rotUniformLocation = 0;
static float s_angle = 0.0f;
static bool s_spin = true;

bool LoadBinaryShader(GLuint shader, const char *path)
{
	FILE *file;
	long length;
	char *buffer;

	// open file
	file = fopen(path, "rb");
	if (!file)
		return false;
	// find length
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);
	// read contents into shader
	buffer = (char *)malloc(length);
	fread(buffer, 1, length, file);
	glShaderBinary(1, &shader, GL_NVIDIA_PLATFORM_BINARY_NV, buffer, length);
	free(buffer);
	// close file
	fclose(file);
	return true;
}

static bool InitGraphicsState(void)
{
	GLuint vsObj, fsObj;

	vsObj = glCreateShader(GL_VERTEX_SHADER);
	fsObj = glCreateShader(GL_FRAGMENT_SHADER);
	s_programObj = glCreateProgram();
	glAttachShader(s_programObj, vsObj);
	glAttachShader(s_programObj, fsObj);

	if (!LoadBinaryShader(vsObj, VERTEX_SHADER_LOCATION))
	{
		printf("\n>>> Couldn't load source vertex shader from file! Exiting...\n\n");
		return false;
	}
	if (!LoadBinaryShader(fsObj, FRAGMENT_SHADER_LOCATION))
	{
		printf("\n>>> Couldn't load source fragment shader from file! Exiting...\n\n");
		return false;
	}

	glBindAttribLocation(s_programObj, 0, "pos_attr");
	glBindAttribLocation(s_programObj, 1, "col_attr");

	glLinkProgram(s_programObj);
	GLint success = 0;
	glGetProgramiv(s_programObj, GL_LINK_STATUS, &success);
	if(!success) 
	{
		printf("\n>>> Couldn't link shader! Exiting...\n\n");
		return false;
	}

	glUseProgram(s_programObj);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, s_vert);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, s_col);

	s_rotUniformLocation = glGetUniformLocation(s_programObj, "rot");

	return true;
}

static void Display(void)
{
	// This is not the safest or best way to handle timing, but this code
	// is only added to make the triangle rotate at a basically constant
	// rate, independent of the target (Win32) platform
	static DWORD lastTime = GetTickCount();
	DWORD newTime = GetTickCount();
	DWORD delta = newTime - lastTime;

	// Handle rollover
	if (newTime < lastTime)
		delta = 0;

	lastTime = newTime;

	if (s_spin)
		s_angle += delta * 0.01f * NV_PI / 15.0f;

	glClear(GL_COLOR_BUFFER_BIT);

	// we only need to pass cos and sin to the shader.
	glUniform2f(s_rotUniformLocation, (GLfloat)cos(s_angle) / 2, (GLfloat)sin(s_angle));

	glDrawArrays(GL_TRIANGLES, 0, 3);

}

void SuppressReboot()
{
	HKEY key = NULL;
	HRESULT hr = S_OK;
	DWORD value;

	if (SUCCEEDED(hr))
		hr = HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Control\\Power\\State\\Reboot", 0, 0, &key));
	if (SUCCEEDED(hr))
		hr = HRESULT_FROM_WIN32(RegSetValueEx(key, L"Flags", 0, REG_DWORD, (BYTE *)&(value = 0x10000), sizeof(DWORD)));
	if (SUCCEEDED(hr))
		hr = HRESULT_FROM_WIN32(RegSetValueEx(key, L"Default", 0, REG_DWORD, (BYTE *)&(value = 0), sizeof(DWORD)));
	if (key)
		RegCloseKey(key);
}

int main(int argc, char *argv[])
{
	ZDK_INPUT_STATE input;

	// suppress reboot
	SuppressReboot();
	// set up opengl es
	ZDKSystem_ShowSplashScreen(false);
	ZDKGL_Initialize();
	// prepare to render
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	if (!InitGraphicsState())
		return 1;
	// loop da shoop
	for (; ;)
	{
		// test if we need to exit
		ZDKInput_GetState(&input);
		if (input.TouchState.Count > 0)
			break;
		// let's do it
		ZDKGL_BeginDraw();
		Display();
		ZDKGL_EndDraw();
	}
	// clean up and exit
	ZDKGL_Cleanup();
	return 0;
}
