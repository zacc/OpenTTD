/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file win32_v.h Base of the Windows video driver. */

#ifndef VIDEO_WIN32_H
#define VIDEO_WIN32_H

#include "video_driver.hpp"
#include <mutex>
#include <condition_variable>

/** Base class for Windows video drivers. */
class VideoDriver_Win32Base : public VideoDriver {
public:
	VideoDriver_Win32Base() : main_wnd(nullptr), fullscreen(false), draw_mutex(nullptr), draw_signal(nullptr) {}

	void Stop() override;

	void MakeDirty(int left, int top, int width, int height) override;

	void MainLoop() override;

	bool ChangeResolution(int w, int h) override;

	bool ToggleFullscreen(bool fullscreen) override;

	void AcquireBlitterLock() override;

	void ReleaseBlitterLock() override;

	bool ClaimMousePointer() override;

	void EditBoxLostFocus() override;

protected:
	HWND main_wnd;          ///< Handle to system window.
	bool fullscreen;        ///< Whether to use (true) fullscreen mode.
	bool has_focus = false; ///< Does our window have system focus?
	Rect dirty_rect;        ///< Region of the screen that needs redrawing.
	int width = 0;          ///< Width in pixels of our display surface.
	int height = 0;         ///< Height in pixels of our display surface.
	int width_org = 0;      ///< Original monitor resolution width, before we changed it.
	int height_org = 0;     ///< Original monitor resolution height, before we changed it.

	bool draw_threaded;          ///< Whether the drawing is/may be done in a separate thread.
	bool buffer_locked;          ///< Video buffer was locked by the main thread.
	volatile bool draw_continue; ///< Should we keep continue drawing?

	std::recursive_mutex *draw_mutex;         ///< Mutex to keep the access to the shared memory controlled.
	std::condition_variable_any *draw_signal; ///< Signal to draw the next frame.

	Dimension GetScreenSize() const override;
	float GetDPIScale() override;
	void InputLoop() override;
	bool LockVideoBuffer() override;
	void UnlockVideoBuffer() override;
	void CheckPaletteAnim() override;

	void Initialize();
	bool MakeWindow(bool full_screen);
	void ClientSizeChanged(int w, int h);

	/** Get screen depth to use for fullscreen mode. */
	virtual uint8 GetFullscreenBpp();
	/** (Re-)create the backing store. */
	virtual bool AllocateBackingStore(int w, int h, bool force = false) = 0;
	/** Get a pointer to the video buffer. */
	virtual void *GetVideoPointer() = 0;
	/** Palette of the window has changed. */
	virtual void PaletteChanged(HWND hWnd) = 0;

private:
	std::unique_lock<std::recursive_mutex> draw_lock;

	static void PaintThreadThunk(VideoDriver_Win32Base *drv);

	friend LRESULT CALLBACK WndProcGdi(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
/** The GDI video driver for windows. */
class VideoDriver_Win32GDI : public VideoDriver_Win32Base {
public:
	VideoDriver_Win32GDI() : dib_sect(nullptr), gdi_palette(nullptr), buffer_bits(nullptr) {}

	const char *Start(const StringList &param) override;

	void Stop() override;

	bool AfterBlitterChange() override;

	const char *GetName() const override { return "win32"; }

protected:
	HBITMAP  dib_sect;      ///< System bitmap object referencing our rendering buffer.
	HPALETTE gdi_palette;   ///< Palette object for 8bpp blitter.
	void     *buffer_bits;  ///< Internal rendering buffer.

	void Paint() override;
	void *GetVideoPointer() override { return this->buffer_bits; }
	void PaintThread() override;

	bool AllocateBackingStore(int w, int h, bool force = false) override;
	void PaletteChanged(HWND hWnd) override;
	void MakePalette();
	void UpdatePalette(HDC dc, uint start, uint count);

#ifdef _DEBUG
public:
	static int RedrawScreenDebug();
#endif
};

/** The factory for Windows' video driver. */
class FVideoDriver_Win32GDI : public DriverFactoryBase {
public:
	FVideoDriver_Win32GDI() : DriverFactoryBase(Driver::DT_VIDEO, 10, "win32", "Win32 GDI Video Driver") {}
	Driver *CreateInstance() const override { return new VideoDriver_Win32GDI(); }
};

#ifdef WITH_OPENGL

/** The OpenGL video driver for windows. */
class VideoDriver_Win32OpenGL : public VideoDriver_Win32Base {
public:
	VideoDriver_Win32OpenGL() : dc(nullptr), gl_rc(nullptr) {}

	const char *Start(const StringList &param) override;

	void Stop() override;

	bool ToggleFullscreen(bool fullscreen) override;

	bool AfterBlitterChange() override;

	const char *GetName() const override { return "win32-opengl"; }

protected:
	HDC   dc;          ///< Window device context.
	HGLRC gl_rc;       ///< OpenGL context.

	uint8 GetFullscreenBpp() override { return 32; } // OpenGL is always 32 bpp.

	void Paint() override;
	void PaintThread() override {}

	bool AllocateBackingStore(int w, int h, bool force = false) override;
	void *GetVideoPointer() override;
	void PaletteChanged(HWND hWnd) override {}

	const char *AllocateContext();
	void DestroyContext();
};

/** The factory for Windows' OpenGL video driver. */
class FVideoDriver_Win32OpenGL : public DriverFactoryBase {
public:
	FVideoDriver_Win32OpenGL() : DriverFactoryBase(Driver::DT_VIDEO, 9, "win32-opengl", "Win32 OpenGL Video Driver") {}
	/* virtual */ Driver *CreateInstance() const override { return new VideoDriver_Win32OpenGL(); }
};

#endif /* WITH_OPENGL */

#endif /* VIDEO_WIN32_H */
